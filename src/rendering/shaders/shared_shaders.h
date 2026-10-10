#pragma once
#include <string>

namespace rendering::shaders {
inline const char* probeShader = R"HLSL(
cbuffer Probes:register(b1){
    float4 probePositions[2];float4 probeSh[3][9];
    float4 probeTimeWeights;float4 probeInfo;
};
TextureCubeArray probeEnvironment:register(t11);
Texture2D probeBrdf:register(t12);
SamplerState probeSampler:register(s3);
float3 spatialProbeWeights(float3 world){
    float a=1-smoothstep(0,probePositions[0].w,distance(world,probePositions[0].xyz));
    float b=1-smoothstep(0,probePositions[1].w,distance(world,probePositions[1].xyz));
    float total=a+b;
    return float3(max(0,1-total),a/max(1,total),b/max(1,total));
}
float3 diffuseProbe(float3 world,float3 n){
    float basis[9]={0.282094792,0.488602512*n.y,0.488602512*n.z,
        0.488602512*n.x,1.092548431*n.x*n.y,1.092548431*n.y*n.z,
        0.315391565*(3*n.z*n.z-1),1.092548431*n.x*n.z,
        0.546274215*(n.x*n.x-n.y*n.y)};
    float3 weights=spatialProbeWeights(world),result=0;
    [unroll] for(int p=0;p<3;++p){
        float3 value=0;
        [unroll] for(int i=0;i<9;++i)value+=probeSh[p][i].rgb*basis[i];
        result+=max(0,value)*weights[p];
    }
    return result*probeInfo.y;
}
float3 specularProbe(float3 world,float3 ray,float roughness){
    float3 weights=spatialProbeWeights(world),result=0;
    [unroll] for(int p=0;p<3;++p)if(weights[p]>0.0001){
        int firstCube=p==0?6:(p-1)*3;
        [unroll] for(int time=0;time<3;++time)if(probeTimeWeights[time]>0.0001)
            result+=probeEnvironment.SampleLevel(probeSampler,
                float4(ray,firstCube+time),roughness*probeInfo.x).rgb*
                weights[p]*probeTimeWeights[time];
    }
    return result*probeInfo.y;
}
)HLSL";

inline const char* sceneShaderPrelude = R"HLSL(
cbuffer Scene : register(b0){
    row_major float4x4 viewProjection;
    row_major float4x4 shadowViewProjection[3];
    float4 sun;
    float4 ambient;
    float4 fogColor;
    float4 eye;
    float4 params;
    float4 weatherAndTime;
    float4 materialPbr;
    float4 materialSurface;
    float4 materialOptions;
    float4 shadowInfo;
    float4 localLightPosition[12];
    float4 localLightColor[12];
    row_major float4x4 headlightViewProjection;
    float4 headlightShadowInfo;
    row_major float4x4 streetViewProjection;
    float4 streetShadowInfo;
    float4 temporalInfo;
    row_major float4x4 previousViewProjection;
};
Texture2D diffuseTexture : register(t0);
Texture2D detailTexture : register(t1);
Texture2D normalTexture : register(t2);
Texture2D foliageTexture : register(t3);
Texture2DArray shadowTexture : register(t4);
Texture2D modelNormalTexture : register(t5);
Texture2D modelOrmTexture : register(t6);
Texture2D modelOcclusionTexture : register(t7);
Texture2D modelEmissiveTexture : register(t8);
Texture2D headlightShadowTexture : register(t9);
Texture2D streetShadowTexture : register(t10);
SamplerState linearSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);
SamplerState modelSampler : register(s2);
struct Input { float3 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; float4 color:COLOR0; };
struct InstancedInput {
    float3 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; float4 color:COLOR0;
    float4 a:INSTANCE0; float4 b:INSTANCE1; float4 c:INSTANCE2; float4 tint:INSTANCE3;
    float4 quaternion:INSTANCE4;
};
struct SkinnedInput {
    float3 position:POSITION;float3 normal:NORMAL;float2 uv:TEXCOORD0;float4 color:COLOR0;
    float4 previous:POSITION1;
};
struct Output { float4 position:SV_POSITION; float3 world:TEXCOORD1; float3 normal:NORMAL; float2 uv:TEXCOORD0; float4 color:COLOR0; float4 shadowPosition[3]:TEXCOORD2;
    float4 previousPosition:TEXCOORD5;float previousValid:TEXCOORD6;};
