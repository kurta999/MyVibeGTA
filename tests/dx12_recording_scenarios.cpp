#include "../src/dx12_backend.h"
#include "../src/cpu_jobs.h"
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>

void dx12RecordingScenarios(dx12::Device& device,dx12::Context& context){
    auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    const char* shader=R"(
cbuffer Draw:register(b0){float4 color;float4 params;}
Texture2D<float4> input:register(t0);
float4 VS(uint id:SV_VertexID,uint instance:SV_InstanceID):SV_POSITION{return float4(id==1?3:-1,id==2?3:-1,params.x+instance*.0001,1);}
float4 PS():SV_TARGET{return color;}
float4 Copy():SV_TARGET{return input.Load(int3(8,8,0));}
)";
    dx12::Shader *vs=nullptr,*ps=nullptr,*copy=nullptr;
    auto compile=[&](const char* entry,const char* profile,dx12::Shader** out){dx12::ComPtr<ID3DBlob> code,error;
        auto hr=D3DCompile(shader,std::strlen(shader),"recording",nullptr,nullptr,entry,profile,0,0,&code,&error);
        if(FAILED(hr)&&error)std::fprintf(stderr,"%s\n",static_cast<const char*>(error->GetBufferPointer()));check(SUCCEEDED(hr),"recording shader compile");
        check(SUCCEEDED(device.CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,out)),"recording shader create");};
    compile("VS","vs_5_0",&vs);compile("PS","ps_5_0",&ps);compile("Copy","ps_5_0",&copy);
    dx12::TextureDesc desc{};desc.Width=desc.Height=16;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=dx12::RenderTarget|dx12::ShaderInput;
    dx12::Resource *color=nullptr,*final=nullptr,*depth=nullptr;dx12::View *colorTarget=nullptr,*finalTarget=nullptr,*colorView=nullptr,*depthTarget=nullptr;
    check(SUCCEEDED(device.CreateTexture2D(&desc,nullptr,&color))&&SUCCEEDED(device.CreateTexture2D(&desc,nullptr,&final)),"recording color targets");
    check(SUCCEEDED(device.CreateRenderTargetView(color,nullptr,&colorTarget))&&SUCCEEDED(device.CreateRenderTargetView(final,nullptr,&finalTarget))&&SUCCEEDED(device.CreateShaderResourceView(color,nullptr,&colorView)),"recording color views");
    desc.Format=DXGI_FORMAT_R32_TYPELESS;desc.BindFlags=dx12::DepthTarget;check(SUCCEEDED(device.CreateTexture2D(&desc,nullptr,&depth)),"recording depth");
    D3D12_DEPTH_STENCIL_VIEW_DESC dd{};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;check(SUCCEEDED(device.CreateDepthStencilView(depth,&dd,&depthTarget)),"recording depth view");
    dx12::BufferDesc bd{};bd.ByteWidth=32;bd.BindFlags=dx12::Constant;dx12::Buffer* constants=nullptr;check(SUCCEEDED(device.CreateBuffer(&bd,nullptr,&constants)),"recording constants");
    unsigned indices[]={0,1,2};bd.ByteWidth=sizeof(indices);bd.BindFlags=dx12::Index;dx12::InitialData initial{indices};dx12::Buffer* index=nullptr;check(SUCCEEDED(device.CreateBuffer(&bd,&initial,&index)),"recording index buffer");
    D3D12_RASTERIZER_DESC rd{};rd.FillMode=D3D12_FILL_MODE_SOLID;rd.CullMode=D3D12_CULL_MODE_NONE;rd.DepthClipEnable=TRUE;dx12::RasterizerState* raster=nullptr;check(SUCCEEDED(device.CreateRasterizerState(&rd,&raster)),"recording raster");
    std::array<dx12::Resource*,24> pixels{},depths{};cpu::Pool pool(4);
    const auto previousStats=device.recordingStats;
    for(unsigned frame=0;frame<12;++frame)for(unsigned parallel=0;parallel<2;++parallel){
        unsigned slot=frame*2+parallel;
        context.ClearState();context.VSSetShader(vs,nullptr,0);context.HSSetShader(nullptr,nullptr,0);context.DSSetShader(nullptr,nullptr,0);
        context.IASetInputLayout(nullptr);context.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context.RSSetState(raster);context.OMSetDepthStencilState(nullptr,0);context.OMSetBlendState(nullptr,nullptr,UINT_MAX);
        context.VSSetConstantBuffers(0,1,&constants);context.IASetIndexBuffer(index,DXGI_FORMAT_R32_UINT,0);
        D3D12_VIEWPORT viewport{0,0,16,16,0,1};context.RSSetViewports(1,&viewport);
        for(int pass=0;pass<2;++pass){
            context.OMSetRenderTargets(pass?1:0,pass?&colorTarget:nullptr,depthTarget);
            context.ClearDepthStencilView(depthTarget,D3D12_CLEAR_FLAG_DEPTH,1,0);context.PSSetShader(pass?ps:nullptr,nullptr,0);
            if(parallel)context.beginParallel(&pool);
            for(unsigned draw=0;draw<512;++draw){
                float data[]={float((draw+frame)%11)/11,float((draw+frame)%7)/7,float((draw+frame)%5)/5,1,.1f+float(draw%7)*.03f,0,0,0};
                context.UpdateSubresource(constants,0,nullptr,data,0,0);
                if(draw%2)context.DrawIndexedInstanced(3,2,0,0,0);else context.DrawInstanced(3,2,0,0);
            }
            if(parallel&&pass&&frame==5){
                context.ClearState();
                check(context.recordPool==nullptr,"ClearState must finish the captured batch before resources can change roles");
                context.VSSetConstantBuffers(0,1,&constants);
            }else context.endParallel();
            if(!pass){auto read=depth->texture;read.Usage=dx12::Readback;read.BindFlags=0;check(SUCCEEDED(device.CreateTexture2D(&read,nullptr,&depths[slot])),"recording shadow readback");context.CopyResource(depths[slot],depth);}
        }
        if(parallel){
            // Native OM commands may still be recording. A released wrapper
            // must not make its CPU descriptor available to another target.
            auto oldHandle=colorTarget->handle.ptr;colorTarget->Release();colorTarget=nullptr;
            check(SUCCEEDED(device.CreateRenderTargetView(color,nullptr,&colorTarget)),"recording replacement target");
            check(colorTarget->handle.ptr!=oldHandle,"worker RTV descriptor must remain leased until recording joins");
        }
        // A following pass samples the worker-written render target. Keeping
        // barriers and all list segments in submission order is essential.
        context.OMSetRenderTargets(1,&finalTarget,nullptr);context.PSSetShader(copy,nullptr,0);context.PSSetShaderResources(0,1,&colorView);
        context.Draw(3,0);dx12::View* empty=nullptr;context.PSSetShaderResources(0,1,&empty);
        auto read=final->texture;read.Usage=dx12::Readback;read.BindFlags=0;check(SUCCEEDED(device.CreateTexture2D(&read,nullptr,&pixels[slot])),"recording color readback");
        context.CopyResource(pixels[slot],final);device.submit(false);
    }
    for(unsigned frame=0;frame<12;++frame)for(auto* collection:{&pixels,&depths}){
        dx12::MappedData reference{},actual{};auto* a=(*collection)[frame*2];auto* b=(*collection)[frame*2+1];
        check(SUCCEEDED(context.Map(a,0,dx12::MapRead,0,&reference))&&SUCCEEDED(context.Map(b,0,dx12::MapRead,0,&actual)),"recording comparison map");
        for(unsigned row=0;row<16;++row)check(std::memcmp(static_cast<unsigned char*>(reference.pData)+row*reference.RowPitch,static_cast<unsigned char*>(actual.pData)+row*actual.RowPitch,16*4)==0,"serial/worker pixel or shadow mismatch");
        if(collection==&depths)check(*static_cast<float*>(actual.pData)>.09f&&*static_cast<float*>(actual.pData)<.11f,"shadow geometry must be rendered");
        context.Unmap(a,0);context.Unmap(b,0);a->Release();b->Release();
    }
    check(device.recordingStats.draws-previousStats.draws==12*1024&&device.recordingStats.lists-previousStats.lists==12*8,"shadow and opaque draws must execute through four worker lists each");
    context.ClearState();context.VSSetShader(nullptr,nullptr,0);context.PSSetShader(nullptr,nullptr,0);context.RSSetState(nullptr);
    colorTarget->Release();finalTarget->Release();colorView->Release();depthTarget->Release();color->Release();final->Release();depth->Release();constants->Release();index->Release();raster->Release();vs->Release();ps->Release();copy->Release();
    std::printf("PASS parallel native recording: 12 frame pairs, exact color/depth pixels, indexed/instanced draws, target-to-SRV dependencies, RTV leases, allocator recycling; peak %u recorders\n",device.recordingStats.peakActive);
}
