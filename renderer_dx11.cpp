#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "game.h"
#include "camera.h"
#include "dx11_assets.h"
#include "ui.h"
#include "weather.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {
namespace {
using namespace DirectX;
IDXGISwapChain* swapChain=nullptr;
ID3D11Device* device=nullptr;
ID3D11DeviceContext* context=nullptr;
ID3D11RenderTargetView* target=nullptr;
ID3D11Texture2D* depthTexture=nullptr;
ID3D11DepthStencilView* depthView=nullptr;
ID3D11VertexShader* sceneVS=nullptr;
ID3D11VertexShader* instanceVS=nullptr;
ID3D11PixelShader* scenePS=nullptr;
ID3D11PixelShader* alphaShadowPS=nullptr;
ID3D11HullShader* sceneHS=nullptr;
ID3D11DomainShader* sceneDS=nullptr;
ID3D11VertexShader* hudVS=nullptr;
ID3D11PixelShader* hudPS=nullptr;
ID3D11InputLayout* inputLayout=nullptr;
ID3D11InputLayout* instanceLayout=nullptr;
ID3D11Buffer* vertexBuffer=nullptr;
ID3D11Buffer* staticBuffer=nullptr;
ID3D11Buffer* instanceBuffer=nullptr;
std::unordered_map<const dx11::Mesh*,ID3D11Buffer*> meshBuffers;
std::unordered_map<const dx11::Mesh*,ID3D11ShaderResourceView*> modelTextures;
std::unordered_map<std::wstring,ID3D11ShaderResourceView*> sharedModelTextures;
ID3D11Buffer* sceneBuffer=nullptr;
ID3D11RasterizerState* rasterState=nullptr;
ID3D11RasterizerState* shadowRaster=nullptr;
ID3D11Texture2D* shadowTexture=nullptr;
ID3D11DepthStencilView* shadowDepth=nullptr;
ID3D11ShaderResourceView* shadowView=nullptr;
ID3D11SamplerState* shadowSampler=nullptr;
int shadowSize=0;
ID3D11DepthStencilState* noDepth=nullptr;
ID3D11DepthStencilState* readDepth=nullptr;
ID3D11BlendState* alphaBlend=nullptr;
ID3D11SamplerState* sampler=nullptr;
ID3D11ShaderResourceView* textures[2]{};
ID3D11ShaderResourceView* detailTextures[dx11::MATERIAL_GROUPS]{};
ID3D11ShaderResourceView* normalTextures[dx11::MATERIAL_GROUPS]{};
ID3D11Texture2D* hudTexture=nullptr;
ID3D11ShaderResourceView* hudView=nullptr;
ULONG_PTR gdiplusToken=0;
size_t vertexCapacity=0;
size_t instanceCapacity=0;
int bufferW=0,bufferH=0;
std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];
size_t staticStarts[dx11::MATERIAL_GROUPS]{},staticCounts[dx11::MATERIAL_GROUPS]{};
std::vector<dx11::Vertex> vertices;
std::vector<dx11::ModelInstance> models;
struct InstanceData {XMFLOAT4 a,b,c,tint;};
struct InstanceBatch {const dx11::Mesh* mesh;int material;UINT start,count;};
std::vector<InstanceData> instanceData;
std::vector<InstanceBatch> instanceBatches;
std::vector<unsigned char> hudPixels;
struct SceneConstants {
    XMFLOAT4X4 viewProjection;
    XMFLOAT4X4 shadowViewProjection;
    XMFLOAT4 sun;
    XMFLOAT4 ambient;
    XMFLOAT4 fogColor;
    XMFLOAT4 eye;
    XMFLOAT4 params;
};
template<class T> void release(T*& object){if(object){object->Release();object=nullptr;}}
std::wstring executableFolder(){
    wchar_t filename[MAX_PATH]{};GetModuleFileNameW(nullptr,filename,MAX_PATH);
    std::wstring result(filename);auto slash=result.find_last_of(L"\\/");
    return result.substr(0,slash);
}
bool compile(const char* source,const char* entry,const char* profile,ID3DBlob** output){
    ID3DBlob* errors=nullptr;
    HRESULT status=D3DCompile(source,std::strlen(source),"MiniCityShader",nullptr,nullptr,
        entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,output,&errors);
    if(FAILED(status)&&errors)MessageBoxA(win,static_cast<const char*>(errors->GetBufferPointer()),
        "Direct3D shader compile error",MB_ICONERROR);
    release(errors);return SUCCEEDED(status);
}
const char* sceneShader=R"HLSL(
cbuffer Scene : register(b0){
    row_major float4x4 viewProjection;
    row_major float4x4 shadowViewProjection;
    float4 sun;
    float4 ambient;
    float4 fogColor;
    float4 eye;
    float4 params;
};
Texture2D diffuseTexture : register(t0);
Texture2D detailTexture : register(t1);
Texture2D normalTexture : register(t2);
Texture2D foliageTexture : register(t3);
Texture2D shadowTexture : register(t4);
SamplerState linearSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);
struct Input { float3 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; float4 color:COLOR0; };
struct InstancedInput {
    float3 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; float4 color:COLOR0;
    float4 a:INSTANCE0; float4 b:INSTANCE1; float4 c:INSTANCE2; float4 tint:INSTANCE3;
};
struct Output { float4 position:SV_POSITION; float3 world:TEXCOORD1; float3 normal:NORMAL; float2 uv:TEXCOORD0; float4 color:COLOR0; float4 shadowPosition:TEXCOORD2; };
Output VS(Input input){
    Output result;
    result.position=mul(float4(input.position,1),viewProjection);
    result.shadowPosition=mul(float4(input.position,1),shadowViewProjection);
    result.world=input.position;result.normal=input.normal;result.uv=input.uv;result.color=input.color;
    return result;
}
Output VSInstanced(InstancedInput input){
    float3 local=float3((input.position.x-input.c.x)*input.a.x,
        (input.position.y-input.c.y)*input.a.y,
        (input.position.z-input.c.z)*input.a.z);
    float3 pitched=float3(local.x,input.tint.w*local.y+input.c.w*local.z,
        -input.c.w*local.y+input.tint.w*local.z);
    float3 world=float3(input.b.y+input.a.w*pitched.x+input.b.x*pitched.z,
        input.b.z+pitched.y,input.b.w-input.b.x*pitched.x+input.a.w*pitched.z);
    float3 scaledNormal=input.normal/max(input.a.xyz,float3(0.0001,0.0001,0.0001));
    float3 pitchedNormal=float3(scaledNormal.x,
        input.tint.w*scaledNormal.y+input.c.w*scaledNormal.z,
        -input.c.w*scaledNormal.y+input.tint.w*scaledNormal.z);
    float3 normal=normalize(float3(input.a.w*pitchedNormal.x+input.b.x*pitchedNormal.z,
        pitchedNormal.y,-input.b.x*pitchedNormal.x+input.a.w*pitchedNormal.z));
    Output result;
    result.position=mul(float4(world,1),viewProjection);
    result.shadowPosition=mul(float4(world,1),shadowViewProjection);
    result.world=world;result.normal=normal;result.uv=input.uv;
    result.color=float4(input.color.rgb*input.tint.rgb,input.color.a);
    return result;
}
struct TessFactors {float edge[3]:SV_TessFactor;float inside:SV_InsideTessFactor;};
float edgeFactor(float3 a,float3 b){
    float distanceToEye=distance((a+b)*0.5,eye.xyz);
    int material=(int)(params.x+0.5);
    float maximum=material==11?(eye.w>1.5?4.0:2.0):
        material==4?2.0:(eye.w>1.5?3.0:2.0);
    return 1.0+floor((maximum-1.0)*saturate((300.0-distanceToEye)/230.0)+0.5);
}
TessFactors HSFactors(InputPatch<Output,3> patch,uint patchId:SV_PrimitiveID){
    TessFactors factors;
    factors.edge[0]=edgeFactor(patch[1].world,patch[2].world);
    factors.edge[1]=edgeFactor(patch[2].world,patch[0].world);
    factors.edge[2]=edgeFactor(patch[0].world,patch[1].world);
    factors.inside=max(factors.edge[0],max(factors.edge[1],factors.edge[2]));
    return factors;
}
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HSFactors")]
Output HS(InputPatch<Output,3> patch,uint controlPoint:SV_OutputControlPointID){return patch[controlPoint];}
float surfaceDisplacement(float3 world,float3 normal,float2 uv){
    int material=(int)(params.x+0.5);
    if(material==10){
        int2 tile=int2(floor(saturate(uv)*4.0));
        int patch=tile.y*4+tile.x;
        if(patch!=0&&patch!=1&&patch!=2&&patch!=8&&patch!=9&&patch!=11)return 0;
        return 0.18*sin(world.x*0.075)*sin(world.y*0.064+world.z*0.052);
    }
    if(material==11){
        if(normal.y<0.7)return 0;
        float jointX=abs(frac(world.x/9.0)-0.5);
        float jointZ=abs(frac(world.z/9.0)-0.5);
        return 0.08+0.06*sin(world.x*0.15)*sin(world.z*0.14)-
            0.02*(smoothstep(0.47,0.50,jointX)+smoothstep(0.47,0.50,jointZ));
    }
    if(material==4)
        return 0.32*sin(world.x*0.105+world.y*0.081)*sin(world.z*0.097);
    return 0.16*sin(world.x*0.083+world.y*0.069)*sin(world.z*0.081);
}
[domain("tri")]
Output DS(TessFactors factors,const OutputPatch<Output,3> patch,float3 bary:SV_DomainLocation){
    Output result;
    result.world=patch[0].world*bary.x+patch[1].world*bary.y+patch[2].world*bary.z;
    result.normal=normalize(patch[0].normal*bary.x+patch[1].normal*bary.y+patch[2].normal*bary.z);
    result.uv=patch[0].uv*bary.x+patch[1].uv*bary.y+patch[2].uv*bary.z;
    result.color=patch[0].color*bary.x+patch[1].color*bary.y+patch[2].color*bary.z;
    result.world+=result.normal*surfaceDisplacement(result.world,result.normal,result.uv);
    result.position=mul(float4(result.world,1),viewProjection);
    result.shadowPosition=mul(float4(result.world,1),shadowViewProjection);
    return result;
}
float4 PS(Output input):SV_TARGET{
    float4 base=input.color;
    int material=(int)(params.x+0.5);
    bool modelTexture=frac(params.x)>0.1;
    if(material==1||modelTexture)
        base*=diffuseTexture.Sample(linearSampler,input.uv);
    if(modelTexture&&material!=13)clip(base.a-0.35);
    if(material==13){
        float distanceToEye=distance(input.world,eye.xyz);
        float fog=saturate((distanceToEye-params.y)/max(1,params.z-params.y));
        return float4(lerp(base.rgb,fogColor.rgb,fog),base.a*(1-fog));
    }
    float3 normal=normalize(input.normal);
    if(material==10){
        float3 bump=normalTexture.Sample(linearSampler,input.uv).xyz*2-1;
        float3 tangent=abs(normal.y)>0.5?float3(1,0,0):
            abs(normal.x)>0.5?float3(0,0,1):float3(1,0,0);
        float3 bitangent=normalize(cross(normal,tangent));
        normal=normalize(lerp(normal,normalize(tangent*bump.x+
            bitangent*bump.y+normal*bump.z),0.48));
    }else if(material>=2){
        float scale=material==2||material==3||material==11||material==12?0.055:
            material==4?0.085:material==5?0.25:material==6?0.11:
            material==7?0.028:material==8?0.022:0.03;
        float3 axis=abs(normal);
        float2 uv=axis.y>axis.x&&axis.y>axis.z?input.world.xz*scale:
            axis.x>axis.z?input.world.zy*scale:input.world.xy*scale;
        float3 surface=detailTexture.Sample(linearSampler,uv).rgb;
        float strength=material==5?0.24:material==6?0.18:0.80;
        if(material==4&&base.g>base.r*1.18){
            surface=foliageTexture.Sample(linearSampler,uv).rgb;
            base.rgb=lerp(base.rgb,float3(0.37,0.62,0.28),0.45);
            strength=0.54;
        }else if(material==4)strength=0.72;
        if(material==2||material==3||material==11||material==12)
            strength=base.b>base.r*1.15&&base.b>base.g*1.05?0.06:0.79;
        if(modelTexture)strength*=0.13;
        base.rgb*=lerp(float3(1,1,1),surface*1.65,strength);
        float3 bump=normalTexture.Sample(linearSampler,uv).xyz*2-1;
        float3 tangent=axis.y>axis.x&&axis.y>axis.z?float3(1,0,0):
            axis.x>axis.z?float3(0,0,1):float3(1,0,0);
        float3 bitangent=normalize(cross(normal,tangent));
        float3 mapped=normalize(tangent*bump.x+bitangent*bump.y+normal*bump.z);
        normal=normalize(lerp(normal,mapped,strength*0.55));
    }
    float diffuse=saturate(dot(normal,normalize(sun.xyz)));
    float visibility=1;
    if(params.w>0.5&&sun.w>0.4&&input.shadowPosition.w>0){
        float3 projected=input.shadowPosition.xyz/input.shadowPosition.w;
        float2 uv=float2(projected.x*0.5+0.5,0.5-projected.y*0.5);
        if(all(uv>=0)&&all(uv<=1)&&projected.z>0&&projected.z<1){
            float bias=max(0.00018,0.0012*(1-diffuse));
            visibility=lerp(0.35,1.0,shadowTexture.SampleCmpLevelZero(shadowSampler,uv,projected.z-bias));
        }
    }
    float light=ambient.x+sun.w*diffuse*visibility;
    float3 lit=base.rgb*light;
    if((material==10||material==12)&&base.b>base.r*1.12){
        float3 viewDirection=normalize(eye.xyz-input.world);
        float3 halfVector=normalize(viewDirection+normalize(sun.xyz));
        float reflection=pow(saturate(dot(normal,halfVector)),18);
        lit+=float3(0.28,0.39,0.45)*reflection*sun.w;
    }
    float night=1-saturate((ambient.x-0.32)/0.42);
    float lampColumn=round((input.world.x-368)/450);
    float lampRow=round((input.world.z-115)/215);
    float2 lampPosition=float2(368+lampColumn*450,115+lampRow*215);
    float lampRadius=saturate(1-length(input.world.xz-lampPosition)/110);
    float lampHeight=saturate((60-input.world.y)/60);
    float lampGlow=night*lampRadius*lampRadius*lampHeight*1.4;
    lit+=base.rgb*float3(1.0,0.72,0.36)*lampGlow;
    float distanceToEye=distance(input.world,eye.xyz);
    float fog=saturate((distanceToEye-params.y)/max(1,params.z-params.y));
    float3 mapped=lit/(1+max(0,lit-0.85));
    return float4(lerp(mapped,fogColor.rgb,fog),base.a);
}
void PSShadowAlpha(Output input){
    clip(diffuseTexture.Sample(linearSampler,input.uv).a-0.35);
}
)HLSL";
const char* hudShader=R"HLSL(
Texture2D image : register(t0);
SamplerState linearSampler : register(s0);
struct Output { float4 position:SV_POSITION; float2 uv:TEXCOORD0; };
Output VS(uint id:SV_VertexID){
    Output result;
    float2 positions[4]={float2(-1,1),float2(1,1),float2(-1,-1),float2(1,-1)};
    float2 uv[4]={float2(0,0),float2(1,0),float2(0,1),float2(1,1)};
    result.position=float4(positions[id],0,1);result.uv=uv[id];return result;
}
float4 PS(Output input):SV_TARGET{return image.Sample(linearSampler,input.uv);}
)HLSL";
bool createShaders(){
    ID3DBlob *vs=nullptr,*instanced=nullptr,*ps=nullptr,*shadowAlpha=nullptr,*hull=nullptr,*domain=nullptr,*hudVertex=nullptr,*hudPixel=nullptr;
    if(!compile(sceneShader,"VS","vs_5_0",&vs)||!compile(sceneShader,"PS","ps_5_0",&ps)||
       !compile(sceneShader,"VSInstanced","vs_5_0",&instanced)||
       !compile(sceneShader,"PSShadowAlpha","ps_5_0",&shadowAlpha)||
       !compile(sceneShader,"HS","hs_5_0",&hull)||!compile(sceneShader,"DS","ds_5_0",&domain)||
       !compile(hudShader,"VS","vs_5_0",&hudVertex)||!compile(hudShader,"PS","ps_5_0",&hudPixel)){
        release(vs);release(instanced);release(ps);release(shadowAlpha);release(hull);release(domain);
        release(hudVertex);release(hudPixel);return false;}
    HRESULT result=device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&sceneVS);
    if(SUCCEEDED(result))result=device->CreateVertexShader(instanced->GetBufferPointer(),instanced->GetBufferSize(),nullptr,&instanceVS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&scenePS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(shadowAlpha->GetBufferPointer(),shadowAlpha->GetBufferSize(),nullptr,&alphaShadowPS);
    if(SUCCEEDED(result))result=device->CreateHullShader(hull->GetBufferPointer(),hull->GetBufferSize(),nullptr,&sceneHS);
    if(SUCCEEDED(result))result=device->CreateDomainShader(domain->GetBufferPointer(),domain->GetBufferSize(),nullptr,&sceneDS);
    if(SUCCEEDED(result))result=device->CreateVertexShader(hudVertex->GetBufferPointer(),hudVertex->GetBufferSize(),nullptr,&hudVS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(hudPixel->GetBufferPointer(),hudPixel->GetBufferSize(),nullptr,&hudPS);
    D3D11_INPUT_ELEMENT_DESC layout[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(SUCCEEDED(result))result=device->CreateInputLayout(layout,4,vs->GetBufferPointer(),
        vs->GetBufferSize(),&inputLayout);
    D3D11_INPUT_ELEMENT_DESC instanceElements[]={
        {"INSTANCE",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,16,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,32,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,48,D3D11_INPUT_PER_INSTANCE_DATA,1}};
    D3D11_INPUT_ELEMENT_DESC fullLayout[8]{};
    std::copy(std::begin(layout),std::end(layout),fullLayout);
    std::copy(std::begin(instanceElements),std::end(instanceElements),fullLayout+4);
    if(SUCCEEDED(result))result=device->CreateInputLayout(fullLayout,8,instanced->GetBufferPointer(),
        instanced->GetBufferSize(),&instanceLayout);
    release(vs);release(instanced);release(ps);release(shadowAlpha);release(hull);release(domain);
    release(hudVertex);release(hudPixel);
    return SUCCEEDED(result);
}
bool loadTexture(const std::wstring& file,ID3D11ShaderResourceView** view){
    Gdiplus::Bitmap image(file.c_str());if(image.GetLastStatus()!=Gdiplus::Ok)return false;
    UINT width=image.GetWidth(),height=image.GetHeight();
    if(width==0||height==0||width>8192||height>8192)return false;
    Gdiplus::Rect region(0,0,width,height);Gdiplus::BitmapData bits{};
    if(image.LockBits(&region,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&bits)!=Gdiplus::Ok)return false;
    std::vector<unsigned char> pixels(size_t(width)*height*4);
    for(UINT row=0;row<height;++row)
        std::memcpy(pixels.data()+size_t(row)*width*4,
            static_cast<const unsigned char*>(bits.Scan0)+ptrdiff_t(row)*bits.Stride,width*4);
    image.UnlockBits(&bits);
    D3D11_TEXTURE2D_DESC description{};description.Width=width;description.Height=height;
    description.MipLevels=1;description.ArraySize=1;description.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count=1;description.Usage=D3D11_USAGE_IMMUTABLE;
    description.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=pixels.data();initial.SysMemPitch=width*4;
    ID3D11Texture2D* texture=nullptr;
    HRESULT status=device->CreateTexture2D(&description,&initial,&texture);
    if(SUCCEEDED(status))status=device->CreateShaderResourceView(texture,nullptr,view);
    release(texture);return SUCCEEDED(status);
}
bool createTargets(int width,int height){
    if(width<1||height<1)return false;
    if(bufferW&&FAILED(swapChain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0)))return false;
    ID3D11Texture2D* backBuffer=nullptr;
    HRESULT result=swapChain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&backBuffer));
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(backBuffer,nullptr,&target);
    release(backBuffer);if(FAILED(result))return false;
    D3D11_TEXTURE2D_DESC depthDescription{};
    depthDescription.Width=width;depthDescription.Height=height;depthDescription.MipLevels=1;
    depthDescription.ArraySize=1;depthDescription.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDescription.SampleDesc.Count=1;depthDescription.Usage=D3D11_USAGE_DEFAULT;
    depthDescription.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    result=device->CreateTexture2D(&depthDescription,nullptr,&depthTexture);
    if(SUCCEEDED(result))result=device->CreateDepthStencilView(depthTexture,nullptr,&depthView);
    if(FAILED(result))return false;
    D3D11_TEXTURE2D_DESC hudDescription{};hudDescription.Width=width;hudDescription.Height=height;
    hudDescription.MipLevels=1;hudDescription.ArraySize=1;
    hudDescription.Format=DXGI_FORMAT_B8G8R8A8_UNORM;hudDescription.SampleDesc.Count=1;
    hudDescription.Usage=D3D11_USAGE_DYNAMIC;hudDescription.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    hudDescription.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    result=device->CreateTexture2D(&hudDescription,nullptr,&hudTexture);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(hudTexture,nullptr,&hudView);
    if(FAILED(result))return false;
    D3D11_VIEWPORT viewport{};viewport.Width=float(width);viewport.Height=float(height);
    viewport.MinDepth=0;viewport.MaxDepth=1;context->RSSetViewports(1,&viewport);
    bufferW=width;bufferH=height;hudPixels.resize(size_t(width)*height*4);return true;
}
bool growVertexBuffer(size_t count){
    if(count<=vertexCapacity)return true;
    release(vertexBuffer);vertexCapacity=std::max(count,vertexCapacity*2+100000);
    if(vertexCapacity>3000000)return false;
    D3D11_BUFFER_DESC description{};
    description.ByteWidth=UINT(vertexCapacity*sizeof(dx11::Vertex));
    description.Usage=D3D11_USAGE_DYNAMIC;description.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    description.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device->CreateBuffer(&description,nullptr,&vertexBuffer));
}
bool prepareInstances(){
    std::sort(models.begin(),models.end(),[](const auto& a,const auto& b){
        if(a.material!=b.material)return a.material<b.material;
        return std::less<const dx11::Mesh*>{}(a.source,b.source);
    });
    instanceData.clear();instanceBatches.clear();
    for(const auto& model:models){
        if(meshBuffers.find(model.source)==meshBuffers.end()){
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth=UINT(model.source->vertices.size()*sizeof(dx11::Vertex));
            desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA data{};data.pSysMem=model.source->vertices.data();
            ID3D11Buffer* buffer=nullptr;
            if(FAILED(device->CreateBuffer(&desc,&data,&buffer)))return false;
            meshBuffers.emplace(model.source,buffer);
        }
        if(!model.source->textureFile.empty()&&
           modelTextures.find(model.source)==modelTextures.end()){
            ID3D11ShaderResourceView* texture=nullptr;
            auto cached=sharedModelTextures.find(model.source->textureFile);
            if(cached!=sharedModelTextures.end()){
                texture=cached->second;texture->AddRef();
            }else{
                if(!loadTexture(model.source->textureFile,&texture))return false;
                sharedModelTextures.emplace(model.source->textureFile,texture);
            }
            modelTextures.emplace(model.source,texture);
        }
        if(instanceBatches.empty()||instanceBatches.back().mesh!=model.source||
           instanceBatches.back().material!=model.material)
            instanceBatches.push_back({model.source,model.material,UINT(instanceData.size()),0});
        ++instanceBatches.back().count;
        instanceData.push_back({{model.scaleX,model.scaleY,model.scaleZ,model.cosYaw},
            {model.sinYaw,model.x,model.y,model.z},
            {model.centerX,model.minY,model.centerZ,model.sinPitch},
            {model.r,model.g,model.b,model.cosPitch}});
    }
    if(instanceData.empty())return true;
    if(instanceData.size()>instanceCapacity){
        release(instanceBuffer);
        instanceCapacity=std::max(instanceData.size(),instanceCapacity*2+128);
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth=UINT(instanceCapacity*sizeof(InstanceData));
        desc.Usage=D3D11_USAGE_DYNAMIC;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(FAILED(device->CreateBuffer(&desc,nullptr,&instanceBuffer)))return false;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(instanceBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return false;
    std::memcpy(mapped.pData,instanceData.data(),instanceData.size()*sizeof(InstanceData));
    context->Unmap(instanceBuffer,0);
    return true;
}
bool createStaticGeometry(){
    std::vector<dx11::Vertex> staticGroups[dx11::MATERIAL_GROUPS];
    dx11::buildStaticScene(staticGroups);
    std::vector<dx11::Vertex> packed;
    for(int group=0;group<dx11::MATERIAL_GROUPS;++group){
        staticStarts[group]=packed.size();staticCounts[group]=staticGroups[group].size();
        packed.insert(packed.end(),staticGroups[group].begin(),staticGroups[group].end());
    }
    if(packed.empty())return false;
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=UINT(packed.size()*sizeof(dx11::Vertex));
    desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data{};data.pSysMem=packed.data();
    return SUCCEEDED(device->CreateBuffer(&desc,&data,&staticBuffer));
}
void setTessellation(int material){
    bool enabled=ui::graphicsQuality>0&&
        (material==2||material==4||material==10||material==11||material==12);
    context->IASetPrimitiveTopology(enabled?D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->HSSetShader(enabled?sceneHS:nullptr,nullptr,0);
    context->DSSetShader(enabled?sceneDS:nullptr,nullptr,0);
}
void drawInstances(bool shadow,SceneConstants& constants,bool transparent=false){
    context->IASetInputLayout(instanceLayout);
    context->VSSetShader(instanceVS,nullptr,0);
    for(const auto& batch:instanceBatches){
        if(batch.mesh->transparent!=transparent)continue;
        if(shadow&&!batch.mesh->castsShadow)continue;
        // Imported foliage already has dense leaf geometry. Hull/domain
        // tessellation multiplies its cost without improving the silhouette.
        setTessellation(batch.mesh->textured&&batch.material==4?0:batch.material);
        ID3D11Buffer* buffers[]={meshBuffers.at(batch.mesh),instanceBuffer};
        UINT strides[]={sizeof(dx11::Vertex),sizeof(InstanceData)},offsets[]={0,0};
        context->IASetVertexBuffers(0,2,buffers,strides,offsets);
        auto texture=modelTextures.find(batch.mesh);
        bool hasModelTexture=texture!=modelTextures.end();
        constants.params.x=float(batch.material)+(hasModelTexture?0.25f:0.0f);
        context->UpdateSubresource(sceneBuffer,0,nullptr,&constants,0,0);
        if(shadow){
            bool alpha=batch.mesh->alphaTest&&hasModelTexture;
            context->PSSetShader(alpha?alphaShadowPS:nullptr,nullptr,0);
            if(alpha){
                ID3D11ShaderResourceView* base=texture->second;
                context->PSSetShaderResources(0,1,&base);
            }
        }else{
            int group=batch.material;
            ID3D11ShaderResourceView* base=hasModelTexture?texture->second:nullptr;
            ID3D11ShaderResourceView* resources[4]={base,
                group<dx11::MATERIAL_GROUPS?detailTextures[group]:nullptr,
                group<dx11::MATERIAL_GROUPS?normalTextures[group]:nullptr,
                detailTextures[9]};
            context->PSSetShaderResources(0,4,resources);
        }
        context->DrawInstanced(UINT(batch.mesh->vertices.size()),batch.count,0,batch.start);
        ++drawCalls;
    }
}
bool createStates(){
    D3D11_BUFFER_DESC constant{};constant.ByteWidth=sizeof(SceneConstants);
    constant.Usage=D3D11_USAGE_DEFAULT;constant.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&constant,nullptr,&sceneBuffer)))return false;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;
    raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    if(FAILED(device->CreateRasterizerState(&raster,&rasterState)))return false;
    raster.DepthBias=300;raster.SlopeScaledDepthBias=1.5f;
    if(FAILED(device->CreateRasterizerState(&raster,&shadowRaster)))return false;
    D3D11_SAMPLER_DESC comparison{};
    comparison.Filter=D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    comparison.AddressU=comparison.AddressV=comparison.AddressW=D3D11_TEXTURE_ADDRESS_BORDER;
    comparison.BorderColor[0]=comparison.BorderColor[1]=comparison.BorderColor[2]=comparison.BorderColor[3]=1;
    comparison.ComparisonFunc=D3D11_COMPARISON_LESS_EQUAL;
    comparison.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(device->CreateSamplerState(&comparison,&shadowSampler)))return false;
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=FALSE;
    depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    if(FAILED(device->CreateDepthStencilState(&depth,&noDepth)))return false;
    depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    if(FAILED(device->CreateDepthStencilState(&depth,&readDepth)))return false;
    D3D11_BLEND_DESC blend{};blend.RenderTarget[0].BlendEnable=TRUE;
    blend.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;
    blend.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(FAILED(device->CreateBlendState(&blend,&alphaBlend)))return false;
    D3D11_SAMPLER_DESC sample{};sample.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sample.AddressU=sample.AddressV=sample.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
    sample.MaxLOD=D3D11_FLOAT32_MAX;
    return SUCCEEDED(device->CreateSamplerState(&sample,&sampler));
}
bool createShadowTargets(int size){
    ID3D11ShaderResourceView* empty=nullptr;
    context->PSSetShaderResources(4,1,&empty);
    release(shadowView);release(shadowDepth);release(shadowTexture);shadowSize=0;
    if(size==0)return true;
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width=texture.Height=UINT(size);texture.MipLevels=1;texture.ArraySize=1;
    texture.Format=DXGI_FORMAT_R32_TYPELESS;texture.SampleDesc.Count=1;
    texture.Usage=D3D11_USAGE_DEFAULT;
    texture.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    if(FAILED(device->CreateTexture2D(&texture,nullptr,&shadowTexture)))return false;
    D3D11_DEPTH_STENCIL_VIEW_DESC depth{};
    depth.Format=DXGI_FORMAT_D32_FLOAT;depth.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    if(FAILED(device->CreateDepthStencilView(shadowTexture,&depth,&shadowDepth)))return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format=DXGI_FORMAT_R32_FLOAT;view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels=1;
    if(FAILED(device->CreateShaderResourceView(shadowTexture,&view,&shadowView)))return false;
    shadowSize=size;return true;
}
void releaseTargets(){
    context->OMSetRenderTargets(0,nullptr,nullptr);
    ID3D11ShaderResourceView* nullView=nullptr;context->PSSetShaderResources(0,1,&nullView);
    release(hudView);release(hudTexture);release(depthView);release(depthTexture);release(target);
}
SceneConstants constantsForFrame(const camera::Pose& pose,float solar,float daylight){
    SceneConstants constants{};
    const auto& conditions=weather::current();
    float light=daylight*(1.0f-conditions.clouds*0.42f);
    XMVECTOR eye=XMVectorSet(pose.eye.x,pose.eye.y,pose.eye.z,1);
    XMVECTOR targetPoint=XMVectorSet(pose.target.x,pose.target.y,pose.target.z,1);
    // Movement, steering, and mouse yaw use the same right-handed view as the OpenGL build.
    XMMATRIX view=XMMatrixLookAtRH(eye,targetPoint,XMVectorSet(0,1,0,0));
    const float drawScales[]={0.65f,1.0f,1.5f};
    float drawScale=drawScales[std::clamp(ui::drawDistance,0,2)];
    XMMATRIX projection=XMMatrixPerspectiveFovRH(XMConvertToRadians(camera::fieldOfView()),
        float(bufferW)/bufferH,2.0f,1250.0f*drawScale);
    XMStoreFloat4x4(&constants.viewProjection,XMMatrixMultiply(view,projection));
    constants.sun={std::cos((gameHour-6)*PI/12),std::max(0.18f,std::abs(solar)),0.3f,
        0.28f+0.68f*light};
    XMVECTOR focus=XMVectorSet(player.x,0,player.z,1);
    XMVECTOR sunDirection=XMVector3Normalize(XMVectorSet(constants.sun.x,constants.sun.y,constants.sun.z,0));
    XMVECTOR lightEye=XMVectorAdd(focus,XMVectorScale(sunDirection,1400));
    XMMATRIX lightView=XMMatrixLookAtRH(lightEye,focus,XMVectorSet(0,1,0,0));
    XMMATRIX lightProjection=XMMatrixOrthographicRH(1800,1800,1,3300);
    XMStoreFloat4x4(&constants.shadowViewProjection,XMMatrixMultiply(lightView,lightProjection));
    constants.ambient={0.32f+0.42f*light,0,0,0};
    constants.fogColor={0.045f+0.52f*light,0.065f+0.68f*light,
        0.14f+0.74f*light,1};
    constants.fogColor.x=constants.fogColor.x*(1-conditions.clouds*0.4f)+
        0.40f*conditions.clouds*0.4f;
    constants.fogColor.y=constants.fogColor.y*(1-conditions.clouds*0.4f)+
        0.43f*conditions.clouds*0.4f;
    constants.fogColor.z=constants.fogColor.z*(1-conditions.clouds*0.4f)+
        0.47f*conditions.clouds*0.4f;
    constants.eye={pose.eye.x,pose.eye.y,pose.eye.z,float(ui::graphicsQuality)};
    float fogEnd=(ui::graphicsQuality==0?650.0f:ui::graphicsQuality==1?850.0f:1050.0f)*
        drawScale*conditions.visibility;
    constants.params={0,fogEnd*0.42f,fogEnd,
        shadowDepth&&ui::shadowQuality>0&&daylight>0.05f?1.0f:0.0f};
    return constants;
}
void captureIfRequested(){
    char file[MAX_PATH]{};
    if(!GetEnvironmentVariableA("MINICITY_CAPTURE",file,MAX_PATH))return;
    SetEnvironmentVariableA("MINICITY_CAPTURE",nullptr);
    ID3D11Texture2D* back=nullptr;
    if(FAILED(swapChain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&back))))return;
    D3D11_TEXTURE2D_DESC description{};back->GetDesc(&description);
    description.Usage=D3D11_USAGE_STAGING;description.BindFlags=0;
    description.CPUAccessFlags=D3D11_CPU_ACCESS_READ;description.MiscFlags=0;
    ID3D11Texture2D* staging=nullptr;
    if(SUCCEEDED(device->CreateTexture2D(&description,nullptr,&staging))){
        context->CopyResource(staging,back);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&mapped))){
            BITMAPFILEHEADER fileHeader{};BITMAPINFOHEADER imageHeader{};
            fileHeader.bfType=0x4D42;fileHeader.bfOffBits=sizeof(fileHeader)+sizeof(imageHeader);
            fileHeader.bfSize=fileHeader.bfOffBits+description.Width*description.Height*4;
            imageHeader.biSize=sizeof(imageHeader);imageHeader.biWidth=LONG(description.Width);
            imageHeader.biHeight=-LONG(description.Height);imageHeader.biPlanes=1;
            imageHeader.biBitCount=32;imageHeader.biCompression=BI_RGB;
            std::ofstream output(file,std::ios::binary);
            if(output){output.write(reinterpret_cast<const char*>(&fileHeader),sizeof(fileHeader));
                output.write(reinterpret_cast<const char*>(&imageHeader),sizeof(imageHeader));
                for(UINT row=0;row<description.Height;++row)
                    output.write(reinterpret_cast<const char*>(static_cast<const unsigned char*>(mapped.pData)+
                        size_t(row)*mapped.RowPitch),size_t(description.Width)*4);}
            context->Unmap(staging,0);
        }
    }
    release(staging);release(back);
}
}
bool initRenderer(){
    DXGI_SWAP_CHAIN_DESC description{};description.BufferCount=2;
    description.BufferDesc.Width=UINT(std::max(1,screenW));
    description.BufferDesc.Height=UINT(std::max(1,screenH));
    description.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    description.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow=win;description.SampleDesc.Count=1;
    description.Windowed=TRUE;description.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL requested=D3D_FEATURE_LEVEL_11_0,created{};
    HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,&requested,1,D3D11_SDK_VERSION,&description,
        &swapChain,&device,&created,&context);
    if(FAILED(result))result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,&requested,1,D3D11_SDK_VERSION,&description,
        &swapChain,&device,&created,&context);
    if(FAILED(result))return false;
    Gdiplus::GdiplusStartupInput startup;
    if(Gdiplus::GdiplusStartup(&gdiplusToken,&startup,nullptr)!=Gdiplus::Ok)return false;
    if(!createShaders()||!createStates()||!createTargets(std::max(1,screenW),std::max(1,screenH)))return false;
    std::wstring base=executableFolder();
    if(!loadTexture(base+L"\\assets\\texture_atlas.png",&textures[1]))return false;
    const wchar_t* materials[]={nullptr,nullptr,L"Bricks001",L"Concrete001",
        L"Bark001",L"Fabric001",L"Metal001",L"Asphalt001",L"Ground054",L"Grass001",nullptr};
    for(int i=2;i<10;++i){
        std::wstring path=base+L"\\assets\\materials\\"+materials[i];
        if(!loadTexture(path+L"_Color.jpg",&detailTextures[i])||
           !loadTexture(path+L"_NormalDX.jpg",&normalTextures[i]))return false;
    }
    if(!loadTexture(base+L"\\assets\\models\\baked\\marina\\MarinaFacade_NormalDX.png",
            &normalTextures[10]))return false;
    detailTextures[11]=detailTextures[3];detailTextures[11]->AddRef();
    normalTextures[11]=normalTextures[3];normalTextures[11]->AddRef();
    detailTextures[12]=detailTextures[3];detailTextures[12]->AddRef();
    normalTextures[12]=normalTextures[3];normalTextures[12]->AddRef();
    dx11::loadMeshes(base+L"\\assets\\models\\baked");
    return createStaticGeometry();
}
void render(){
    drawCalls=0;
    int width=std::max(1,screenW),height=std::max(1,screenH);
    if(width!=bufferW||height!=bufferH){releaseTargets();
        if(!createTargets(width,height))return;}
    dx11::buildScene(groups,models);
    vertices.clear();size_t starts[dx11::MATERIAL_GROUPS]{},counts[dx11::MATERIAL_GROUPS]{};
    for(int group=0;group<dx11::MATERIAL_GROUPS;++group){starts[group]=vertices.size();counts[group]=groups[group].size();
        vertices.insert(vertices.end(),groups[group].begin(),groups[group].end());}
    if(!growVertexBuffer(vertices.size()))return;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(vertexBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(dx11::Vertex));
    context->Unmap(vertexBuffer,0);
    if(!prepareInstances())return;
    float solar=std::sin((gameHour-6)*PI/12.0f);
    float daylight=std::clamp(solar*2.3f+0.42f,0.0f,1.0f);
    int requestedShadowSize=ui::shadowQuality==0?0:ui::shadowQuality==1?1024:2048;
    if(requestedShadowSize!=shadowSize&&!createShadowTargets(requestedShadowSize))createShadowTargets(0);
    Vec2 focus=previousPlayer*(1.0f-renderAlpha)+player*renderAlpha;
    camera::Pose pose=camera::compute(focus,playerY,rightMouse&&occupied<0&&!ui::paused(),occupied);
    SceneConstants constants=constantsForFrame(pose,solar,daylight);
    UINT stride=sizeof(dx11::Vertex),offset=0;
    context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(sceneVS,nullptr,0);
    context->VSSetConstantBuffers(0,1,&sceneBuffer);
    context->HSSetConstantBuffers(0,1,&sceneBuffer);
    context->DSSetConstantBuffers(0,1,&sceneBuffer);
    if(constants.params.w>0){
        ID3D11ShaderResourceView* empty=nullptr;
        context->PSSetShaderResources(4,1,&empty);
        context->OMSetRenderTargets(0,nullptr,shadowDepth);
        context->ClearDepthStencilView(shadowDepth,D3D11_CLEAR_DEPTH,1,0);
        D3D11_VIEWPORT shadowViewport{};
        shadowViewport.Width=shadowViewport.Height=float(shadowSize);
        shadowViewport.MaxDepth=1;
        context->RSSetViewports(1,&shadowViewport);
        context->RSSetState(shadowRaster);
        context->PSSetShader(nullptr,nullptr,0);
        context->PSSetSamplers(0,1,&sampler);
        SceneConstants shadowConstants=constants;
        shadowConstants.viewProjection=constants.shadowViewProjection;
        for(int group=0;group<dx11::MATERIAL_GROUPS;++group)if(counts[group]){
            setTessellation(group);
            shadowConstants.params.x=float(group);
            context->UpdateSubresource(sceneBuffer,0,nullptr,&shadowConstants,0,0);
            context->Draw(UINT(counts[group]),UINT(starts[group]));++drawCalls;
        }
        drawInstances(true,shadowConstants);
    }
    float clearColor[]={constants.fogColor.x,constants.fogColor.y,constants.fogColor.z,1};
    context->OMSetRenderTargets(1,&target,depthView);
    context->ClearRenderTargetView(target,clearColor);
    context->ClearDepthStencilView(depthView,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
    D3D11_VIEWPORT mainViewport{};mainViewport.Width=float(bufferW);mainViewport.Height=float(bufferH);
    mainViewport.MaxDepth=1;context->RSSetViewports(1,&mainViewport);
    context->RSSetState(rasterState);
    context->VSSetShader(sceneVS,nullptr,0);context->PSSetShader(scenePS,nullptr,0);
    context->VSSetConstantBuffers(0,1,&sceneBuffer);
    context->PSSetConstantBuffers(0,1,&sceneBuffer);
    context->PSSetSamplers(0,1,&sampler);
    context->PSSetSamplers(1,1,&shadowSampler);
    context->PSSetShaderResources(4,1,&shadowView);
    context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
    context->IASetInputLayout(inputLayout);
    context->VSSetShader(sceneVS,nullptr,0);
    for(int group=0;group<dx11::MATERIAL_GROUPS;++group){
        if(!counts[group]&&!staticCounts[group])continue;
        constants.params.x=float(group);
        context->UpdateSubresource(sceneBuffer,0,nullptr,&constants,0,0);
        setTessellation(group);
        ID3D11ShaderResourceView* baseTexture=group==1?textures[1]:nullptr;
        ID3D11ShaderResourceView* resources[4]={baseTexture,detailTextures[group],normalTextures[group],detailTextures[9]};
        context->PSSetShaderResources(0,4,resources);
        if(staticCounts[group]){
            context->IASetVertexBuffers(0,1,&staticBuffer,&stride,&offset);
            context->Draw(UINT(staticCounts[group]),UINT(staticStarts[group]));++drawCalls;
        }
        if(counts[group]){
            context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
            context->Draw(UINT(counts[group]),UINT(starts[group]));++drawCalls;
        }
    }
    drawInstances(false,constants);
    float effectBlend[4]{0,0,0,0};
    context->OMSetDepthStencilState(readDepth,0);
    context->OMSetBlendState(alphaBlend,effectBlend,0xffffffffu);
    drawInstances(false,constants,true);
    context->OMSetBlendState(nullptr,effectBlend,0xffffffffu);
    context->OMSetDepthStencilState(nullptr,0);
    setTessellation(-1);
    dx11::buildHud(hudPixels.data(),bufferW,bufferH);
    if(SUCCEEDED(context->Map(hudTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){
        for(int row=0;row<bufferH;++row)
            std::memcpy(static_cast<unsigned char*>(mapped.pData)+size_t(row)*mapped.RowPitch,
                hudPixels.data()+size_t(row)*bufferW*4,size_t(bufferW)*4);
        context->Unmap(hudTexture,0);
        context->OMSetDepthStencilState(noDepth,0);
        float blend[4]{0,0,0,0};context->OMSetBlendState(alphaBlend,blend,0xffffffffu);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->VSSetShader(hudVS,nullptr,0);context->PSSetShader(hudPS,nullptr,0);
        context->PSSetShaderResources(0,1,&hudView);
        context->Draw(4,0);++drawCalls;
        context->OMSetBlendState(nullptr,blend,0xffffffffu);
        context->OMSetDepthStencilState(nullptr,0);
    }
    captureIfRequested();
    swapChain->Present(1,0);
}
void shutdownRenderer(){
    dx11::shutdownHud();
    if(context)context->ClearState();
    releaseTargets();
    release(shadowView);release(shadowDepth);release(shadowTexture);
    for(auto& texture:textures)release(texture);
    for(auto& texture:detailTextures)release(texture);
    for(auto& texture:normalTextures)release(texture);
    release(sampler);release(shadowSampler);release(alphaBlend);release(readDepth);release(noDepth);
    release(rasterState);release(shadowRaster);
    release(sceneBuffer);release(vertexBuffer);release(staticBuffer);release(inputLayout);
    release(instanceBuffer);release(instanceLayout);
    for(auto& item:meshBuffers)release(item.second);
    meshBuffers.clear();
    for(auto& item:modelTextures)release(item.second);
    modelTextures.clear();
    sharedModelTextures.clear();
    release(sceneVS);release(instanceVS);release(scenePS);release(alphaShadowPS);release(sceneHS);release(sceneDS);
    release(hudVS);release(hudPS);
    release(swapChain);release(context);release(device);
    if(gdiplusToken){Gdiplus::GdiplusShutdown(gdiplusToken);gdiplusToken=0;}
}
}