Output VS(Input input){
    Output result;
    result.position=mul(float4(input.position,1),viewProjection);
    [unroll] for(int i=0;i<3;++i)
        result.shadowPosition[i]=mul(float4(input.position,1),shadowViewProjection[i]);
    result.world=input.position;result.normal=input.normal;result.uv=input.uv;result.color=input.color;
    result.previousPosition=0;result.previousValid=0;
    return result;
}
Output VSSkinned(SkinnedInput input){
    Input current;current.position=input.position;current.normal=input.normal;
    current.uv=input.uv;current.color=input.color;
    Output result=VS(current);
    result.previousPosition=mul(float4(input.previous.xyz,1),previousViewProjection);
    result.previousValid=input.previous.w;return result;
}
Output VSInstanced(InstancedInput input){
    float3 local=float3((input.position.x-input.c.x)*input.a.x,
        (input.position.y-input.c.y)*input.a.y,
        (input.position.z-input.c.z)*input.a.z);
    local+=2*cross(input.quaternion.xyz,cross(input.quaternion.xyz,local)+input.quaternion.w*local);
    float3 pitched=float3(local.x,input.tint.w*local.y+input.c.w*local.z,
        -input.c.w*local.y+input.tint.w*local.z);
    float3 world=float3(input.b.y+input.a.w*pitched.x+input.b.x*pitched.z,
        input.b.z+pitched.y,input.b.w-input.b.x*pitched.x+input.a.w*pitched.z);
    if(materialSurface.w>0.5){
        float phase=weatherAndTime.w*1.6+input.b.y*.047+input.b.w*.063;
        float wave=sin(phase)+.35*sin(phase*2.13);
        float bend=input.color.a*min(3.0,input.a.y*.12)*wave;
        world.x+=bend;world.z+=bend*.48;
    }
    float3 scaledNormal=input.normal/max(input.a.xyz,float3(0.0001,0.0001,0.0001));
    scaledNormal+=2*cross(input.quaternion.xyz,cross(input.quaternion.xyz,scaledNormal)+input.quaternion.w*scaledNormal);
    float3 pitchedNormal=float3(scaledNormal.x,
        input.tint.w*scaledNormal.y+input.c.w*scaledNormal.z,
        -input.c.w*scaledNormal.y+input.tint.w*scaledNormal.z);
    float3 normal=normalize(float3(input.a.w*pitchedNormal.x+input.b.x*pitchedNormal.z,
        pitchedNormal.y,-input.b.x*pitchedNormal.x+input.a.w*pitchedNormal.z));
    Output result;
    result.position=mul(float4(world,1),viewProjection);
    [unroll] for(int i=0;i<3;++i)
        result.shadowPosition[i]=mul(float4(world,1),shadowViewProjection[i]);
    result.world=world;result.normal=normal;result.uv=input.uv;
    result.color=float4(input.color.rgb*input.tint.rgb,input.color.a);
    if(materialSurface.w>0.5)result.color.a=1;
    result.previousPosition=0;result.previousValid=0;
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
    result.previousPosition=0;result.previousValid=0;
    result.world+=result.normal*surfaceDisplacement(result.world,result.normal,result.uv);
    result.position=mul(float4(result.world,1),viewProjection);
    [unroll] for(int i=0;i<3;++i)
        result.shadowPosition[i]=mul(float4(result.world,1),shadowViewProjection[i]);
    return result;
}
struct SceneOutput {float4 color:SV_TARGET0;float4 surface:SV_TARGET1;
    float4 indirect:SV_TARGET2;float4 motion:SV_TARGET3;
    float4 reflectionResponse:SV_TARGET4;};
