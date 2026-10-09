#include "../src/dx12_backend.h"
#include "../src/dx12_cloud_shader.h"
#include <DirectXPackedVector.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

void dx12CloudScenarios(dx12::Device& device,dx12::Context& context){
    using namespace DirectX;
    auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    std::vector<dx12::Ref*> objects;
    auto own=[&](auto* p){objects.push_back(p);return p;};
    auto shader=[&](const std::string& source,const char* entry,const char* profile){
        dx12::ComPtr<ID3DBlob> code,error;HRESULT hr=D3DCompile(source.data(),source.size(),"cloud-test",nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);
        if(FAILED(hr)&&error)std::fprintf(stderr,"%s\n",static_cast<const char*>(error->GetBufferPointer()));check(SUCCEEDED(hr),"cloud shader compilation");
        dx12::Shader* output=nullptr;check(SUCCEEDED(device.CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&output)),"cloud shader object");return own(output);
    };
    const char* vertex=R"(
struct Out {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
Out VS(uint id:SV_VertexID){Out o;o.uv=float2((id<<1)&2,id&2);o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
)";
    const char* reference=R"(
float4 Reference(float4 position:SV_POSITION,float2 uv:TEXCOORD0):SV_TARGET{
    uint width,height;sceneDepth.GetDimensions(width,height);
    float4 farPoint=mul(float4(uv.x*2-1,1-uv.y*2,1,1),inverseViewProjection);
    float3 ray=normalize(farPoint.xyz/farPoint.w-cameraEye.xyz);
    float depth=sceneDepth.SampleLevel(linearSampler,uv,0);
    if(depth<.9999){
        if(cameraEye.y<=1600)return float4(0,0,0,1);
        float4 world=mul(float4(uv.x*2-1,1-uv.y*2,depth,1),inverseViewProjection);
        return volumetricSky(ray,uv*float2(width,height),length(world.xyz/world.w-cameraEye.xyz),0,false);
    }
    // Existing complete sky shading supplies the oracle. Remove its known
    // full-resolution background to compare the separately stored volume.
    float4 sky=volumetricSky(ray,uv*float2(width,height));
    return float4(sky.rgb-skyBackground(ray,0,true)*sky.a,sky.a);
}
float4 Resolve(float4 position:SV_POSITION,float2 uv:TEXCOORD0):SV_TARGET{
    bool sky=effects.x>.5;
    return resolveClouds(uv,float3(0,1,0),position.xy,100,sky);
}
)";
    auto* vs=shader(vertex,"VS","vs_5_0");auto* cloud=shader(dx12::cloud::prelude+dx12::cloud::pass,"CloudPS","ps_5_0");
    std::string referenceSource=dx12::cloud::prelude+dx12::cloud::reconstruction+reference;
    auto* ref=shader(referenceSource,"Reference","ps_5_0");auto* resolve=shader(referenceSource,"Resolve","ps_5_0");
    auto texture=[&](unsigned width,unsigned height,DXGI_FORMAT format,unsigned flags,const void* data=nullptr,unsigned pitch=0){
        dx12::TextureDesc desc{};desc.Width=width;desc.Height=height;desc.Format=format;desc.BindFlags=flags;dx12::Resource* result=nullptr;dx12::InitialData initial{data,pitch};
        check(SUCCEEDED(device.CreateTexture2D(&desc,data?&initial:nullptr,&result)),"cloud texture");return own(result);
    };
    auto srv=[&](dx12::Resource* resource){dx12::View* result=nullptr;check(SUCCEEDED(device.CreateShaderResourceView(resource,nullptr,&result)),"cloud SRV");return own(result);};
    auto rtv=[&](dx12::Resource* resource){dx12::View* result=nullptr;check(SUCCEEDED(device.CreateRenderTargetView(resource,nullptr,&result)),"cloud RTV");return own(result);};
    auto read=[&](dx12::Resource* source){auto desc=source->texture;desc.Usage=dx12::Readback;desc.BindFlags=0;dx12::Resource* result=nullptr;
        check(SUCCEEDED(device.CreateTexture2D(&desc,nullptr,&result)),"cloud readback");own(result);context.CopyResource(result,source);return result;};
    auto noise=dx11::sky::noiseVolume();dx12::VolumeDesc volume{};volume.Width=volume.Height=volume.Depth=dx11::sky::noiseSize;volume.Format=DXGI_FORMAT_R8G8B8A8_UNORM;volume.BindFlags=dx12::ShaderInput;
    dx12::InitialData noiseData{noise.data(),dx11::sky::noiseSize*4,dx11::sky::noiseSize*dx11::sky::noiseSize*4};dx12::Resource* noiseTexture=nullptr;
    check(SUCCEEDED(device.CreateTexture3D(&volume,&noiseData,&noiseTexture)),"cloud noise volume");own(noiseTexture);auto* noiseView=srv(noiseTexture);
    D3D12_SAMPLER_DESC sampling{};sampling.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampling.AddressU=sampling.AddressV=sampling.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;sampling.MaxLOD=D3D12_FLOAT32_MAX;
    dx12::SamplerState* sampler=nullptr;check(SUCCEEDED(device.CreateSamplerState(&sampling,&sampler)),"cloud sampler");own(sampler);
    dx12::BufferDesc bd{};bd.ByteWidth=sizeof(dx12::cloud::Constants);bd.BindFlags=dx12::Constant;dx12::Buffer* cb=nullptr;check(SUCCEEDED(device.CreateBuffer(&bd,nullptr,&cb)),"cloud constants");own(cb);
    D3D12_RASTERIZER_DESC raster{};raster.FillMode=D3D12_FILL_MODE_SOLID;raster.CullMode=D3D12_CULL_MODE_NONE;raster.DepthClipEnable=TRUE;
    dx12::RasterizerState* rs=nullptr;check(SUCCEEDED(device.CreateRasterizerState(&raster,&rs)),"cloud raster");own(rs);
    context.ClearState();context.VSSetShader(vs,nullptr,0);context.HSSetShader(nullptr,nullptr,0);context.DSSetShader(nullptr,nullptr,0);context.IASetInputLayout(nullptr);context.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context.RSSetState(rs);context.OMSetDepthStencilState(nullptr,0);context.OMSetBlendState(nullptr,nullptr,UINT_MAX);context.VSSetConstantBuffers(0,1,&cb);context.PSSetSamplers(0,1,&sampler);context.PSSetSamplers(1,1,&sampler);context.PSSetShaderResources(8,1,&noiseView);
    float maximumError=0;unsigned cases=0;
    for(auto size:std::array<std::pair<unsigned,unsigned>,3>{{{1,1},{65,33},{128,64}}}){
        auto [width,height]=size;unsigned halfWidth=(width+1)/2,halfHeight=(height+1)/2;
        std::vector<float> depths(width*height,1);
        auto* depth=texture(width,height,DXGI_FORMAT_R32_FLOAT,dx12::ShaderInput,depths.data(),width*4);auto* depthView=srv(depth);context.PSSetShaderResources(1,1,&depthView);
        auto* half=texture(halfWidth,halfHeight,DXGI_FORMAT_R16G16B16A16_FLOAT,dx12::RenderTarget|dx12::ShaderInput);
        auto* guide=texture(halfWidth,halfHeight,DXGI_FORMAT_R32_FLOAT,dx12::RenderTarget|dx12::ShaderInput);
        auto* full=texture(halfWidth,halfHeight,DXGI_FORMAT_R32G32B32A32_FLOAT,dx12::RenderTarget);
        dx12::View* outputs[]={rtv(half),rtv(guide)};auto* fullTarget=rtv(full);
        D3D12_VIEWPORT viewport{0,0,float(halfWidth),float(halfHeight),0,1};context.RSSetViewports(1,&viewport);
        for(int mode=0;mode<7;++mode){
            std::fill(depths.begin(),depths.end(),mode>=5?.99f:1.0f);context.UpdateSubresource(depth,0,nullptr,depths.data(),width*4,0);
            dx12::cloud::Constants state{};state.cameraEye={4500,mode==2||mode==6?2150.0f:mode==3?3300.0f:30.0f,4500,1};state.skyTop.w=mode==0?.05f:.78f;state.skyWeather={mode==4?2100.0f:0,800,48,mode==4?0.0f:1.0f};state.sunDirection={.5f,.8f,.3f,1};
            auto eye=XMLoadFloat4(&state.cameraEye);auto direction=XMVectorSet(0,mode==3?-.4f:.5f,1,0);
            auto vp=XMMatrixLookAtRH(eye,eye+direction,XMVectorSet(0,1,0,0))*XMMatrixPerspectiveFovRH(1.3f,float(width)/height,2,5000);
            XMStoreFloat4x4(&state.viewProjection,vp);XMStoreFloat4x4(&state.inverseViewProjection,XMMatrixInverse(nullptr,vp));
            context.UpdateSubresource(cb,0,nullptr,&state,0,0);context.OMSetRenderTargets(2,outputs,nullptr);context.PSSetShader(cloud,nullptr,0);context.Draw(3,0);
            context.OMSetRenderTargets(1,&fullTarget,nullptr);context.PSSetShader(ref,nullptr,0);context.Draw(3,0);
            auto* a=read(half);auto* b=read(full);dx12::MappedData actual{},expected{};check(SUCCEEDED(context.Map(a,0,dx12::MapRead,0,&actual))&&SUCCEEDED(context.Map(b,0,dx12::MapRead,0,&expected)),"cloud volume comparison map");
            for(unsigned y=0;y<halfHeight;++y)for(unsigned x=0;x<halfWidth*4;++x){
                auto* h=reinterpret_cast<const PackedVector::HALF*>(static_cast<const unsigned char*>(actual.pData)+y*actual.RowPitch);
                auto* f=reinterpret_cast<const float*>(static_cast<const unsigned char*>(expected.pData)+y*expected.RowPitch);
                float value=PackedVector::XMConvertHalfToFloat(h[x]);check(std::isfinite(value)&&value>=0&&value<=2,"finite cloud radiance/transmission");
                float error=std::abs(value-f[x]);maximumError=std::max(maximumError,error);check(error<.002f,"split volume must match complete sky shading within half-float precision");
            }
            context.Unmap(a,0);context.Unmap(b,0);++cases;
        }
    }
    // Strongly contrasting synthetic layers verify reconstruction rejects the
    // other depth class, including odd output sizes and the fallback path.
    const unsigned width=13,height=7,halfWidth=7,halfHeight=4;
    std::vector<XMFLOAT4> colors(halfWidth*halfHeight);std::vector<float> guides(colors.size());
    for(unsigned y=0;y<halfHeight;++y)for(unsigned x=0;x<halfWidth;++x){bool sky=x<3;colors[y*halfWidth+x]=sky?XMFLOAT4{.25f,.5f,.75f,.5f}:XMFLOAT4{.75f,.25f,.125f,.25f};guides[y*halfWidth+x]=sky?-1.0f:100.0f;}
    auto* colorsView=srv(texture(halfWidth,halfHeight,DXGI_FORMAT_R32G32B32A32_FLOAT,dx12::ShaderInput,colors.data(),halfWidth*16));
    auto* guidesView=srv(texture(halfWidth,halfHeight,DXGI_FORMAT_R32_FLOAT,dx12::ShaderInput,guides.data(),halfWidth*4));
    auto* output=texture(width,height,DXGI_FORMAT_R32G32B32A32_FLOAT,dx12::RenderTarget);auto* target=rtv(output);dx12::View* inputs[]={colorsView,guidesView};
    context.PSSetShaderResources(9,2,inputs);context.OMSetRenderTargets(1,&target,nullptr);context.PSSetShader(resolve,nullptr,0);D3D12_VIEWPORT viewport{0,0,width,height,0,1};context.RSSetViewports(1,&viewport);
    for(int mode=0;mode<3;++mode){dx12::cloud::Constants state{};state.cameraEye={4500,30,4500,1};state.skyTop.w=.8f;state.skyWeather.z=0;state.temporal.z=1;state.effects.x=mode==0?1.0f:0.0f;
        if(mode==2){for(auto& g:guides)g=200;context.UpdateSubresource(guidesView->resource,0,nullptr,guides.data(),halfWidth*4,0);}
        context.UpdateSubresource(cb,0,nullptr,&state,0,0);context.Draw(3,0);auto* staging=read(output);dx12::MappedData mapped{};check(SUCCEEDED(context.Map(staging,0,dx12::MapRead,0,&mapped)),"cloud reconstruction map");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto* p=reinterpret_cast<const XMFLOAT4*>(static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch)+x;
            const XMFLOAT4 expected=mode==0?XMFLOAT4{.25f,.5f,.75f,.5f}:XMFLOAT4{.75f,.25f,.125f,.25f};
            bool fallback=std::abs(p->x)<1e-6f&&std::abs(p->y)<1e-6f&&std::abs(p->z)<1e-6f&&std::abs(p->w-1)<1e-6f;
            bool compatible=std::abs(p->x-expected.x)<1e-6f&&std::abs(p->y-expected.y)<1e-6f&&std::abs(p->z-expected.z)<1e-6f&&std::abs(p->w-expected.w)<1e-6f;
            check(mode==2?fallback:(compatible||fallback),"cloud reconstruction must reject sky/surface mixing and incompatible termination depth");
            if(mode==0&&x==0)check(compatible,"valid sky sample must be reconstructed");if(mode==1&&x==width-1)check(compatible,"valid geometry sample must be reconstructed");
        }
        context.Unmap(staging,0);
    }
    context.ClearState();context.VSSetShader(nullptr,nullptr,0);context.PSSetShader(nullptr,nullptr,0);context.RSSetState(nullptr);device.submit();
    for(auto it=objects.rbegin();it!=objects.rend();++it)(*it)->Release();
    std::printf("PASS cloud GPU: %u volume cases, 1x1/odd/even sizes, clear/overcast/wind/night/inside/above, sky/surface edges and depth rejection; max split error %.7f\n",cases,maximumError);
}
