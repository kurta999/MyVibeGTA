#pragma once
#include "dx11_sky.h"
#include <DirectXMath.h>
#include <string>

namespace dx12::cloud {
struct Constants {
    DirectX::XMFLOAT4X4 viewProjection,inverseViewProjection;
    DirectX::XMFLOAT4 cameraEye,pixelSize,grade,effects,skyTop,skyHorizon,debug;
    DirectX::XMFLOAT4X4 previousViewProjection;
    DirectX::XMFLOAT4 temporal,sunDirection,sunScreen,skyWeather;
};
inline const std::string prelude=std::string(R"HLSL(
cbuffer Post : register(b0){
    row_major float4x4 viewProjection;
    row_major float4x4 inverseViewProjection;
    float4 cameraEye;float4 pixelSize;float4 grade;float4 effects;
    float4 skyTop;float4 skyHorizon;float4 debug;
    row_major float4x4 previousViewProjection;
    float4 temporal;float4 sunDirection;float4 sunScreen;float4 skyWeather;
};
Texture2D<float> sceneDepth : register(t1);
SamplerState linearSampler : register(s0);
)HLSL")+dx11::sky::shader;

inline constexpr const char* reconstruction=R"HLSL(
Texture2D<float4> cloudVolume : register(t9);
Texture2D<float> cloudGuide : register(t10);
float4 resolveClouds(float2 uv,float3 ray,float2 pixel,float maxDistance,bool isSky){
    if(temporal.z<.5)return cloudScattering(ray,pixel,maxDistance);
    uint width,height;cloudVolume.GetDimensions(width,height);
    float2 position=uv*float2(width,height)-.5;
    int2 base=int2(floor(position));float2 fraction=frac(position);
    float4 sum=0;float total=0;
    [unroll] for(int y=0;y<2;++y)[unroll] for(int x=0;x<2;++x){
        int2 address=clamp(base+int2(x,y),int2(0,0),int2(width,height)-1);
        float guide=cloudGuide.Load(int3(address,0));
        // Never mix sky rays with surface-terminated rays. Near geometry
        // edges, use a tight distance tolerance or a full-resolution fallback.
        bool compatible=isSky?guide<0:guide>0&&abs(guide-maxDistance)<max(8,maxDistance*.01);
        float weight=(x?fraction.x:1-fraction.x)*(y?fraction.y:1-fraction.y);
        if(compatible){sum+=cloudVolume.Load(int3(address,0))*weight;total+=weight;}
    }
    if(total>.0001)return sum/total;
    return cloudScattering(ray,pixel,maxDistance);
}
)HLSL";

inline constexpr const char* pass=R"HLSL(
struct CloudOutput {float4 volume:SV_TARGET0;float guide:SV_TARGET1;};
CloudOutput CloudPS(float4 position:SV_POSITION,float2 uv:TEXCOORD0){
    uint width,height;sceneDepth.GetDimensions(width,height);
    int2 base=int2(floor(uv*float2(width,height)-.5));float depth=0;
    // Preserve thin sky openings in a half-resolution footprint. The full
    // resolution pass rejects the sky guide on opaque foreground pixels.
    [unroll] for(int y=0;y<2;++y)[unroll] for(int x=0;x<2;++x)
        depth=max(depth,sceneDepth.Load(int3(clamp(base+int2(x,y),int2(0,0),int2(width,height)-1),0)));
    bool isSky=depth>=.9999;
    float4 farPoint=mul(float4(uv.x*2-1,1-uv.y*2,1,1),inverseViewProjection);
    float3 ray=normalize(farPoint.xyz/farPoint.w-cameraEye.xyz);
    float distance=50000;
    if(!isSky){
        float4 world=mul(float4(uv.x*2-1,1-uv.y*2,depth,1),inverseViewProjection);
        distance=length(world.xyz/world.w-cameraEye.xyz);
    }
    CloudOutput output;output.guide=isSky?-1:distance;
    [branch] if(!isSky&&cameraEye.y<=1600){output.volume=float4(0,0,0,1);return output;}
    output.volume=cloudScattering(ray,uv*float2(width,height),distance);
    return output;
}
)HLSL";
}