float sampleSunShadow(float4 shadowPosition,float diffuse,int cascade){
    if(shadowPosition.w<=0)return 1;
    float3 projected=shadowPosition.xyz/shadowPosition.w;
    float2 uv=float2(projected.x*0.5+0.5,0.5-projected.y*0.5);
    if(any(uv<0)||any(uv>1)||projected.z<=0||projected.z>=1)return 1;
    float bias=max(0.00018,0.0012*(1-diffuse));
    float filtered=0;
    [unroll] for(int sy=0;sy<2;++sy)
        [unroll] for(int sx=0;sx<2;++sx)
            filtered+=shadowTexture.SampleCmpLevelZero(shadowSampler,
                float3(uv+(float2(sx,sy)-0.5)*shadowInfo.x*1.6,cascade),
                projected.z-bias);
    return lerp(0.24,1.0,filtered*0.25);
}
float sampleHeadlightShadow(float3 world,float diffuse){
    float4 position=mul(float4(world,1),headlightViewProjection);
    if(position.w<=0)return 1;
    float3 projected=position.xyz/position.w;
    float2 uv=float2(projected.x*0.5+0.5,0.5-projected.y*0.5);
    if(any(uv<=0.001)||any(uv>=0.999)||projected.z<=0||projected.z>=1)
        return 1;
    float bias=0.0015+0.003*(1-diffuse);
    float filtered=0;
    [unroll] for(int y=0;y<2;++y)
        [unroll] for(int x=0;x<2;++x)
            filtered+=headlightShadowTexture.SampleCmpLevelZero(shadowSampler,
                uv+(float2(x,y)-0.5)*headlightShadowInfo.w*1.5,
                projected.z-bias);
    return filtered*0.25;
}
float sampleStreetShadow(float3 world,float diffuse){
    float4 position=mul(float4(world,1),streetViewProjection);
    if(position.w<=0)return 1;
    float3 projected=position.xyz/position.w;
    float2 uv=float2(projected.x*0.5+0.5,0.5-projected.y*0.5);
    if(any(uv<=0.001)||any(uv>=0.999)||projected.z<=0||projected.z>=1)
        return 1;
    float bias=0.0015+0.003*(1-diffuse);
    float filtered=0;
    [unroll] for(int y=0;y<2;++y)
        [unroll] for(int x=0;x<2;++x)
            filtered+=streetShadowTexture.SampleCmpLevelZero(shadowSampler,
                uv+(float2(x,y)-0.5)*streetShadowInfo.z*1.5,
                projected.z-bias);
    return filtered*0.25;
}
float3 fresnelSchlick(float3 f0,float cosine){
    return f0+(1-f0)*pow(1-saturate(cosine),5);
}
float microfacet(float ndv,float ndl,float ndh,float roughness){
    float alpha=max(0.0025,roughness*roughness),a2=alpha*alpha;
    float d=ndh*ndh*(a2-1)+1;
    // A denominator offset suppresses precisely the smooth-surface peaks.
    float distribution=a2/max(3.14159265*d*d,1e-10);
    float gv=ndl*sqrt(ndv*ndv*(1-a2)+a2);
    float gl=ndv*sqrt(ndl*ndl*(1-a2)+a2);
    return distribution*0.5/max(gv+gl,1e-6);
}
float3 directBrdf(float3 n,float3 v,float3 l,float roughness,
                  float3 f0,float3 base,float metallic){
    if(dot(n,l)<=0||dot(n,v)<=0)return 0;
    float3 h=normalize(v+l);
    float ndv=max(0.001,dot(n,v)),ndl=saturate(dot(n,l));
    float3 f=fresnelSchlick(f0,dot(v,h));
    return ((1-f)*(1-metallic)*base/3.14159265+
        microfacet(ndv,ndl,saturate(dot(n,h)),roughness)*f)*ndl;
}
float3 coatedDirect(float3 n,float3 coatNormal,float3 v,float3 l,
                     float roughness,float3 f0,float3 base,float metallic){
    if(dot(v+l,v+l)<1e-6)return 0;
    float3 value=directBrdf(n,v,l,roughness,f0,base,metallic);
    float coat=materialSurface.x;
    if(coat>0){
        float3 h=normalize(v+l);
        float ndv=max(.001,dot(coatNormal,v)),ndl=saturate(dot(coatNormal,l));
        float fc=fresnelSchlick(.04,saturate(dot(v,h))).x;
        float top=microfacet(ndv,ndl,saturate(dot(coatNormal,h)),
            max(.05,materialSurface.y))*fc*ndl;
        value=value*(1-coat*fc)+coat*top;
    }
    return value;
}
)HLSL";

