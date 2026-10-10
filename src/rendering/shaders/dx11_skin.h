#pragma once

namespace rendering::shaders {
inline const char* skinShader = R"HLSL(
struct SkinVertex {float3 position;float3 normal;float2 uv;float4 color;
    uint packedJoints;float4 weights;};
StructuredBuffer<SkinVertex> source:register(t0);
RWByteAddressBuffer destination:register(u0);
RWByteAddressBuffer previousDestination:register(u1);
cbuffer Skin:register(b0){
    column_major float4x4 palette[256];
    column_major float4x4 previousPalette[256];
    float4 scale,origin,translation,yaw;
    float4 previousScale,previousOrigin,previousTranslation,previousYaw;
    uint vertexCount,vertexStart,jointCount,padding;
};
[numthreads(64,1,1)]
void CS(uint3 id:SV_DispatchThreadID){
    if(id.x>=vertexCount)return;
    SkinVertex input=source[id.x];
    float3 position=0,normal=0,previousPosition=0;
    [unroll]for(uint influence=0;influence<4;++influence){
        uint joint=(input.packedJoints>>(8*influence))&255;
        float weight=input.weights[influence];
        if(weight>0&&joint<jointCount){
            position+=weight*mul(palette[joint],float4(input.position,1)).xyz;
            normal+=weight*mul(palette[joint],float4(input.normal,0)).xyz;
            previousPosition+=weight*mul(previousPalette[joint],float4(input.position,1)).xyz;
        }
    }
    position=(position-origin.xyz)*scale.xyz;
    float3 world=translation.xyz+float3(yaw.x*position.x+yaw.y*position.z,
        position.y,-yaw.y*position.x+yaw.x*position.z);
    normal/=scale.xyz;
    float magnitude=max(0.0001,length(normal));
    float3 worldNormal=float3(yaw.x*normal.x+yaw.y*normal.z,normal.y,
        -yaw.y*normal.x+yaw.x*normal.z)/magnitude;
    uint address=(vertexStart+id.x)*48;
    destination.Store3(address,asuint(world));
    destination.Store3(address+12,asuint(worldNormal));
    destination.Store2(address+24,asuint(input.uv));
    destination.Store4(address+32,asuint(input.color));
    previousPosition=(previousPosition-previousOrigin.xyz)*previousScale.xyz;
    float3 previousWorld=previousTranslation.xyz+float3(
        previousYaw.x*previousPosition.x+previousYaw.y*previousPosition.z,
        previousPosition.y,-previousYaw.y*previousPosition.x+previousYaw.x*previousPosition.z);
    previousDestination.Store4((vertexStart+id.x)*16,asuint(float4(previousWorld,float(padding))));
}
)HLSL";
}
