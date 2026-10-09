#include "../src/dx12_backend.h"
#include <d3dcompiler.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

void dx12StateScenarios(dx12::Device& device,dx12::Context& context){
    auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    const char* shader=R"(
cbuffer Color:register(b0){float4 color;}
float4 VS(uint id:SV_VertexID):SV_POSITION{return float4(id==1?3:-1,id==2?3:-1,0,1);}
float4 PS():SV_TARGET{return color;}
float4 Alt():SV_TARGET{return color.bgra;}
)";
    dx12::Shader *vs=nullptr,*ps=nullptr,*alt=nullptr;
    auto compile=[&](const char* entry,const char* profile,dx12::Shader** out){dx12::ComPtr<ID3DBlob> code;
        check(SUCCEEDED(D3DCompile(shader,std::strlen(shader),"state",nullptr,nullptr,entry,profile,0,0,&code,nullptr)),"state shader compile");
        check(SUCCEEDED(device.CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,out)),"state shader create");};
    compile("VS","vs_5_0",&vs);compile("PS","ps_5_0",&ps);compile("Alt","ps_5_0",&alt);
    dx12::TextureDesc desc{};desc.Width=desc.Height=8;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=dx12::RenderTarget;
    dx12::Resource *target=nullptr,*readback=nullptr;dx12::View* view=nullptr;
    check(SUCCEEDED(device.CreateTexture2D(&desc,nullptr,&target)),"state target");check(SUCCEEDED(device.CreateRenderTargetView(target,nullptr,&view)),"state target view");
    desc.BindFlags=0;desc.Usage=dx12::Readback;check(SUCCEEDED(device.CreateTexture2D(&desc,nullptr,&readback)),"state readback");
    dx12::BufferDesc bd{};bd.ByteWidth=16;bd.BindFlags=dx12::Constant;dx12::Buffer* constants=nullptr;check(SUCCEEDED(device.CreateBuffer(&bd,nullptr,&constants)),"state constants");
    context.ClearState();context.VSSetShader(vs,nullptr,0);context.PSSetShader(ps,nullptr,0);context.HSSetShader(nullptr,nullptr,0);context.DSSetShader(nullptr,nullptr,0);
    D3D12_RASTERIZER_DESC rasterDesc{};rasterDesc.FillMode=D3D12_FILL_MODE_SOLID;rasterDesc.CullMode=D3D12_CULL_MODE_NONE;rasterDesc.DepthClipEnable=TRUE;
    dx12::RasterizerState* raster=nullptr;check(SUCCEEDED(device.CreateRasterizerState(&rasterDesc,&raster)),"state raster");
    context.IASetInputLayout(nullptr);context.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context.RSSetState(raster);context.OMSetDepthStencilState(nullptr,0);context.OMSetBlendState(nullptr,nullptr,UINT_MAX);
    D3D12_VIEWPORT viewport{0,0,8,8,0,1};context.RSSetViewports(1,&viewport);context.OMSetRenderTargets(1,&view,nullptr);context.PSSetConstantBuffers(0,1,&constants);
    for(int frame=0;frame<6;++frame){
        float color[]={.2f+frame*.1f,.3f,.8f-frame*.1f,1};context.UpdateSubresource(constants,0,nullptr,color,0,0);
        context.PSSetShader(frame%2?alt:ps,nullptr,0);
        context.Draw(3,0);auto before=context.bindStats;
        for(int repeat=0;repeat<20;++repeat)context.Draw(3,0);
        check(context.bindStats.pipelineLookups==before.pipelineLookups&&context.bindStats.pipelineBinds==before.pipelineBinds&&context.bindStats.heapBinds==before.heapBinds&&context.bindStats.constantBinds==before.constantBinds,"identical draws must reuse native state");
        // Remap the same buffer object; GPU addresses change and must rebind.
        color[1]=.65f;context.UpdateSubresource(constants,0,nullptr,color,0,0);context.Draw(3,0);
        check(context.bindStats.constantBinds==before.constantBinds+1,"remapped constant address must bind");
        // Simulate an external SDK recorder changing both root and pipeline.
        device.commands->SetGraphicsRootSignature(nullptr);context.invalidateBindings();context.Draw(3,0);
        // CPU RTV handles can be recycled in the same recording. Identity,
        // rather than the handle alone, must invalidate the output binding.
        auto recycledHandle=view->handle.ptr;view->Release();target->Release();
        auto targetDesc=desc;targetDesc.Usage=dx12::Default;targetDesc.BindFlags=dx12::RenderTarget;
        check(SUCCEEDED(device.CreateTexture2D(&targetDesc,nullptr,&target)),"replacement state target");
        check(SUCCEEDED(device.CreateRenderTargetView(target,nullptr,&view))&&view->handle.ptr==recycledHandle,"recycled RTV handle fixture");
        const float clear[4]{};context.ClearRenderTargetView(view,clear);context.OMSetRenderTargets(1,&view,nullptr);context.Draw(3,0);
        context.CopyResource(readback,target);device.submit(false);
        dx12::MappedData mapped{};check(SUCCEEDED(context.Map(readback,0,dx12::MapRead,0,&mapped)),"state map");
        auto* pixel=static_cast<unsigned char*>(mapped.pData)+4*mapped.RowPitch+16;
        for(int c=0;c<4;++c){int source=frame%2&&c<3?2-c:c;check(std::abs(pixel[c]/255.0f-color[source])<.006f,"cached draw state pixel mismatch");}
        context.Unmap(readback,0);
    }
    context.ClearState();context.VSSetShader(nullptr,nullptr,0);context.PSSetShader(nullptr,nullptr,0);context.RSSetState(nullptr);raster->Release();
    view->Release();target->Release();readback->Release();constants->Release();vs->Release();ps->Release();alt->Release();
    std::puts("PASS cached draw-state pixels: repeated draws, shader switches, remapped constants, recycled RTV handles, external state invalidation and frame recycling");
}