inline const char* sceneShaderPixel = R"HLSL(SceneOutput PS(Output input){
    SceneOutput output;
    output.indirect=float4(0,0,0,0);output.reflectionResponse=0;
    output.motion=0;
    if(temporalInfo.y>0.5){
        output.motion.w=-1;
        if(input.previousValid>0.5&&input.previousPosition.w>0.00001){
            float3 previous=input.previousPosition.xyz/input.previousPosition.w;
            float2 previousUv=float2(previous.x*0.5+0.5,0.5-previous.y*0.5);
            float valid=all(previousUv>=0)&&all(previousUv<=1)&&previous.z>0&&previous.z<1?1:-1;
            output.motion=float4(input.position.xy*temporalInfo.zw-previousUv,previous.z,valid);
        }
    }
    float4 base=input.color;
    int material=(int)(params.x+0.5);
    bool modelTexture=frac(params.x)>0.1;
    if(material==0||material==3||material==7||material==8||material==9||material==11||material==15)
        base.rgb=pow(saturate(base.rgb),2.2);
    if(material==1||modelTexture)
        base*=modelTexture?diffuseTexture.Sample(modelSampler,input.uv):
            diffuseTexture.Sample(linearSampler,input.uv);
    int mapFlags=(int)(materialPbr.w+0.5);
    if(modelTexture&&(mapFlags&16)!=0){
        clip(base.a-materialOptions.x);
    }
    if(material==4&&materialSurface.w<0.5&&modelTexture&&(mapFlags&16)!=0&&base.g>base.r*1.18){
        float nearCamera=distance(input.world,eye.xyz);
        float keep=smoothstep(11.0,34.0,nearCamera);
        float stableNoise=frac(sin(dot(floor(input.world.xz*0.72),
            float2(12.9898,78.233)))*43758.5453);
        clip(keep-stableNoise);
    }
    if(materialOptions.z>0.5||materialOptions.w>0.5){
        float3 shown=base.rgb;
        if(materialOptions.w>0.5){
            float lod=modelTexture?diffuseTexture.CalculateLevelOfDetail(
                modelSampler,input.uv):material==1?
                diffuseTexture.CalculateLevelOfDetail(linearSampler,input.uv):0;
            shown=modelTexture||material==1?
                lerp(float3(0.08,0.3,0.95),float3(0.95,0.16,0.04),
                    saturate(lod/8.0)):float3(0.12,0.12,0.12);
        }
        output.color=float4(shown,base.a);
        output.surface=float4(normalize(input.normal)*0.5+0.5,1);
        return output;
    }
    if((material==13&&!modelTexture)||materialPbr.z<0){
        float distanceToEye=distance(input.world,eye.xyz);
        float fog=saturate((distanceToEye-params.y)/max(1,params.z-params.y));
        output.color=float4(lerp(base.rgb,fogColor.rgb,fog),base.a*(1-fog));
        output.surface=float4(0.5,1,0.5,1);
        return output;
    }
    if(materialOptions.y>0.5){
        float range=distance(input.world,eye.xyz);
        int cascade=shadowInfo.w<1.5||range<shadowInfo.y?0:
            shadowInfo.w<2.5||range<shadowInfo.z?1:2;
        output.color=float4(cascade==0?float3(0.9,0.22,0.18):
            cascade==1?float3(0.22,0.85,0.25):float3(0.24,0.34,0.95),base.a);
        output.surface=float4(normalize(input.normal)*0.5+0.5,1);
        return output;
    }
    float3 normal=normalize(input.normal);
    if(materialSurface.w>0.5&&dot(normal,eye.xyz-input.world)<0)normal=-normal;
    float3 coatNormal=normal;
    float normalCoherence=1.0;
    if(material==10){
        float4 normalSample=normalTexture.Sample(modelSampler,input.uv);
        normalCoherence=normalSample.a;
        float3 bump=normalSample.xyz*2-1;
        float3 tangent=abs(normal.y)>0.5?float3(1,0,0):
            abs(normal.x)>0.5?float3(0,0,1):float3(1,0,0);
        float3 bitangent=normalize(cross(normal,tangent));
        normal=normalize(lerp(normal,normalize(tangent*bump.x+
            bitangent*bump.y+normal*bump.z),0.48));
    }else if(material>=2&&material!=3&&(mapFlags&1)==0){
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
        float4 normalSample=normalTexture.Sample(linearSampler,uv);
        normalCoherence=normalSample.a;
        float3 bump=normalSample.xyz*2-1;
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
    if((mapFlags&1)!=0){
        float4 normalSample=modelNormalTexture.Sample(modelSampler,input.uv);
        float3 mapped=normalSample.xyz*2-1;
        if((mapFlags&32)!=0){
            mapped.z=sqrt(saturate(1-dot(mapped.xy,mapped.xy)));
            // BC5 keeps X/Y; the associated cooked ORM alpha stores coherence.
            if((mapFlags&2)!=0)normalCoherence=min(normalCoherence,
                modelOrmTexture.Sample(modelSampler,input.uv).a);
        }else normalCoherence=min(normalCoherence,normalSample.a);
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
        float4 orm=modelOrmTexture.Sample(modelSampler,input.uv);
        roughness=clamp(roughness*orm.g,0.06,1.0);
        metallic*=orm.b;
    }
    // UV-space grime and fine scuffs stay attached to moving bodywork.
    // Keep glass transparent and leave its Fresnel response intact.
    if((mapFlags&64)!=0&&materialSurface.z<1){
        float2 q=input.uv*32;
        float grain=frac(sin(dot(floor(q),float2(127.1,311.7)))*43758.5453);
        float patches=saturate(.5+.35*sin(q.x*.37+sin(q.y*.29))+.25*sin(q.y*.61));
        float dirt=smoothstep(.48,.92,patches)*(.20+.20*grain);
        float phase=frac(input.uv.y*137+sin(input.uv.x*41)*.13);
        float aa=max(fwidth(input.uv.y*137),.006);
        float scratch=(1-smoothstep(.006,.006+aa,min(phase,1-phase)))*
            step(.78,grain)*smoothstep(.2,.6,patches);
        base.rgb=lerp(base.rgb,float3(.12,.09,.055),dirt);
        base.rgb=lerp(base.rgb,float3(.36,.38,.39),scratch*.6);
        roughness=lerp(roughness,.91,dirt);
        roughness=lerp(roughness,.67,scratch*.4);
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
    // Normal mips store the length of the averaged normal in alpha. Broaden
    // the specular lobe as normal directions cancel at distance.
    roughness=sqrt(saturate(roughness*roughness+
        (1.0-saturate(normalCoherence))*0.5));
    float3 viewDirection=normalize(eye.xyz-input.world);
    float ndv=max(0.001,dot(normal,viewDirection));
    bool glass=materialSurface.z>1;
    float dielectricF0=glass?pow((materialSurface.z-1)/(materialSurface.z+1),2):.04;
    float3 f0=lerp(dielectricF0.xxx,base.rgb,metallic);
    // A thin pane reflects the environment and sun while the blended queue
    // transmits the already-rendered background. It is not refractive glass.
    if(glass)base.rgb*=.08;
    float occlusion=(mapFlags&4)!=0?
        modelOcclusionTexture.Sample(modelSampler,input.uv).r:1.0;
    float3 lit=(base.rgb*ambient.rgb*(1-metallic)*0.75+
        f0*ambient.rgb*0.15)*occlusion;
    if(probeInfo.z>0.5){
        float3 fresnelIbl=f0+(max(1-roughness,f0)-f0)*pow(1-ndv,5);
        float2 dfg=probeBrdf.SampleLevel(probeSampler,float2(ndv,roughness),0).rg;
        float3 response=f0*dfg.x+dfg.y;
        lit=((1-fresnelIbl)*(1-metallic)*base.rgb*
            diffuseProbe(input.world,normal)+
            specularProbe(input.world,reflect(-viewDirection,normal),roughness)*response)*occlusion;
        output.reflectionResponse=float4(response*occlusion,1);
    }
    if(materialSurface.x>0){
        float coatView=max(.001,dot(coatNormal,viewDirection));
        float fc=fresnelSchlick(.04,coatView).x*materialSurface.x;
        lit*=1-fc;output.reflectionResponse.rgb*=1-fc;
        if(probeInfo.z>0.5){
            float2 dfg=probeBrdf.SampleLevel(probeSampler,
                float2(coatView,max(.05,materialSurface.y)),0).rg;
            lit+=specularProbe(input.world,reflect(-viewDirection,coatNormal),
                max(.05,materialSurface.y))*(.04*dfg.x+dfg.y)*materialSurface.x*occlusion;
        }
    }
    float3 indirect=lit;
    float diffuse=saturate(dot(normal,normalize(sun.xyz)));
    float visibility=1;
    if(params.w>0.5&&sun.w>0.0001){
        float range=distance(input.world,eye.xyz);
        int cascade=shadowInfo.w<1.5||range<shadowInfo.y?0:
            shadowInfo.w<2.5||range<shadowInfo.z?1:2;
        visibility=sampleSunShadow(input.shadowPosition[cascade],diffuse,cascade);
        float nearBand=shadowInfo.y*0.08;
        float midBand=shadowInfo.z*0.06;
        if(shadowInfo.w>1.5&&abs(range-shadowInfo.y)<nearBand){
            float a=sampleSunShadow(input.shadowPosition[0],diffuse,0);
            float b=sampleSunShadow(input.shadowPosition[1],diffuse,1);
            visibility=lerp(a,b,smoothstep(shadowInfo.y-nearBand,
                shadowInfo.y+nearBand,range));
        }else if(shadowInfo.w>2.5&&abs(range-shadowInfo.z)<midBand){
            float a=sampleSunShadow(input.shadowPosition[1],diffuse,1);
            float b=sampleSunShadow(input.shadowPosition[2],diffuse,2);
            visibility=lerp(a,b,smoothstep(shadowInfo.z-midBand,
                shadowInfo.z+midBand,range));
        }
    }
    float3 lightDirection=normalize(sun.xyz);
    float3 sunTint=lerp(float3(1.0,0.61,0.35),float3(1.0,0.98,0.93),
        saturate(abs(sun.y-0.18)*2.5));
    if(sun.w>0)lit+=coatedDirect(normal,coatNormal,viewDirection,
        lightDirection,roughness,f0,base.rgb,metallic)*sunTint*sun.w*visibility*2.5;
    if(materialSurface.w>0.5)
        lit+=base.rgb*sunTint*sun.w*visibility*.18*
            saturate(dot(-lightDirection,viewDirection));
    [unroll] for(int i=0;i<12;++i){
        float3 delta=localLightPosition[i].xyz-input.world;
        float radius=localLightPosition[i].w;
        float distanceToLight=length(delta);
        if(radius<1||distanceToLight>=radius)continue;
        float3 direction=delta/max(distanceToLight,0.001);
        float attenuation=pow(saturate(1-distanceToLight/radius),2);
        float diffuseLocal=max(0,dot(normal,direction));
        float localVisibility=1.0;
        if(headlightShadowInfo.z>0.5&&
           (abs(i-headlightShadowInfo.x)<0.5||
            abs(i-headlightShadowInfo.y)<0.5))
            localVisibility=sampleHeadlightShadow(input.world,diffuseLocal);
        if(streetShadowInfo.y>0.5&&abs(i-streetShadowInfo.x)<0.5)
            localVisibility*=sampleStreetShadow(input.world,diffuseLocal);
        lit+=coatedDirect(normal,coatNormal,viewDirection,direction,
            roughness,f0,base.rgb,metallic)*localLightColor[i].rgb*
            localLightColor[i].w*attenuation*localVisibility*2.8;
    }
    if(road&&precipitation>0.05){
        float fresnelWet=pow(1-ndv,3);
        lit+=fogColor.rgb*(0.08+0.22*fresnelWet)*precipitation;
    }
    if(material==6&&ambient.w>0.5&&probeInfo.z<0.5){
        float3 reflected=reflect(-viewDirection,normal);
        float horizon=saturate(reflected.y*0.5+0.5);
        float3 environment=lerp(fogColor.rgb*0.65,fogColor.rgb*1.45,horizon);
        float fresnelPaint=0.08+0.42*pow(1-ndv,3);
        lit+=environment*fresnelPaint*(1-roughness)*
            (ambient.w>1.5?1.0:0.65);
    }
    lit+=((mapFlags&8)!=0?modelEmissiveTexture.Sample(modelSampler,input.uv).rgb:
        base.rgb)*materialPbr.z;
    if(probeInfo.w>1.5)lit=spatialProbeWeights(input.world);
    else if(probeInfo.w>0.5)lit=indirect;
    float distanceToEye=distance(input.world,eye.xyz);
    float fog=saturate((distanceToEye-params.y)/max(1,params.z-params.y));
    if(probeInfo.w>1.5)fog=0;
    float reflectionMask=material==3?0.10:
        road&&precipitation>0.05?1.0-0.78*precipitation:base.a;
    if(glass&&material==13)reflectionMask=lerp(base.a,1,
        fresnelSchlick(dielectricF0.xxx,ndv).x);
    output.color=float4(lerp(lit,fogColor.rgb,fog),reflectionMask);
    output.surface=float4(normal*0.5+0.5,roughness);
    output.indirect=float4(indirect*(1-fog),temporalInfo.x);
    output.reflectionResponse.rgb*=1-fog;
    return output;
}
void PSShadowAlpha(Output input){
    float4 base=diffuseTexture.Sample(modelSampler,input.uv);
    clip(base.a-materialOptions.x);
    if(base.g>base.r*1.18){
        float keep=smoothstep(11.0,34.0,distance(input.world,eye.xyz));
        float stableNoise=frac(sin(dot(floor(input.world.xz*0.72),
            float2(12.9898,78.233)))*43758.5453);
        clip(keep-stableNoise);
    }
}
)HLSL";

