#include "../src/dx11_sky.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using Microsoft::WRL::ComPtr;
namespace {
constexpr int width=128,height=64;
constexpr int size=dx11::sky::noiseSize;
struct State {float top[4]{0,0,0,.32f},weather[4]{0,0,48,1},eye[4]{4500,30,4500,1},sun[4]{.5f,.8f,.3f,1};};
const char* prefix=R"HLSL(
cbuffer State:register(b0){float4 skyTop,skyWeather,cameraEye,sunDirection;};
)HLSL";
const char* entry=R"HLSL(
struct Output {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
Output VS(uint id:SV_VertexID){Output o;
    float2 v=float2((id<<1)&2,id&2);o.uv=v;o.position=float4(v*float2(2,-2)+float2(-1,1),0,1);return o;}
float4 PS(Output i):SV_TARGET{
    float azimuth=(i.uv.x-.5)*3.14159265;
    float elevation=lerp(.02,1.5,i.uv.y)*cameraEye.w;
    float3 ray=float3(cos(azimuth)*cos(elevation),sin(elevation),sin(azimuth)*cos(elevation));
    return skyTop.x>0?volumetricSky(ray,i.position.xy,skyTop.x,float3(.1,.2,.3),false):
        volumetricSky(ray,i.position.xy);}
)HLSL";
ComPtr<ID3DBlob> compile(const std::string& source,const char* name,const char* profile){
    ComPtr<ID3DBlob> code,error;
    HRESULT result=D3DCompile(source.data(),source.size(),nullptr,nullptr,nullptr,name,profile,
        D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);
    if(FAILED(result)&&error)std::fprintf(stderr,"%s\n",static_cast<const char*>(error->GetBufferPointer()));
    assert(SUCCEEDED(result));return code;
}
}
int main(){
    auto noise=dx11::sky::noiseVolume();assert(noise.size()==size*size*size*4);
    for(int channel=0;channel<3;++channel){
        int minimum=255,maximum=0;double adjacent=0,seam=0;
        for(int z=0;z<size;++z)for(int y=0;y<size;++y)for(int x=0;x<size;++x){
            auto at=((z*size+y)*size+x)*4+channel;minimum=std::min(minimum,int(noise[at]));maximum=std::max(maximum,int(noise[at]));
            if(x==size-1)seam+=std::abs(int(noise[at])-int(noise[at-(size-1)*4]));
            else adjacent+=std::abs(int(noise[at])-int(noise[at+4]));
        }
        assert(maximum-minimum>100);
        assert(seam/(size*size)<adjacent/(size*size*(size-1))*2+2); // seamless periodic filtering
    }
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,&level,&context);
    if(FAILED(hr))hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,&level,&context);
    assert(SUCCEEDED(hr));
    std::string source=std::string(prefix)+dx11::sky::shader+entry;
    auto vs=compile(source,"VS","vs_5_0"),ps=compile(source,"PS","ps_5_0");
    ComPtr<ID3D11VertexShader> vertex;ComPtr<ID3D11PixelShader> pixel;
    assert(SUCCEEDED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex)));
    assert(SUCCEEDED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel)));
    D3D11_TEXTURE3D_DESC volume{};volume.Width=volume.Height=volume.Depth=size;volume.MipLevels=1;
    volume.Format=DXGI_FORMAT_R8G8B8A8_UNORM;volume.Usage=D3D11_USAGE_IMMUTABLE;volume.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{noise.data(),size*4,size*size*4};ComPtr<ID3D11Texture3D> texture;
    ComPtr<ID3D11ShaderResourceView> noiseView;
    assert(SUCCEEDED(device->CreateTexture3D(&volume,&data,&texture)));
    assert(SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&noiseView)));
    D3D11_SAMPLER_DESC sampling{};sampling.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampling.AddressU=sampling.AddressV=sampling.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sampling.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;assert(SUCCEEDED(device->CreateSamplerState(&sampling,&sampler)));
    D3D11_TEXTURE2D_DESC image{};image.Width=width;image.Height=height;image.MipLevels=image.ArraySize=1;
    image.SampleDesc.Count=1;image.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;image.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target,readback;ComPtr<ID3D11RenderTargetView> output;
    assert(SUCCEEDED(device->CreateTexture2D(&image,nullptr,&target)));
    assert(SUCCEEDED(device->CreateRenderTargetView(target.Get(),nullptr,&output)));
    image.BindFlags=0;image.Usage=D3D11_USAGE_STAGING;image.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    assert(SUCCEEDED(device->CreateTexture2D(&image,nullptr,&readback)));
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(State);cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> buffer;assert(SUCCEEDED(device->CreateBuffer(&cb,nullptr,&buffer)));
    D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);
    auto* rtv=output.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex.Get(),nullptr,0);context->PSSetShader(pixel.Get(),nullptr,0);
    auto* srv=noiseView.Get();context->PSSetShaderResources(8,1,&srv);
    auto* filter=sampler.Get();context->PSSetSamplers(1,1,&filter);
    auto* constants=buffer.Get();context->PSSetConstantBuffers(0,1,&constants);
    auto render=[&](State state){
        context->UpdateSubresource(buffer.Get(),0,nullptr,&state,0,0);context->Draw(3,0);
        context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        assert(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)));
        std::vector<float> pixels(width*height*4);
        for(int y=0;y<height;++y){auto* row=reinterpret_cast<const float*>(static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch);
            std::copy(row,row+width*4,pixels.begin()+y*width*4);}
        context->Unmap(readback.Get(),0);
        for(float v:pixels)assert(std::isfinite(v)&&v>=0&&v<10);
        return pixels;
    };
    auto average=[](const std::vector<float>& p,int c){double sum=0;for(size_t i=c;i<p.size();i+=4)sum+=p[i];return sum/(width*height);};
    State state;state.top[3]=.05f;auto clear=render(state);
    state.top[3]=.78f;auto overcast=render(state);
    double clearOpacity=1-average(clear,3),cloudOpacity=1-average(overcast,3);
    std::printf("Sky GPU readback: clear opacity %.4f, overcast %.4f\n",clearOpacity,cloudOpacity);
    assert(cloudOpacity>clearOpacity+.15&&cloudOpacity>.2);
    state.weather[2]=0;auto noMarch=render(state);
    assert(average(noMarch,3)>.99999); // Profiling bypass must not divide by zero.
    state.weather[2]=48;
    state.weather[0]=2100;state.weather[1]=800;auto moved=render(state);
    double difference=0;for(size_t i=0;i<moved.size();i+=4)difference+=std::abs(moved[i]-overcast[i]);
    assert(difference/(width*height)>.01);
    state.weather[3]=0;auto night=render(state);assert(average(night,1)<average(moved,1)*.25);
    state.weather[3]=1;state.eye[1]=2150;render(state);
    state.eye[1]=3500;state.eye[3]=-1;auto above=render(state);assert(1-average(above,3)>.05);
    state.eye[1]=30;render(state); // downward rays must stay finite and clear of a remote shell
    state.eye[3]=1;state.top[0]=500;auto foreground=render(state);
    assert(std::abs(average(foreground,0)-.1)<.00001&&average(foreground,3)>.99999);
    std::puts("Sky shader, periodic noise, coverage, wind, night, inside/above/below layers passed.");
}
