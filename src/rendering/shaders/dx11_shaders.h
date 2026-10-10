#pragma once
#include <string>
#include "../../dx11_sky.h"

namespace rendering::shaders {
inline const std::string postShader = std::string(R"HLSL(
cbuffer Post : register(b0){
    row_major float4x4 viewProjection;
    row_major float4x4 inverseViewProjection;
    float4 cameraEye;float4 pixelSize;float4 grade;float4 effects;
    float4 skyTop;float4 skyHorizon;float4 debug;
    row_major float4x4 previousViewProjection;
    float4 temporal;float4 sunDirection;float4 sunScreen;float4 skyWeather;
};
Texture2D sceneColor : register(t0);
Texture2D sceneDepth : register(t1);
Texture2D sceneSurface : register(t2);
Texture2D bloomHalf : register(t3);
Texture2D bloomQuarter : register(t4);
Texture2D bloomEighth : register(t5);
Texture2D reflectionDelta : register(t6);
Texture2D sceneIndirect : register(t7);
SamplerState linearSampler : register(s0);
)HLSL") + dx11::sky::shader + R"HLSL(
struct Input {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
float luminance(float3 c){return dot(c,float3(0.2126,0.7152,0.0722));}
float solarVisibility(){
    if(sunScreen.w<.5||sunDirection.w<=0)return 0;
    float visibility=0;
    float aspect=pixelSize.y/pixelSize.x;
    [unroll] for(int y=-1;y<=1;++y)[unroll] for(int x=-1;x<=1;++x){
        float2 at=sunScreen.xy+float2(x/aspect,y)*sunScreen.z*.65;
        if(all(at>0)&&all(at<1))
            visibility+=step(.99999,sceneDepth.SampleLevel(linearSampler,at,0).r);
    }
    float edge=min(min(sunScreen.x,1-sunScreen.x),min(sunScreen.y,1-sunScreen.y));
    return visibility/9.0*smoothstep(0,.035,edge)*sunDirection.w;
}
float3 lensFlare(float2 uv){
    float visibility=solarVisibility();
    if(visibility<=0)return 0;
    float aspect=pixelSize.y/pixelSize.x;
    float2 axis=.5-sunScreen.xy;
    float2 delta=(uv-sunScreen.xy)*float2(aspect,1);
    float radius=length(delta);
    float3 value=float3(1,.69,.35)*exp(-radius*radius/0.0012)*.14;
    value+=float3(1,.78,.48)*exp(-abs(delta.x)*24-abs(delta.y)*850)*.065;
    float offsets[4]={.65,1.25,1.7,2.25};
    float sizes[4]={.018,.028,.045,.07};
    float3 colors[4]={float3(.3,.55,1),float3(.8,.35,.16),
        float3(.18,.6,.4),float3(.48,.22,.7)};
    [unroll] for(int i=0;i<4;++i){
        float2 p=(uv-(sunScreen.xy+axis*offsets[i]))*float2(aspect,1);
        float hex=max(abs(p.y),abs(p.x)*.8660254+abs(p.y)*.5);
        float core=1-smoothstep(sizes[i]*.72,sizes[i],hex);
        float rim=exp(-pow((hex-sizes[i]*.82)/(sizes[i]*.09),2));
        value+=colors[i]*(core*.023+rim*.035);
    }
    return value*visibility;
}
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
            }else center=lerp(center,a,effects.z<1.0?0.45:0.72);
        }
    }
    float4 surface=sceneColor.SampleLevel(linearSampler,uv,0);
    float d=sceneDepth.SampleLevel(linearSampler,uv,0).r;
    // Derivatives must run across every lane, including geometry at the
    // horizon. Computing this inside the sky branch produces invalid widths.
    float4 farSky=mul(float4(uv.x*2-1,1-uv.y*2,1,1),inverseViewProjection);
    float3 skyRay=normalize(farSky.xyz/farSky.w-cameraEye.xyz);
    float sunSeparation=acos(clamp(dot(skyRay,sunDirection.xyz),-1,1));
    float sunFootprint=clamp(fwidth(sunSeparation),.00008,.003);
    if(debug.w>.5)return float4(lensFlare(uv)*8,1);
    float4 geometry=sceneSurface.SampleLevel(linearSampler,uv,0);
    if(debug.x>0.5){
        if(debug.x>4.5)return float4(saturate(
            sceneIndirect.SampleLevel(linearSampler,uv,0).rgb*2.0),1);
        if(debug.x>3.5){
            float3 glow=bloomHalf.SampleLevel(linearSampler,uv,0).rgb*0.5+
                bloomQuarter.SampleLevel(linearSampler,uv,0).rgb*0.3+
                bloomEighth.SampleLevel(linearSampler,uv,0).rgb*0.2;
            return float4(saturate(glow*3.0),1);
        }
        if(debug.x>2.5)return float4(center,1);
        if(d>=0.9999)return float4(0,0,0,1);
        if(debug.x<1.5)return float4(geometry.rgb,1);
        return float4(geometry.www,1);
    }
    float3 shadingNormal=normalize(geometry.xyz*2-1);
    if(effects.w>0.5&&surface.a<0.95&&geometry.w<0.9&&d<0.9999){
        float2 halfPixel=pixelSize.xy*2.0;
        float2 position=uv/halfPixel-0.5;
        float2 base=floor(position),fraction=frac(position);
        float3 delta=0;float total=0;
        [unroll] for(int y=0;y<2;++y)
            [unroll] for(int x=0;x<2;++x){
                int2 address=clamp(int2(base)+int2(x,y),int2(0,0),
                    max(int2(1,1),int2(floor(1.0/halfPixel)))-1);
                float4 sample=reflectionDelta.Load(int3(address,0));
                float bilinear=(x==0?1-fraction.x:fraction.x)*
                    (y==0?1-fraction.y:fraction.y);
                float depthWeight=1-smoothstep(3.0,24.0,
                    abs(linearDepth(d)-sample.a));
                float weight=bilinear*depthWeight;
                delta+=sample.rgb*weight;total+=weight;
            }
        if(total>0.001)center+=delta/total;
    }
    if(d<.9999&&cameraEye.y>1600){
        float4 world=mul(float4(uv.x*2-1,1-uv.y*2,d,1),inverseViewProjection);
        float sceneDistance=length(world.xyz/world.w-cameraEye.xyz);
        center=volumetricSky(skyRay,input.position.xy,sceneDistance,center,false).rgb;
    }
    if(d>=0.9999){
        float4 sky=volumetricSky(skyRay,input.position.xy);
        center=sky.rgb;
        if(sunDirection.w>0){
            float disk=1-smoothstep(.00465-sunFootprint,.00465+sunFootprint,sunSeparation);
            float haze=exp(-sunSeparation*sunSeparation/.002)*.18;
            center+=float3(1,.80,.52)*(disk*30+haze)*sunDirection.w*sky.a;
        }
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
            float3 neighborNormal=normalize(
                sceneSurface.SampleLevel(linearSampler,saturate(at),0).xyz*2-1);
            float difference=z-neighbor;
            occlusion+=smoothstep(0.45,8.0,difference)*
                (1-smoothstep(14.0,42.0,difference))*
                saturate(dot(shadingNormal,neighborNormal));
        }
        float ao=(effects.x>1.5?0.52:0.35)*occlusion/tapCount;
        center=max(0,center-sceneIndirect.SampleLevel(linearSampler,uv,0).rgb*ao);
    }
    if(effects.y>0.01){
        float3 glow=bloomHalf.SampleLevel(linearSampler,uv,0).rgb*0.5+
            bloomQuarter.SampleLevel(linearSampler,uv,0).rgb*0.3+
            bloomEighth.SampleLevel(linearSampler,uv,0).rgb*0.2;
        center+=glow*effects.y;
    }
    center+=lensFlare(uv);
    return float4(max(0,center),1);
}
)HLSL";
} // namespace rendering::shaders