inline const char* hudShader = R"HLSL(
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

inline const char* reflectionShader = R"HLSL(
cbuffer Post : register(b0){
    row_major float4x4 viewProjection;
    row_major float4x4 inverseViewProjection;
    float4 cameraEye;float4 pixelSize;float4 grade;float4 effects;
    float4 skyTop;float4 skyHorizon;float4 debug;
};
Texture2D sceneColor : register(t0);
Texture2D sceneDepth : register(t1);
Texture2D sceneSurface : register(t2);
Texture2D sceneReflectionResponse : register(t3);
SamplerState linearSampler : register(s0);
struct Input {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
float linearDepth(float d){return pixelSize.z*pixelSize.w/
    max(0.001,pixelSize.w-d*(pixelSize.w-pixelSize.z));}
float4 PS(Input input):SV_TARGET{
    int2 texel=int2(input.position.xy)*2;
    float d=sceneDepth.Load(int3(texel,0)).r;
    float4 surface=sceneColor.Load(int3(texel,0));
    float4 geometry=sceneSurface.Load(int3(texel,0));
    float depth=linearDepth(d);
    float4 response=sceneReflectionResponse.Load(int3(texel,0));
    if(d>=0.9999||geometry.w>=0.9||
       (probeInfo.z<0.5&&surface.a>=0.95)||
       (probeInfo.z>0.5&&max(response.r,max(response.g,response.b))<0.01))
        return float4(0,0,0,depth);
    float2 uv=input.uv;
    float4 clip=float4(uv.x*2-1,1-uv.y*2,d,1);
    float4 world=mul(clip,inverseViewProjection);
    world.xyz/=world.w;
    float3 incoming=normalize(world.xyz-cameraEye.xyz);
    float3 normal=normalize(geometry.xyz*2-1);
    float3 reflected=reflect(incoming,normal);
    float3 environment=probeInfo.z>0.5?
        specularProbe(world.xyz,reflected,geometry.w):skyHorizon.rgb;
    float3 reflection=environment;
    bool hit=false;
    int steps=effects.w>1.5?24:10;
    [loop] for(int i=0;i<steps;++i){
        float3 ray=world.xyz+reflected*(6+i*(effects.w>1.5?9:17));
        float4 projected=mul(float4(ray,1),viewProjection);
        if(projected.w<=0)break;
        float3 ndc=projected.xyz/projected.w;
        float2 rayUv=float2(ndc.x*0.5+0.5,0.5-ndc.y*0.5);
        if(any(rayUv<0)||any(rayUv>1)||ndc.z<=0||ndc.z>=1)break;
        float hitDepth=sceneDepth.SampleLevel(linearSampler,rayUv,0).r;
        float thickness=linearDepth(ndc.z)-linearDepth(hitDepth);
        if(hitDepth<0.9999&&thickness>1.5&&thickness<25.0){
            reflection=sceneColor.SampleLevel(linearSampler,rayUv,0).rgb;
            hit=true;break;
        }
    }
    float edge=min(min(uv.x,uv.y),min(1-uv.x,1-uv.y));
    if(probeInfo.z>0.5){
        // The scene already contains probe specular. A miss keeps it; a hit
        // replaces only that lobe, avoiding a second environment contribution.
        float weight=hit?0.66*smoothstep(0,0.08,edge)*(1-geometry.w):0;
        return float4((reflection-environment)*response.rgb*weight,depth);
    }
    float weight=(hit?0.66*smoothstep(0,0.08,edge):0.25)*
        (1-surface.a)*(1-geometry.w);
    return float4((reflection-surface.rgb)*weight,depth);
}
)HLSL";

