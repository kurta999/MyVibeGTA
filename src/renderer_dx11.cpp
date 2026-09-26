#ifndef NOMINMAX
#define NOMINMAX
#endif
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
#include "regions.h"
#include "commerce.h"
#include "fire.h"
#include "logging.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cwchar>
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
ID3D11Texture2D* sceneTexture=nullptr;
ID3D11RenderTargetView* sceneTarget=nullptr;
ID3D11ShaderResourceView* sceneView=nullptr;
ID3D11Texture2D* depthTexture=nullptr;
ID3D11DepthStencilView* depthView=nullptr;
ID3D11ShaderResourceView* depthViewSRV=nullptr;
ID3D11VertexShader* sceneVS=nullptr;
ID3D11VertexShader* instanceVS=nullptr;
ID3D11PixelShader* scenePS=nullptr;
ID3D11PixelShader* alphaShadowPS=nullptr;
ID3D11HullShader* sceneHS=nullptr;
ID3D11DomainShader* sceneDS=nullptr;
ID3D11VertexShader* hudVS=nullptr;
ID3D11PixelShader* hudPS=nullptr;
ID3D11PixelShader* postPS=nullptr;
ID3D11Buffer* postBuffer=nullptr;
ID3D11InputLayout* inputLayout=nullptr;
ID3D11InputLayout* instanceLayout=nullptr;
ID3D11Buffer* vertexBuffer=nullptr;
ID3D11Buffer* staticBuffer=nullptr;
ID3D11Buffer* instanceBuffer=nullptr;
std::unordered_map<const dx11::Mesh*,ID3D11Buffer*> meshBuffers;
std::unordered_map<const dx11::Mesh*,ID3D11ShaderResourceView*> modelTextures;
std::unordered_map<std::wstring,ID3D11ShaderResourceView*> sharedModelTextures;
std::unordered_map<std::wstring,ID3D11ShaderResourceView*> pbrTextures;
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
ID3D11BlendState* hudBlend=nullptr;
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
unsigned int screenshotSequence=0;
struct SceneConstants {
    XMFLOAT4X4 viewProjection;
    XMFLOAT4X4 shadowViewProjection;
    XMFLOAT4 sun;
    XMFLOAT4 ambient;
    XMFLOAT4 fogColor;
    XMFLOAT4 eye;
    XMFLOAT4 params;
    XMFLOAT4 weatherAndTime;
    XMFLOAT4 materialPbr;
    XMFLOAT4 localLightPosition[12];
    XMFLOAT4 localLightColor[12];
};
struct PostConstants {XMFLOAT4X4 viewProjection,inverseViewProjection;
    XMFLOAT4 cameraEye,pixelSize,grade,effects,skyTop,skyHorizon;};
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
    if(FAILED(status)&&errors){
        std::ofstream log("shader-error.log",std::ios::app);
        if(log)log<<entry<<": "<<static_cast<const char*>(errors->GetBufferPointer())<<'\n';
        if(std::strstr(GetCommandLineA(),"--smoke")==nullptr)
            MessageBoxA(win,static_cast<const char*>(errors->GetBufferPointer()),
                "Direct3D shader compile error",MB_ICONERROR);
    }
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
    float4 weatherAndTime;
    float4 materialPbr;
    float4 localLightPosition[12];
    float4 localLightColor[12];
};
Texture2D diffuseTexture : register(t0);
Texture2D detailTexture : register(t1);
Texture2D normalTexture : register(t2);
Texture2D foliageTexture : register(t3);
Texture2D shadowTexture : register(t4);
Texture2D modelNormalTexture : register(t5);
Texture2D modelOrmTexture : register(t6);
Texture2D modelOcclusionTexture : register(t7);
Texture2D modelEmissiveTexture : register(t8);
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
    if(material==0||material==3||material==7||material==8||material==9||material==11||material==15)
        base.rgb=pow(saturate(base.rgb),2.2);
    if(material==1||modelTexture)
        base*=diffuseTexture.Sample(linearSampler,input.uv);
    if(modelTexture&&material!=13){
        float threshold=material==4?0.42:0.35;
        clip(base.a-threshold);
    }
    if(material==4&&modelTexture&&base.g>base.r*1.18){
        float nearCamera=distance(input.world,eye.xyz);
        float keep=smoothstep(11.0,34.0,nearCamera);
        float stableNoise=frac(sin(dot(floor(input.world.xz*0.72),
            float2(12.9898,78.233)))*43758.5453);
        clip(keep-stableNoise);
    }
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
    }else if(material>=2&&material!=3){
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
    float precipitation=weatherAndTime.x;
    if(material==3){
        float2 wave=input.world.xz*0.055+weatherAndTime.w*0.35;
        normal=normalize(float3(0.055*sin(wave.x+wave.y*0.7),1.0,
            0.055*cos(wave.y-wave.x*0.6)));
    }
    float snow=weatherAndTime.y;
    float night=weatherAndTime.z;
    if(snow>0&&input.world.y<220&&normal.y>0.55&&material!=13&&material!=14){
        float noise=frac(sin(dot(input.world.xz,float2(0.074,0.113)))*43758.5453);
        float cover=snow*smoothstep(0.52,0.78,normal.y)*
            smoothstep(0.25,0.65,noise);
        base.rgb=lerp(base.rgb,float3(0.83,0.90,0.96),cover*0.82);
    }
    int mapFlags=(int)(materialPbr.w+0.5);
    if((mapFlags&1)!=0){
        float3 mapped=modelNormalTexture.Sample(linearSampler,input.uv).xyz*2-1;
        float3 dpdx=ddx(input.world),dpdy=ddy(input.world);
        float2 duvdx=ddx(input.uv),duvdy=ddy(input.uv);
        float determinant=duvdx.x*duvdy.y-duvdx.y*duvdy.x;
        if(abs(determinant)>0.00001){
            float3 tangent=normalize((dpdx*duvdy.y-dpdy*duvdx.y)/determinant);
            float3 bitangent=normalize((dpdy*duvdx.x-dpdx*duvdy.x)/determinant);
            normal=normalize(tangent*mapped.x+bitangent*mapped.y+normal*mapped.z);
        }
    }
    float roughness=material==3?0.09:clamp(materialPbr.x,0.06,1.0);
    float metallic=saturate(materialPbr.y);
    if((mapFlags&2)!=0){
        float4 orm=modelOrmTexture.Sample(linearSampler,input.uv);
        roughness=clamp(roughness*orm.g,0.06,1.0);
        metallic*=orm.b;
    }
    bool road=(material==7||material==15)&&normal.y>0.8;
    if(road&&precipitation>0.05){
        float puddle=sin(input.world.x*0.091+sin(input.world.z*0.047))*
            sin(input.world.z*0.083+sin(input.world.x*0.063));
        puddle=material==15?precipitation:
            smoothstep(0.22,0.55,puddle)*precipitation;
        base.rgb*=1-0.30*precipitation-0.12*puddle;
        roughness=lerp(roughness,0.10,puddle*0.9+precipitation*0.22);
    }
    float3 viewDirection=normalize(eye.xyz-input.world);
    float ndv=max(0.001,dot(normal,viewDirection));
    float3 f0=lerp(float3(0.04,0.04,0.04),base.rgb,metallic);
    float occlusion=(mapFlags&4)!=0?
        modelOcclusionTexture.Sample(linearSampler,input.uv).r:1.0;
    float3 lit=(base.rgb*ambient.rgb*(1-metallic)*0.75+
        f0*ambient.rgb*0.15)*occlusion;
    float diffuse=saturate(dot(normal,normalize(sun.xyz)));
    float visibility=1;
    if(params.w>0.5&&sun.w>0.4&&input.shadowPosition.w>0){
        float3 projected=input.shadowPosition.xyz/input.shadowPosition.w;
        float2 uv=float2(projected.x*0.5+0.5,0.5-projected.y*0.5);
        if(all(uv>=0)&&all(uv<=1)&&projected.z>0&&projected.z<1){
            float bias=max(0.00018,0.0012*(1-diffuse));
            uint shadowWidth,shadowHeight;
            shadowTexture.GetDimensions(shadowWidth,shadowHeight);
            float2 texel=1.0/float2(shadowWidth,shadowHeight);
            float filtered=0;
            [unroll] for(int sy=0;sy<2;++sy)
                [unroll] for(int sx=0;sx<2;++sx)
                    filtered+=shadowTexture.SampleCmpLevelZero(shadowSampler,
                        uv+(float2(sx,sy)-0.5)*texel*1.6,projected.z-bias);
            visibility=lerp(0.24,1.0,filtered*0.25);
        }
    }
    float3 lightDirection=normalize(sun.xyz);
    float3 halfVector=normalize(viewDirection+lightDirection);
    float ndl=max(0,dot(normal,lightDirection));
    float ndh=max(0,dot(normal,halfVector));
    float vdh=max(0,dot(viewDirection,halfVector));
    float alpha=roughness*roughness;
    float alpha2=alpha*alpha;
    float denominator=ndh*ndh*(alpha2-1)+1;
    float distribution=alpha2/(3.14159*denominator*denominator+0.001);
    float k=(roughness+1)*(roughness+1)/8;
    float geometry=(ndv/(ndv*(1-k)+k))*(ndl/(ndl*(1-k)+k));
    float3 fresnel=f0+(1-f0)*pow(1-vdh,5);
    float3 brdf=(1-fresnel)*(1-metallic)*base.rgb/3.14159+
        distribution*geometry*fresnel/(4*ndv*max(0.001,ndl));
    float3 sunTint=lerp(float3(1.0,0.61,0.35),float3(1.0,0.98,0.93),
        saturate(abs(sun.y-0.18)*2.5));
    lit+=brdf*sunTint*sun.w*ndl*visibility*2.5;
    [unroll] for(int i=0;i<12;++i){
        float3 delta=localLightPosition[i].xyz-input.world;
        float radius=localLightPosition[i].w;
        float distanceToLight=length(delta);
        if(radius<1||distanceToLight>=radius)continue;
        float3 direction=delta/max(distanceToLight,0.001);
        float attenuation=pow(saturate(1-distanceToLight/radius),2);
        float diffuseLocal=max(0,dot(normal,direction));
        float3 halfLocal=normalize(viewDirection+direction);
        float localSpecular=pow(max(0,dot(normal,halfLocal)),
            lerp(80.0,5.0,roughness));
        lit+=(base.rgb*(1-metallic)*diffuseLocal*0.9+
            f0*localSpecular*2.0)*localLightColor[i].rgb*
            localLightColor[i].w*attenuation;
    }
    if(road&&precipitation>0.05){
        float fresnelWet=pow(1-ndv,3);
        lit+=fogColor.rgb*(0.08+0.22*fresnelWet)*precipitation;
    }
    if(material==6&&ambient.w>0.5){
        float3 reflected=reflect(-viewDirection,normal);
        float horizon=saturate(reflected.y*0.5+0.5);
        float3 environment=lerp(fogColor.rgb*0.65,fogColor.rgb*1.45,horizon);
        float fresnelPaint=0.08+0.42*pow(1-ndv,3);
        lit+=environment*fresnelPaint*(1-roughness)*
            (ambient.w>1.5?1.0:0.65);
    }
    lit+=((mapFlags&8)!=0?modelEmissiveTexture.Sample(linearSampler,input.uv).rgb:
        base.rgb)*materialPbr.z;
    float distanceToEye=distance(input.world,eye.xyz);
    float fog=saturate((distanceToEye-params.y)/max(1,params.z-params.y));
    float reflectionMask=material==3?0.10:
        road&&precipitation>0.05?1.0-0.78*precipitation:base.a;
    return float4(lerp(lit,fogColor.rgb,fog),reflectionMask);
}
void PSShadowAlpha(Output input){
    float4 base=diffuseTexture.Sample(linearSampler,input.uv);
    clip(base.a-0.42);
    if(base.g>base.r*1.18){
        float keep=smoothstep(11.0,34.0,distance(input.world,eye.xyz));
        float stableNoise=frac(sin(dot(floor(input.world.xz*0.72),
            float2(12.9898,78.233)))*43758.5453);
        clip(keep-stableNoise);
    }
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
const char* postShader=R"HLSL(
cbuffer Post : register(b0){
    row_major float4x4 viewProjection;
    row_major float4x4 inverseViewProjection;
    float4 cameraEye;float4 pixelSize;float4 grade;float4 effects;
    float4 skyTop;float4 skyHorizon;
};
Texture2D sceneColor : register(t0);
Texture2D sceneDepth : register(t1);
SamplerState linearSampler : register(s0);
struct Input {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
float luminance(float3 c){return dot(c,float3(0.2126,0.7152,0.0722));}
float linearDepth(float d){return pixelSize.z*pixelSize.w/
    max(0.001,pixelSize.w-d*(pixelSize.w-pixelSize.z));}
float3 sampleColor(float2 uv){return sceneColor.SampleLevel(linearSampler,saturate(uv),0).rgb;}
float4 PS(Input input):SV_TARGET{
    float2 uv=input.uv,stepSize=pixelSize.xy;
    float3 center=sampleColor(uv);
    if(effects.z>0.5){
        float lumaM=luminance(center);
        float lumaNW=luminance(sampleColor(uv+float2(-1,-1)*stepSize));
        float lumaNE=luminance(sampleColor(uv+float2(1,-1)*stepSize));
        float lumaSW=luminance(sampleColor(uv+float2(-1,1)*stepSize));
        float lumaSE=luminance(sampleColor(uv+float2(1,1)*stepSize));
        float lumaMin=min(lumaM,min(min(lumaNW,lumaNE),min(lumaSW,lumaSE)));
        float lumaMax=max(lumaM,max(max(lumaNW,lumaNE),max(lumaSW,lumaSE)));
        if(lumaMax-lumaMin>max(0.04,lumaMax*0.12)){
            float2 dir=float2(-((lumaNW+lumaNE)-(lumaSW+lumaSE)),
                (lumaNW+lumaSW)-(lumaNE+lumaSE));
            float reduce=max((lumaNW+lumaNE+lumaSW+lumaSE)*0.25*0.125,0.0078125);
            dir=clamp(dir/(min(abs(dir.x),abs(dir.y))+reduce),-8.0,8.0)*stepSize;
            float3 a=0.5*(sampleColor(uv+dir*(-1.0/6.0))+
                sampleColor(uv+dir*(1.0/6.0)));
            if(effects.z>1.5){
                float3 b=a*0.5+0.25*(sampleColor(uv+dir*(-0.5))+
                    sampleColor(uv+dir*0.5));
                float lumaB=luminance(b);
                center=lumaB<lumaMin||lumaB>lumaMax?a:b;
            }else center=lerp(center,a,0.72);
        }
    }
    float4 surface=sceneColor.SampleLevel(linearSampler,uv,0);
    float d=sceneDepth.SampleLevel(linearSampler,uv,0).r;
    if(effects.w>0.5&&surface.a<0.95&&d<0.9999){
        float4 clip=float4(uv.x*2-1,1-uv.y*2,d,1);
        float4 world=mul(clip,inverseViewProjection);
        world.xyz/=world.w;
        float3 incoming=normalize(world.xyz-cameraEye.xyz);
        float3 reflected=reflect(incoming,float3(0,1,0));
        float3 reflection=skyHorizon.rgb;
        bool hit=false;
        int steps=effects.w>1.5?24:10;
        [loop] for(int i=0;i<steps;++i){
            float3 ray=world.xyz+reflected*(6+i*(effects.w>1.5?9:17));
            float4 projected=mul(float4(ray,1),viewProjection);
            float3 ndc=projected.xyz/max(projected.w,0.001);
            float2 rayUv=float2(ndc.x*0.5+0.5,0.5-ndc.y*0.5);
            if(any(rayUv<0)||any(rayUv>1)||ndc.z<=0||ndc.z>=1)break;
            float hitDepth=sceneDepth.SampleLevel(linearSampler,rayUv,0).r;
            if(hitDepth<0.9999&&
               linearDepth(hitDepth)+1.5<linearDepth(ndc.z)){
                reflection=sampleColor(rayUv);
                hit=true;break;
            }
        }
        center=lerp(center,reflection,(hit?0.66:0.40)*(1-surface.a));
    }
    if(d>=0.9999){
        float4 farPoint=mul(float4(uv.x*2-1,1-uv.y*2,1,1),inverseViewProjection);
        float3 ray=normalize(farPoint.xyz/farPoint.w-cameraEye.xyz);
        float gradient=saturate(0.34+ray.y*1.8);
        center=lerp(skyHorizon.rgb,skyTop.rgb,gradient);
        float drift=skyHorizon.w*0.018;
        float2 cloudUv=ray.xz/max(0.14,ray.y+0.12);
        float vapor=0.5+0.24*sin(cloudUv.x*8+cloudUv.y*3+drift)+
            0.16*sin(cloudUv.x*17-cloudUv.y*11-drift*0.6)+
            0.10*sin(cloudUv.x*29+cloudUv.y*19+drift*1.3);
        float cloud=smoothstep(0.57,0.77,vapor)*
            saturate((ray.y+0.03)*5.0)*(0.16+skyTop.w*0.46);
        float cloudDay=saturate((skyTop.b-0.09)/0.55);
        center=lerp(center,lerp(float3(0.12,0.15,0.21),
            float3(0.82,0.86,0.88),cloudDay),cloud);
        float ridge=0.006+0.013*sin(ray.x*19+ray.z*7)+
            0.008*sin(ray.x*41-ray.z*23);
        center=lerp(center,skyHorizon.rgb*0.66,
            1-smoothstep(ridge,ridge+0.014,ray.y));
    }
    if(effects.x>0.5&&d<0.9999){
        float z=linearDepth(d),occlusion=0;
        float2 taps[16]={float2(-1,0),float2(1,0),float2(0,-1),float2(0,1),
            float2(-0.7,-0.7),float2(0.7,-0.7),float2(-0.7,0.7),float2(0.7,0.7),
            float2(-2,0),float2(2,0),float2(0,-2),float2(0,2),
            float2(-1.4,-1.4),float2(1.4,-1.4),float2(-1.4,1.4),float2(1.4,1.4)};
        int tapCount=effects.x>1.5?16:8;
        [loop] for(int i=0;i<tapCount;++i){
            float2 at=uv+taps[i]*stepSize*5;
            float neighbor=linearDepth(sceneDepth.SampleLevel(linearSampler,saturate(at),0).r);
            float difference=z-neighbor;
            occlusion+=smoothstep(0.45,8.0,difference)*
                (1-smoothstep(14.0,42.0,difference));
        }
        center*=1-(effects.x>1.5?0.52:0.35)*occlusion/tapCount;
    }
    if(effects.y>0.01){
        float3 glow=0;
        float2 taps[8]={float2(-1,0),float2(1,0),float2(0,-1),float2(0,1),
            float2(-0.7,-0.7),float2(0.7,-0.7),float2(-0.7,0.7),float2(0.7,0.7)};
        [unroll] for(int i=0;i<8;++i){
            float3 c=sampleColor(uv+taps[i]*stepSize*8);
            glow+=c*max(0,luminance(c)-1.05);
        }
        center+=glow*(effects.y/8.0);
    }
    float3 color=max(0,center*grade.w);
    color=color/(1+color);
    color=pow(saturate(color*grade.rgb),1.0/2.2);
    return float4(color,1);
}
)HLSL";
bool createShaders(){
    ID3DBlob *vs=nullptr,*instanced=nullptr,*ps=nullptr,*shadowAlpha=nullptr,*hull=nullptr,*domain=nullptr,*hudVertex=nullptr,*hudPixel=nullptr,*postPixel=nullptr;
    if(!compile(sceneShader,"VS","vs_5_0",&vs)||!compile(sceneShader,"PS","ps_5_0",&ps)||
       !compile(sceneShader,"VSInstanced","vs_5_0",&instanced)||
       !compile(sceneShader,"PSShadowAlpha","ps_5_0",&shadowAlpha)||
       !compile(sceneShader,"HS","hs_5_0",&hull)||!compile(sceneShader,"DS","ds_5_0",&domain)||
       !compile(hudShader,"VS","vs_5_0",&hudVertex)||!compile(hudShader,"PS","ps_5_0",&hudPixel)||
       !compile(postShader,"PS","ps_5_0",&postPixel)){
        release(vs);release(instanced);release(ps);release(shadowAlpha);release(hull);release(domain);
        release(hudVertex);release(hudPixel);release(postPixel);return false;}
    HRESULT result=device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&sceneVS);
    if(SUCCEEDED(result))result=device->CreateVertexShader(instanced->GetBufferPointer(),instanced->GetBufferSize(),nullptr,&instanceVS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&scenePS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(shadowAlpha->GetBufferPointer(),shadowAlpha->GetBufferSize(),nullptr,&alphaShadowPS);
    if(SUCCEEDED(result))result=device->CreateHullShader(hull->GetBufferPointer(),hull->GetBufferSize(),nullptr,&sceneHS);
    if(SUCCEEDED(result))result=device->CreateDomainShader(domain->GetBufferPointer(),domain->GetBufferSize(),nullptr,&sceneDS);
    if(SUCCEEDED(result))result=device->CreateVertexShader(hudVertex->GetBufferPointer(),hudVertex->GetBufferSize(),nullptr,&hudVS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(hudPixel->GetBufferPointer(),hudPixel->GetBufferSize(),nullptr,&hudPS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(postPixel->GetBufferPointer(),postPixel->GetBufferSize(),nullptr,&postPS);
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
    release(hudVertex);release(hudPixel);release(postPixel);
    return SUCCEEDED(result);
}
bool loadTexture(const std::wstring& file,ID3D11ShaderResourceView** view,bool srgb=true){
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
    description.MipLevels=1;description.ArraySize=1;
    description.Format=srgb?DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count=1;description.Usage=D3D11_USAGE_IMMUTABLE;
    description.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=pixels.data();initial.SysMemPitch=width*4;
    ID3D11Texture2D* texture=nullptr;
    HRESULT status=device->CreateTexture2D(&description,&initial,&texture);
    if(SUCCEEDED(status))status=device->CreateShaderResourceView(texture,nullptr,view);
    release(texture);return SUCCEEDED(status);
}
bool cachePbrTexture(const std::wstring& file,bool srgb=true){
    if(file.empty()||pbrTextures.count(file))return true;
    ID3D11ShaderResourceView* view=nullptr;
    if(!loadTexture(file,&view,srgb))return false;
    pbrTextures.emplace(file,view);
    return true;
}
ID3D11ShaderResourceView* pbrTexture(const std::wstring& file){
    auto found=pbrTextures.find(file);
    return found==pbrTextures.end()?nullptr:found->second;
}
bool createTargets(int width,int height){
    if(width<1||height<1)return false;
    if(bufferW&&FAILED(swapChain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0)))return false;
    ID3D11Texture2D* backBuffer=nullptr;
    HRESULT result=swapChain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&backBuffer));
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(backBuffer,nullptr,&target);
    release(backBuffer);if(FAILED(result))return false;
    D3D11_TEXTURE2D_DESC sceneDescription{};
    sceneDescription.Width=width;sceneDescription.Height=height;sceneDescription.MipLevels=1;
    sceneDescription.ArraySize=1;sceneDescription.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    sceneDescription.SampleDesc.Count=1;sceneDescription.Usage=D3D11_USAGE_DEFAULT;
    sceneDescription.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    result=device->CreateTexture2D(&sceneDescription,nullptr,&sceneTexture);
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(sceneTexture,nullptr,&sceneTarget);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(sceneTexture,nullptr,&sceneView);
    if(FAILED(result))return false;
    D3D11_TEXTURE2D_DESC depthDescription{};
    depthDescription.Width=width;depthDescription.Height=height;depthDescription.MipLevels=1;
    depthDescription.ArraySize=1;depthDescription.Format=DXGI_FORMAT_R24G8_TYPELESS;
    depthDescription.SampleDesc.Count=1;depthDescription.Usage=D3D11_USAGE_DEFAULT;
    depthDescription.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    result=device->CreateTexture2D(&depthDescription,nullptr,&depthTexture);
    D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc{};
    depthDesc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    if(SUCCEEDED(result))result=device->CreateDepthStencilView(depthTexture,&depthDesc,&depthView);
    D3D11_SHADER_RESOURCE_VIEW_DESC depthResource{};
    depthResource.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthResource.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
    depthResource.Texture2D.MipLevels=1;
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(depthTexture,&depthResource,&depthViewSRV);
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
bool cacheModel(const dx11::Mesh* source){
    if(meshBuffers.find(source)==meshBuffers.end()){
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth=UINT(source->vertices.size()*sizeof(dx11::Vertex));
        desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=source->vertices.data();
        ID3D11Buffer* buffer=nullptr;
        if(FAILED(device->CreateBuffer(&desc,&data,&buffer)))return false;
        meshBuffers.emplace(source,buffer);
    }
    if(source->materialRanges.empty()&&!source->textureFile.empty()&&
       modelTextures.find(source)==modelTextures.end()){
        ID3D11ShaderResourceView* texture=nullptr;
        auto cached=sharedModelTextures.find(source->textureFile);
        if(cached!=sharedModelTextures.end()){
            texture=cached->second;texture->AddRef();
        }else{
            if(!loadTexture(source->textureFile,&texture))return false;
            sharedModelTextures.emplace(source->textureFile,texture);
        }
        modelTextures.emplace(source,texture);
    }
    for(const auto& range:source->materialRanges)
        if(!cachePbrTexture(range.baseFile)||!cachePbrTexture(range.normalFile,false)||
           !cachePbrTexture(range.ormFile,false)||!cachePbrTexture(range.occlusionFile,false)||
           !cachePbrTexture(range.emissiveFile))return false;
    return true;
}
bool prepareInstances(){
    std::sort(models.begin(),models.end(),[](const auto& a,const auto& b){
        if(a.material!=b.material)return a.material<b.material;
        return std::less<const dx11::Mesh*>{}(a.source,b.source);
    });
    instanceData.clear();instanceBatches.clear();
    for(const auto& model:models){
        if(!cacheModel(model.source))return false;
        if(model.source->shadowProxy&&!cacheModel(model.source->shadowProxy))return false;
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
        (material==2||material==4||material==10||material==11);
    context->IASetPrimitiveTopology(enabled?D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->HSSetShader(enabled?sceneHS:nullptr,nullptr,0);
    context->DSSetShader(enabled?sceneDS:nullptr,nullptr,0);
}
XMFLOAT4 pbrForGroup(int group){
    switch(group){
    case 2:case 3:case 10:case 11:case 12:return {0.82f,0.02f,0,0};
    case 4:return {0.92f,0,0,0};
    case 5:return {0.88f,0,0,0};
    case 6:return {0.37f,0.70f,0,0};
    case 7:return {0.84f,0.02f,0,0};
    case 15:return {0.12f,0.0f,0,0};
    case 8:case 9:return {0.96f,0,0,0};
    case 14:return {0.38f,0,1.7f,0};
    default:return {0.8f,0,0,0};
    }
}
void drawInstances(bool shadow,SceneConstants& constants,bool transparent=false){
    context->IASetInputLayout(instanceLayout);
    context->VSSetShader(instanceVS,nullptr,0);
    for(const auto& batch:instanceBatches){
        if(batch.mesh->transparent!=transparent)continue;
        if(shadow&&!batch.mesh->castsShadow)continue;
        const dx11::Mesh* drawn=shadow&&batch.mesh->shadowProxy?
            batch.mesh->shadowProxy:batch.mesh;
        // Imported foliage already has dense leaf geometry. Hull/domain
        // tessellation multiplies its cost without improving the silhouette.
        setTessellation(drawn->textured&&batch.material==4?0:batch.material);
        ID3D11Buffer* buffers[]={meshBuffers.at(drawn),instanceBuffer};
        UINT strides[]={sizeof(dx11::Vertex),sizeof(InstanceData)},offsets[]={0,0};
        context->IASetVertexBuffers(0,2,buffers,strides,offsets);
        const auto& ranges=drawn->materialRanges;
        for(size_t part=0;part<std::max<size_t>(1,ranges.size());++part){
            const dx11::MaterialRange* range=ranges.empty()?nullptr:&ranges[part];
            auto texture=modelTextures.find(drawn);
            ID3D11ShaderResourceView* base=range?pbrTexture(range->baseFile):
                texture==modelTextures.end()?nullptr:texture->second;
            bool hasModelTexture=base!=nullptr;
            constants.params.x=float(batch.material)+(hasModelTexture?0.25f:0.0f);
            constants.materialPbr=pbrForGroup(batch.material);
            if(range){
                constants.materialPbr.x=range->roughness;
                constants.materialPbr.y=range->metallic;
                constants.materialPbr.z=range->emissive;
                constants.materialPbr.w=float((!range->normalFile.empty()?1:0)|
                    (!range->ormFile.empty()?2:0)|
                    (!range->occlusionFile.empty()?4:0)|
                    (!range->emissiveFile.empty()?8:0));
            }else if(batch.material!=14&&drawn->textured){
                constants.materialPbr.x=drawn->roughness;
                constants.materialPbr.y=drawn->metallic;
            }
            context->UpdateSubresource(sceneBuffer,0,nullptr,&constants,0,0);
            if(shadow){
                bool alpha=drawn->alphaTest&&hasModelTexture;
                context->PSSetShader(alpha?alphaShadowPS:nullptr,nullptr,0);
                if(alpha)context->PSSetShaderResources(0,1,&base);
            }else{
                int group=batch.material;
                ID3D11ShaderResourceView* resources[9]={base,
                    group<dx11::MATERIAL_GROUPS?detailTextures[group]:nullptr,
                    group<dx11::MATERIAL_GROUPS?normalTextures[group]:nullptr,
                    detailTextures[9],nullptr,
                    range?pbrTexture(range->normalFile):nullptr,
                    range?pbrTexture(range->ormFile):nullptr,
                    range?pbrTexture(range->occlusionFile):nullptr,
                    range?pbrTexture(range->emissiveFile):nullptr};
                context->PSSetShaderResources(0,4,resources);
                context->PSSetShaderResources(5,4,resources+5);
            }
            context->DrawInstanced(range?range->count:UINT(drawn->vertices.size()),
                batch.count,range?range->start:0,batch.start);
            ++drawCalls;
        }
    }
}
bool createStates(){
    D3D11_BUFFER_DESC constant{};constant.ByteWidth=sizeof(SceneConstants);
    constant.Usage=D3D11_USAGE_DEFAULT;constant.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&constant,nullptr,&sceneBuffer)))return false;
    constant.ByteWidth=sizeof(PostConstants);
    if(FAILED(device->CreateBuffer(&constant,nullptr,&postBuffer)))return false;
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
    // Scene alpha stores the reflection mask. Transparent effects must keep
    // the mask beneath them instead of turning their soft edges reflective.
    blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ZERO;
    blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ONE;
    blend.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(FAILED(device->CreateBlendState(&blend,&alphaBlend)))return false;
    blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
    if(FAILED(device->CreateBlendState(&blend,&hudBlend)))return false;
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
    ID3D11ShaderResourceView* nullViews[2]{};context->PSSetShaderResources(0,2,nullViews);
    release(hudView);release(hudTexture);release(depthViewSRV);release(depthView);
    release(depthTexture);release(sceneView);release(sceneTarget);release(sceneTexture);release(target);
}
SceneConstants constantsForFrame(const camera::Pose& pose,float solar,float daylight){
    SceneConstants constants{};
    const auto& conditions=weather::current();
    float light=daylight*(1.0f-conditions.clouds*0.42f);
    XMVECTOR eye=XMVectorSet(pose.eye.x,pose.eye.y,pose.eye.z,1);
    XMVECTOR targetPoint=XMVectorSet(pose.target.x,pose.target.y,pose.target.z,1);
    // Movement, steering, and mouse yaw use the same right-handed view as the OpenGL build.
    XMMATRIX view=XMMatrixLookAtRH(eye,targetPoint,XMVectorSet(0,1,0,0));
    float drawScale=ui::drawDistanceScale();
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
    const float night=1.0f-daylight;
    const float twilight=std::max(0.0f,1.0f-std::abs(solar)*3.5f);
    constants.ambient={0.18f+0.36f*light,0.21f+0.35f*light,
        0.27f+0.34f*light,0};
    constants.fogColor={0.045f+0.52f*light,0.065f+0.68f*light,
        0.14f+0.74f*light,1};
    constants.fogColor.x=constants.fogColor.x*(1-conditions.clouds*0.4f)+
        0.40f*conditions.clouds*0.4f;
    constants.fogColor.y=constants.fogColor.y*(1-conditions.clouds*0.4f)+
        0.43f*conditions.clouds*0.4f;
    constants.fogColor.z=constants.fogColor.z*(1-conditions.clouds*0.4f)+
        0.47f*conditions.clouds*0.4f;
    constants.fogColor.x+=twilight*0.20f;
    constants.fogColor.y+=twilight*0.055f;
    constants.fogColor.z-=twilight*0.045f;
    const auto biome=regions::biomeAt(player);
    if(biome==regions::Biome::Snow){
        constants.fogColor.x+=0.045f;constants.fogColor.y+=0.075f;
        constants.fogColor.z+=0.10f;
    }else if(biome==regions::Biome::Desert||biome==regions::Biome::Savanna){
        constants.fogColor.x+=0.09f;constants.fogColor.y+=0.045f;
        constants.fogColor.z-=0.025f;
    }
    constants.weatherAndTime={conditions.snow?0.0f:conditions.precipitation,
        conditions.snow?std::max(0.5f,conditions.precipitation):
            biome==regions::Biome::Snow?0.75f:0.0f,
        night,worldTime};
    constants.ambient.w=float(ui::reflectionQuality);
    struct LocalLight {XMFLOAT4 position,color;float distance;};
    std::vector<LocalLight> lights;
    auto addLight=[&](float x,float y,float z,float radius,
                      float r,float g,float b,float brightness){
        float distance=std::hypot(x-pose.eye.x,z-pose.eye.z);
        if(distance>radius+330)return;
        lights.push_back({{x,y,z,radius},{r,g,b,brightness},distance});
    };
    if(night>0.1f){
        for(int column=0;column<5;++column)for(int row=0;row<7;++row)
            addLight(368.0f+column*450,43,115.0f+row*215,115,
                1.0f,0.74f,0.41f,1.7f*night);
        for(int z=8860;z<10010;z+=116)
            addLight(8066.0f,36,float(z),95,1.0f,0.78f,0.50f,1.4f*night);
        for(const auto& shop:commerce::shops)
            addLight(shop.p.x,20,shop.p.z,65,0.35f,0.90f,1.0f,0.95f*night);
    }
    for(const auto& vehicle:vehicles){
        if(!vehicleLightsOn(vehicle)||
           std::hypot(vehicle.p.x-player.x,vehicle.p.z-player.z)>390)continue;
        Vec2 facing=forward(vehicle.angle);
        Vec2 side{-facing.z,facing.x};
        float front=vehicle.kind==Kind::Bike?13.0f:24.05f;
        float width=vehicle.kind==Kind::Bike?0.0f:8.0f;
        for(float sign:{-1.0f,1.0f}){
            if(width==0&&sign>0)continue;
            addLight(vehicle.p.x+facing.x*front+side.x*width*sign,
                vehicle.rideHeight+(width==0?17.0f:9.5f),
                vehicle.p.z+facing.z*front+side.z*width*sign,
                125,1.0f,0.94f,0.72f,1.7f);
            if(width>0)
                addLight(vehicle.p.x-facing.x*front+side.x*width*sign,
                    vehicle.rideHeight+9.7f,
                    vehicle.p.z-facing.z*front+side.z*width*sign,
                    45,1.0f,0.09f,0.04f,0.75f);
        }
    }
    for(const auto& flame:fire::active())
        addLight(flame.p.x,12,flame.p.z,70+flame.intensity*25,
            1.0f,0.29f,0.08f,0.85f+flame.intensity*0.65f);
    for(const auto& ped:peds)if(ped.alive&&ped.burnTime>0)
        addLight(ped.p.x,18,ped.p.z,75,1.0f,0.30f,0.08f,1.2f);
    for(const auto& vehicle:vehicles)if(!vehicle.exploded&&vehicle.burnTime>0)
        addLight(vehicle.p.x,vehicle.rideHeight+20,vehicle.p.z,105,
            1.0f,0.30f,0.08f,1.5f);
    std::sort(lights.begin(),lights.end(),[](const LocalLight& a,const LocalLight& b){
        return a.distance<b.distance;});
    for(size_t i=0;i<std::min<size_t>(lights.size(),12);++i){
        constants.localLightPosition[i]=lights[i].position;
        constants.localLightColor[i]=lights[i].color;
    }
    constants.eye={pose.eye.x,pose.eye.y,pose.eye.z,float(ui::graphicsQuality)};
    float fogEnd=1250.0f*drawScale*0.94f*conditions.visibility;
    constants.params={0,fogEnd*0.51f,fogEnd,
        shadowDepth&&ui::shadowQuality>0&&daylight>0.05f?1.0f:0.0f};
    return constants;
}
bool saveScreenshotPng(const D3D11_MAPPED_SUBRESOURCE& mapped,UINT width,UINT height,
                       std::string& filename){
    std::wstring folder=executableFolder()+L"\\screenshots";
    if(!CreateDirectoryW(folder.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)
        return false;
    UINT encoderCount=0,encoderBytes=0;
    if(Gdiplus::GetImageEncodersSize(&encoderCount,&encoderBytes)!=Gdiplus::Ok||
       encoderCount==0||encoderBytes==0)return false;
    std::vector<unsigned char> storage(encoderBytes);
    auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
    if(Gdiplus::GetImageEncoders(encoderCount,encoderBytes,encoders)!=Gdiplus::Ok)
        return false;
    const CLSID* pngEncoder=nullptr;
    for(UINT i=0;i<encoderCount;++i)
        if(encoders[i].MimeType&&std::wcscmp(encoders[i].MimeType,L"image/png")==0){
            pngEncoder=&encoders[i].Clsid;break;
        }
    if(!pngEncoder)return false;
    SYSTEMTIME now{};GetLocalTime(&now);
    wchar_t name[100]{};
    for(int attempt=0;attempt<1000;++attempt){
        std::swprintf(name,100,L"MiniCity3D-%04u%02u%02u-%02u%02u%02u-%03u-%lu-%u.png",
            now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,
            now.wMilliseconds,GetCurrentProcessId(),screenshotSequence++);
        std::wstring path=folder+L"\\"+name;
        if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES)continue;
        Gdiplus::Bitmap bitmap(width,height,INT(mapped.RowPitch),PixelFormat32bppARGB,
            static_cast<BYTE*>(mapped.pData));
        if(bitmap.GetLastStatus()!=Gdiplus::Ok||
           bitmap.Save(path.c_str(),pngEncoder,nullptr)!=Gdiplus::Ok)return false;
        filename.clear();
        for(const wchar_t* character=name;*character;++character)
            filename.push_back(static_cast<char>(*character));
        return true;
    }
    return false;
}
void captureIfRequested(){
    char file[MAX_PATH]{};
    DWORD legacyLength=GetEnvironmentVariableA("MINICITY_CAPTURE",file,MAX_PATH);
    bool legacyCapture=legacyLength>0&&legacyLength<MAX_PATH;
    bool pngCapture=screenshotRequested;
    if(!legacyCapture&&!pngCapture)return;
    screenshotRequested=false;
    if(legacyCapture)SetEnvironmentVariableA("MINICITY_CAPTURE",nullptr);
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
            if(pngCapture){
                std::string name;
                if(saveScreenshotPng(mapped,description.Width,description.Height,name)){
                    message="Screenshot saved: screenshots/"+name;
                    logging::write(message.c_str());
                }else{
                    message="Screenshot failed to save";
                    logging::write(message.c_str());
                }
                messageTime=3.0f;
            }
            if(legacyCapture){
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
            }
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
           !loadTexture(path+L"_NormalDX.jpg",&normalTextures[i],false))return false;
    }
    if(!loadTexture(base+L"\\assets\\models\\baked\\marina\\MarinaFacade_NormalDX.png",
            &normalTextures[10],false))return false;
    detailTextures[11]=detailTextures[3];detailTextures[11]->AddRef();
    normalTextures[11]=normalTextures[3];normalTextures[11]->AddRef();
    detailTextures[12]=detailTextures[3];detailTextures[12]->AddRef();
    normalTextures[12]=normalTextures[3];normalTextures[12]->AddRef();
    dx11::loadMeshes(base+L"\\assets\\models\\baked");
    if(!createStaticGeometry())return false;
    // A region jump must not synchronously upload dozens of nature and city
    // meshes on its first visible frame. Load them while the startup window
    // is still hidden; the immutable buffers are shared by later instances.
    for(const dx11::Mesh* mesh:dx11::regionalMeshes())
        if(!cacheModel(mesh)){
            logging::write("Regional resource prewarm incomplete; using on-demand loading");
            break;
        }
    return true;
}
void render(){
    auto renderBegin=std::chrono::steady_clock::now();
    drawCalls=0;
    int width=std::max(1,screenW),height=std::max(1,screenH);
    if(width!=bufferW||height!=bufferH){releaseTargets();
        if(!createTargets(width,height))return;}
    dx11::buildScene(groups,models);
    auto sceneBuilt=std::chrono::steady_clock::now();
    vertices.clear();size_t starts[dx11::MATERIAL_GROUPS]{},counts[dx11::MATERIAL_GROUPS]{};
    for(int group=0;group<dx11::MATERIAL_GROUPS;++group){starts[group]=vertices.size();counts[group]=groups[group].size();
        vertices.insert(vertices.end(),groups[group].begin(),groups[group].end());}
    if(!growVertexBuffer(vertices.size()))return;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(vertexBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(dx11::Vertex));
    context->Unmap(vertexBuffer,0);
    if(!prepareInstances())return;
    auto instancesReady=std::chrono::steady_clock::now();
    float solar=std::sin((gameHour-6)*PI/12.0f);
    float daylight=std::clamp(solar*2.3f+0.42f,0.0f,1.0f);
    int requestedShadowSize=ui::shadowQuality==0?0:ui::shadowQuality==1?1024:2048;
    if(requestedShadowSize!=shadowSize&&!createShadowTargets(requestedShadowSize))createShadowTargets(0);
    // Driving and aiming render against the latest simulation pose.
    Vec2 focus=(occupied>=0||rightMouse)?player:
        previousPlayer*(1.0f-renderAlpha)+player*renderAlpha;
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
    context->OMSetRenderTargets(1,&sceneTarget,depthView);
    context->ClearRenderTargetView(sceneTarget,clearColor);
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
        constants.materialPbr=pbrForGroup(group);
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
    context->OMSetRenderTargets(1,&target,nullptr);
    context->OMSetDepthStencilState(noDepth,0);
    PostConstants post{};
    post.viewProjection=constants.viewProjection;
    XMStoreFloat4x4(&post.inverseViewProjection,
        XMMatrixInverse(nullptr,XMLoadFloat4x4(&constants.viewProjection)));
    post.cameraEye=constants.eye;
    post.pixelSize={1.0f/bufferW,1.0f/bufferH,2.0f,
        1250.0f*ui::drawDistanceScale()};
    const auto biome=regions::biomeAt(player);
    XMFLOAT4 tint{1,1,1,1.15f};
    if(biome==regions::Biome::Snow)tint={0.90f,0.97f,1.08f,1.12f};
    else if(biome==regions::Biome::Desert)tint={1.09f,1.01f,0.86f,1.12f};
    else if(biome==regions::Biome::Savanna)tint={1.07f,1.04f,0.88f,1.12f};
    float twilight=std::max(0.0f,1.0f-std::abs(solar)*4.0f);
    tint.x+=twilight*0.10f;tint.y-=twilight*0.04f;tint.z-=twilight*0.12f;
    float night=1.0f-daylight;
    tint.x-=night*0.09f;tint.y-=night*0.06f;tint.z+=night*0.04f;
    tint.w*=1.0f-weather::current().clouds*0.13f;
    post.grade=tint;
    post.skyTop={0.015f+0.10f*daylight,0.025f+0.32f*daylight,
        0.09f+0.60f*daylight,weather::current().clouds};
    post.skyHorizon={0.055f+0.55f*daylight,0.075f+0.68f*daylight,
        0.15f+0.69f*daylight,gameHour};
    post.skyTop.x+=twilight*0.10f;
    post.skyTop.y-=twilight*0.07f;
    post.skyHorizon.x+=twilight*0.34f;
    post.skyHorizon.y-=twilight*0.18f;
    post.skyHorizon.z-=twilight*0.27f;
    float cloudFade=weather::current().clouds*0.55f;
    const XMFLOAT4 overcast{0.36f,0.40f,0.45f,1};
    post.skyTop.x=post.skyTop.x*(1-cloudFade)+overcast.x*cloudFade;
    post.skyTop.y=post.skyTop.y*(1-cloudFade)+overcast.y*cloudFade;
    post.skyTop.z=post.skyTop.z*(1-cloudFade)+overcast.z*cloudFade;
    post.skyHorizon.x=post.skyHorizon.x*(1-cloudFade)+overcast.x*cloudFade;
    post.skyHorizon.y=post.skyHorizon.y*(1-cloudFade)+overcast.y*cloudFade;
    post.skyHorizon.z=post.skyHorizon.z*(1-cloudFade)+overcast.z*cloudFade;
    post.effects={float(ui::aoQuality),
        ui::graphicsQuality>0?0.12f:0.0f,float(ui::antiAliasingQuality),
        float(ui::reflectionQuality)};
    context->UpdateSubresource(postBuffer,0,nullptr,&post,0,0);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(hudVS,nullptr,0);context->PSSetShader(postPS,nullptr,0);
    context->PSSetConstantBuffers(0,1,&postBuffer);
    ID3D11ShaderResourceView* postInputs[2]={sceneView,depthViewSRV};
    context->PSSetShaderResources(0,2,postInputs);
    context->Draw(4,0);++drawCalls;
    ID3D11ShaderResourceView* emptyInputs[2]{};
    context->PSSetShaderResources(0,2,emptyInputs);
    dx11::buildHud(hudPixels.data(),bufferW,bufferH);
    if(SUCCEEDED(context->Map(hudTexture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){
        for(int row=0;row<bufferH;++row)
            std::memcpy(static_cast<unsigned char*>(mapped.pData)+size_t(row)*mapped.RowPitch,
                hudPixels.data()+size_t(row)*bufferW*4,size_t(bufferW)*4);
        context->Unmap(hudTexture,0);
        context->OMSetDepthStencilState(noDepth,0);
        float blend[4]{0,0,0,0};context->OMSetBlendState(hudBlend,blend,0xffffffffu);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->VSSetShader(hudVS,nullptr,0);context->PSSetShader(hudPS,nullptr,0);
        context->PSSetShaderResources(0,1,&hudView);
        context->Draw(4,0);++drawCalls;
        context->OMSetBlendState(nullptr,blend,0xffffffffu);
        context->OMSetDepthStencilState(nullptr,0);
    }
    captureIfRequested();
    auto beforePresent=std::chrono::steady_clock::now();
    swapChain->Present(1,0);
    auto afterPresent=std::chrono::steady_clock::now();
    if(std::strstr(GetCommandLineA(),"--benchmark-travel")&&
       std::chrono::duration<float,std::milli>(afterPresent-renderBegin).count()>30){
        char timing[200]{};
        std::snprintf(timing,sizeof(timing),
            "Travel frame at %.0f,%.0f: scene %.1f ms, uploads %.1f ms, draw/HUD %.1f ms, present %.1f ms",
            player.x,player.z,
            std::chrono::duration<float,std::milli>(sceneBuilt-renderBegin).count(),
            std::chrono::duration<float,std::milli>(instancesReady-sceneBuilt).count(),
            std::chrono::duration<float,std::milli>(beforePresent-instancesReady).count(),
            std::chrono::duration<float,std::milli>(afterPresent-beforePresent).count());
        logging::write(timing);
    }
}
void shutdownRenderer(){
    dx11::shutdownHud();
    if(context)context->ClearState();
    releaseTargets();
    release(shadowView);release(shadowDepth);release(shadowTexture);
    for(auto& texture:textures)release(texture);
    for(auto& texture:detailTextures)release(texture);
    for(auto& texture:normalTextures)release(texture);
    release(sampler);release(shadowSampler);release(hudBlend);release(alphaBlend);release(readDepth);release(noDepth);
    release(rasterState);release(shadowRaster);
    release(postBuffer);release(sceneBuffer);release(vertexBuffer);release(staticBuffer);release(inputLayout);
    release(instanceBuffer);release(instanceLayout);
    for(auto& item:meshBuffers)release(item.second);
    meshBuffers.clear();
    for(auto& item:modelTextures)release(item.second);
    modelTextures.clear();
    sharedModelTextures.clear();
    for(auto& item:pbrTextures)release(item.second);
    pbrTextures.clear();
    release(sceneVS);release(instanceVS);release(scenePS);release(alphaShadowPS);release(sceneHS);release(sceneDS);
    release(hudVS);release(hudPS);release(postPS);
    release(swapChain);release(context);release(device);
    if(gdiplusToken){Gdiplus::GdiplusShutdown(gdiplusToken);gdiplusToken=0;}
}
}
