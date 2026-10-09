#pragma once
#include "dx11_assets.h"
struct Dx12SkinConstants {
    std::array<float,16> palette[dx11::MAX_GPU_SKIN_JOINTS];
    std::array<float,16> previousPalette[dx11::MAX_GPU_SKIN_JOINTS];
    std::array<float,4> scale,origin,transform,yaw;
    std::array<float,4> previousScale,previousOrigin,previousTransform,previousYaw;
    dx11::SkinDeformation deformation,previousDeformation;
    std::uint32_t count,start,joints,padding;
};
static_assert(sizeof(Dx12SkinConstants)%16==0&&sizeof(Dx12SkinConstants)<=65536);
// Mirrors the retained CPU deformation, including prior procedural poses.
inline constexpr const char* dx12SkinShader=R"HLSL(
struct SkinVertex {float3 position;float3 normal;float2 uv;float4 color;
    uint packedJoints;float4 weights;uint packedParts;};
struct Deformation {
    float4 motion,parameters;
    float4 bodyPosition[6],bodyRest[6],bodyRotation[6];
    float4 attachmentOrigin,attachmentRotation;
};
StructuredBuffer<SkinVertex> source:register(t0);
RWByteAddressBuffer destination:register(u0);
RWByteAddressBuffer previousDestination:register(u1);
cbuffer Skin:register(b0){
    column_major float4x4 palette[256],previousPalette[256];
    float4 scale,origin,translation,yaw;
    float4 previousScale,previousOrigin,previousTranslation,previousYaw;
    Deformation deformation,previousDeformation;
    uint vertexCount,vertexStart,jointCount,padding;
};
float3 bend(float3 p,float pivotY,float pivotZ,float angle){
    float y=p.y-pivotY,z=p.z-pivotZ;
    return float3(p.x,pivotY+cos(angle)*y+sin(angle)*z,pivotZ-sin(angle)*y+cos(angle)*z);
}
float3 rotate(float4 q,float3 p){return p+2*(q.w*cross(q.xyz,p)+cross(q.xyz,cross(q.xyz,p)));}
float3 orient(float3 p,float4 yaw){return float3(yaw.x*p.x+yaw.y*p.z,p.y,-yaw.y*p.x+yaw.x*p.z);}
void motionPose(inout float3 p,inout float3 n,float4 parts,Deformation d,float3 origin){
    int motion=int(d.motion.x);if(motion==0)return;
    float waveSin=d.motion.y,waveCos=d.motion.z,rising=d.motion.w;
    float height=d.parameters.x,minY=origin.y,centerZ=origin.z;
    if(motion>=4&&motion<=7){
        bool bike=motion>=5;float hip=minY+height*.50,knee=minY+height*.27,thigh=bike?1.05:1.45;
        float3 leg=bend(p,hip,centerZ,thigh),bentKnee=bend(float3(p.x,knee,centerZ),hip,centerZ,thigh);
        float legAngle=thigh;if(p.y<knee){leg=bend(leg,bentKnee.y,bentKnee.z,-thigh);legAngle=0;}
        if(bike)leg.x+=(parts.w-parts.z)*height*(motion==7?.30:motion==6?.16:.055);
        float legWeight=saturate(parts.z+parts.w);float3 original=p;
        p=p+(leg-p)*legWeight;n=n+(bend(n,0,0,legAngle)-n)*legWeight;
        float armWeight=saturate(parts.x+parts.y),shoulder=minY+height*.77,armAngle=bike?.95:1.25;
        p=p+(bend(original,shoulder,centerZ,armAngle)-original)*armWeight;
        n=n+(bend(n,0,0,armAngle)-n)*armWeight;
        if(motion==5){p=bend(p,hip,centerZ,-.22);n=bend(n,0,0,-.22);}return;
    }
    float armWave=parts.x*waveSin-parts.y*waveSin,legWave=-parts.z*waveSin+parts.w*waveSin;
    if(motion==8){float arms=parts.x+parts.y;
        p.y+=height*arms*(.12+.06*waveSin);p.z+=height*arms*(.12+.04*waveCos);
        p.x+=height*(parts.x-parts.y)*.035*waveCos;return;}
    if(motion==1){
        p.x+=height*.17*(parts.y*(.5-.5*waveSin)-parts.x*(.5+.5*waveSin));
        p.z+=height*(.18*armWave+.10*legWave);p.y+=height*(.08*(parts.x+parts.y)*waveCos+.07*legWave);
    }else if(motion==2){float arms=parts.x+parts.y,shoulder=minY+height*.77;
        p.y+=arms*max(0,shoulder-p.y)*1.85+height*(.07*armWave+.13*legWave);
        p.z+=height*(.09*arms-.05*legWave);
    }else if(motion==3){p.y+=height*(.14*(parts.x+parts.y)+.10*rising*(parts.z+parts.w));
        p.z+=height*(.09*rising*(parts.z+parts.w)-.04*armWave);}
    float tilt=motion==1?1.05:motion==2?.10:motion==3?.12:0;
    p=bend(p,minY+height*.45,centerZ,-tilt);n=bend(n,0,0,-tilt);
}
void deform(SkinVertex input,bool prior,Deformation d,float4 s,float4 o,float4 t,float4 y,out float3 world,out float3 worldNormal){
    float3 p=0,n=0;float4 parts=0;
    [unroll]for(uint influence=0;influence<4;++influence){
        uint joint=(input.packedJoints>>(8*influence))&255,part=(input.packedParts>>(8*influence))&255;
        float weight=input.weights[influence];
        if(weight>0&&joint<jointCount){
            float4x4 m=prior?previousPalette[joint]:palette[joint];
            float3 position=mul(m,float4(input.position,1)).xyz,normal=mul(m,float4(input.normal,0)).xyz;
            parts+=weight*float4(part==2,part==3,part==4,part==5);
            if(d.parameters.w!=0){
                float3 rest=t.xyz+orient((position-o.xyz)*s.xyz,y);
                position=d.bodyPosition[part].xyz+rotate(d.bodyRotation[part],rest-d.bodyRest[part].xyz);
                normal=rotate(d.bodyRotation[part],orient(normal/s.xyz,y));
            }
            p+=weight*position;n+=weight*normal;
        }
    }
    if(d.parameters.w!=0){world=p;worldNormal=length(n)>0.001?normalize(n):float3(0,0,0);return;}
    motionPose(p,n,parts,d,o.xyz);
    world=t.xyz+orient((p-o.xyz)*s.xyz,y);n/=s.xyz;
    worldNormal=orient(n,y)/max(.0001,length(n));
    float weight=parts.x+parts.y;
    if(weight>0&&abs(d.parameters.y)>=.001){
        float pitch=d.parameters.y*saturate(weight);float3 forward=float3(y.y,0,y.x);
        float3 pivot=t.xyz+float3(forward.x*3,d.parameters.z*.77,forward.z*3);
        float depth=dot(world-pivot,forward),vertical=world.y-pivot.y;
        float rotated=cos(pitch)*depth-sin(pitch)*vertical;
        world.xz+=forward.xz*(rotated-depth);world.y=pivot.y+sin(pitch)*depth+cos(pitch)*vertical;
    }
}
[numthreads(64,1,1)]
void CS(uint3 id:SV_DispatchThreadID){
    if(id.x>=vertexCount)return;
    SkinVertex input=source[id.x];float3 world,normal,previousWorld,unused;
    deform(input,false,deformation,scale,origin,translation,yaw,world,normal);
    world=deformation.attachmentOrigin.xyz+rotate(deformation.attachmentRotation,world-deformation.attachmentOrigin.xyz);
    normal=rotate(deformation.attachmentRotation,normal);
    previousWorld=world;
    if(padding!=0){
        deform(input,true,previousDeformation,previousScale,previousOrigin,previousTranslation,previousYaw,previousWorld,unused);
        previousWorld=previousDeformation.attachmentOrigin.xyz+rotate(previousDeformation.attachmentRotation,previousWorld-previousDeformation.attachmentOrigin.xyz);
    }
    uint address=(vertexStart+id.x)*48;
    destination.Store3(address,asuint(world));destination.Store3(address+12,asuint(normal));
    destination.Store2(address+24,asuint(input.uv));destination.Store4(address+32,asuint(input.color));
    previousDestination.Store4((vertexStart+id.x)*16,asuint(float4(previousWorld,float(padding))));
}
)HLSL";