inline const char* motionShader = R"HLSL(
cbuffer Post : register(b0){
    row_major float4x4 viewProjection,inverseViewProjection;
    float4 cameraEye,pixelSize,grade,effects,skyTop,skyHorizon,debug;
    row_major float4x4 previousViewProjection;
    float4 temporal;
};
Texture2D sceneDepth : register(t0);
Texture2D sceneIndirect : register(t1);
Texture2D objectMotion : register(t2);
struct Input {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
float4 PS(Input input):SV_TARGET{
    float depth=sceneDepth.Load(int3(input.position.xy,0)).r;
    float4 animated=objectMotion.Load(int3(input.position.xy,0));
    if(temporal.x>0.5&&depth<0.9999&&animated.w!=0){
        animated.w=animated.w>0?1:0;return animated;
    }
    if(temporal.x<0.5||depth>=0.9999||
       sceneIndirect.Load(int3(input.position.xy,0)).a<0.5)
        return float4(0,0,depth,0);
    float4 clip=float4(input.uv.x*2-1,1-input.uv.y*2,depth,1);
    float4 world=mul(clip,inverseViewProjection);
    world/=max(0.00001,world.w);
    float4 previous=mul(float4(world.xyz,1),previousViewProjection);
    if(previous.w<=0)return float4(0,0,depth,0);
    float3 ndc=previous.xyz/previous.w;
    float2 previousUv=float2(ndc.x*0.5+0.5,0.5-ndc.y*0.5);
    return float4(input.uv-previousUv,ndc.z,
        all(previousUv>=0)&&all(previousUv<=1)&&ndc.z>0&&ndc.z<1?1:0);
}
)HLSL";

inline const char* temporalShader = R"HLSL(
cbuffer Post : register(b0){
    row_major float4x4 viewProjection,inverseViewProjection;
    float4 cameraEye,pixelSize,grade,effects,skyTop,skyHorizon,debug;
    row_major float4x4 previousViewProjection;
    float4 temporal;
};
Texture2D currentColor : register(t0);
Texture2D currentDepth : register(t1);
Texture2D motionBuffer : register(t2);
Texture2D previousColor : register(t3);
SamplerState linearSampler : register(s0);
struct Input {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
float4 PS(Input input):SV_TARGET{
    float2 uv=input.uv;
    float3 center=currentColor.SampleLevel(linearSampler,uv,0).rgb;
    float depth=currentDepth.Load(int3(input.position.xy,0)).r;
    float4 motion=motionBuffer.Load(int3(input.position.xy,0));
    if(temporal.y>0.5){
        if(motion.w<0.5)return float4(0,0,0,1);
        float2 pixels=motion.xy/pixelSize.xy;
        return float4(saturate(float3(0.5+pixels.x*0.03,
            0.5+pixels.y*0.03,0.5+length(pixels)*0.04)),1);
    }
    if(temporal.x<0.5||motion.w<0.5||depth>=0.9999)
        return float4(center,depth);
    float2 previousUv=uv-motion.xy;
    float4 prior=previousColor.SampleLevel(linearSampler,previousUv,0);
    if(abs(prior.a-motion.z)>max(0.002,0.008*(1-motion.z)))
        return float4(center,depth);
    float3 minimum=center,maximum=center;
    [unroll] for(int y=-1;y<=1;++y)
        [unroll] for(int x=-1;x<=1;++x){
            float3 tap=currentColor.SampleLevel(linearSampler,
                saturate(uv+float2(x,y)*pixelSize.xy),0).rgb;
            minimum=min(minimum,tap);maximum=max(maximum,tap);
        }
    float3 bounded=clamp(prior.rgb,minimum,maximum);
    float speed=length(motion.xy/pixelSize.xy);
    float disagreement=length(center-bounded);
    float weight=0.86*exp(-speed*0.05)*saturate(1-disagreement*3.0);
    return float4(lerp(center,bounded,weight),depth);
}
)HLSL";

inline const char* bloomShader = R"HLSL(
cbuffer Bloom : register(b0){float4 sourceInfo;};
Texture2D sourceImage : register(t0);
SamplerState linearSampler : register(s0);
struct Input {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
float3 read(float2 uv){return sourceImage.SampleLevel(linearSampler,saturate(uv),0).rgb;}
float4 PS(Input input):SV_TARGET{
    float2 stepSize=sourceInfo.xy;
    float3 sum=read(input.uv)*4.0;
    sum+=(read(input.uv+float2(-1,0)*stepSize)+
        read(input.uv+float2(1,0)*stepSize)+
        read(input.uv+float2(0,-1)*stepSize)+
        read(input.uv+float2(0,1)*stepSize))*2.0;
    sum+=read(input.uv+float2(-1,-1)*stepSize)+
        read(input.uv+float2(1,-1)*stepSize)+
        read(input.uv+float2(-1,1)*stepSize)+
        read(input.uv+float2(1,1)*stepSize);
    sum/=16.0;
    if(sourceInfo.z>0.5){
        float brightness=dot(sum,float3(0.2126,0.7152,0.0722));
        sum*=max(0,brightness-0.6)/max(0.001,brightness);
    }
    return float4(sum,1);
}
)HLSL";
} // namespace rendering::shaders
