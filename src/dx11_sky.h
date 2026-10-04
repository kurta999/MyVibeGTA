#pragma once
#include <cstdint>
#include <vector>

namespace dx11::sky {
constexpr int noiseSize=128;
// Periodic FBM + cellular shape/detail channels, sampled with trilinear filtering.
std::vector<std::uint8_t> noiseVolume();
inline constexpr const char* shader=R"HLSL(
Texture3D<float4> cloudNoise : register(t8);
SamplerState cloudSampler : register(s1);
float skyHash(float3 p){
    p=frac(p*.1031);p+=dot(p,p.yzx+33.33);
    return frac((p.x+p.y)*p.z);
}
float3 atmosphere(float3 ray){
    float day=skyWeather.w;
    float mu=dot(ray,sunDirection.xyz);
    float horizon=exp(-max(0,ray.y)*3.5);
    float rayleigh=.75*(1+mu*mu);
    float3 zenith=float3(.055,.22,.56)*(.85+.15*rayleigh);
    float3 haze=float3(.42,.62,.82);
    float dusk=(1-smoothstep(.0,.35,abs(sunDirection.y)))*day;
    float toward=pow(saturate(mu*.5+.5),6);
    haze=lerp(haze,float3(.95,.32,.09),dusk*(.25+.65*toward));
    float3 color=lerp(float3(.002,.004,.012),lerp(zenith,haze,horizon),day);
    float mie=.018*(1-.76*.76)/pow(max(.015,1+.76*.76-2*.76*mu),1.5);
    color+=lerp(float3(.9,.42,.16),float3(1,.88,.67),saturate(sunDirection.y*3))*mie*day;
    // A continuous horizon haze, without the old repeating sine-wave ridge.
    color=lerp(color,lerp(float3(.004,.007,.016),haze*.7,day),1-smoothstep(-.08,.02,ray.y));
    return max(0,color);
}
float2 cloudSphere(float3 origin,float3 ray,float radius){
    float b=dot(origin,ray),c=dot(origin,origin)-radius*radius;
    float root=sqrt(max(0,b*b-c));
    return float2(-b-root,-b+root);
}
float cloudDensity(float3 p,bool detail){
    const float planet=250000;
    float altitude=length(float3(p.x-5000,p.y+planet,p.z-5000))-planet;
    float h=(altitude-1800)/900;
    if(h<=0||h>=1)return 0;
    float3 drift=float3(skyWeather.x,0,skyWeather.y);
    float3 uv=(p-drift)*.000085;
    float4 n=cloudNoise.SampleLevel(cloudSampler,uv,0);
    float weather=cloudNoise.SampleLevel(cloudSampler,float3(uv.x*.32,.17,uv.z*.32),0).r;
    float threshold=.66-skyTop.w*.36+(weather-.5)*.16;
    float shape=saturate((n.r*.8+n.g*.2-threshold)/.18);
    // Flat bases and rounded, eroded tops replace opaque ellipsoids.
    float height=smoothstep(0,.13,h)*(1-smoothstep(.48,1,h));
    float density=shape*height;
    if(detail){
        float erosion=cloudNoise.SampleLevel(cloudSampler,uv*4.07+float3(.13,.31,.57),0).b;
        density=saturate((density-(1-erosion)*.17)/.83);
    }
    return density;
}
float4 volumetricSky(float3 ray,float2 pixel,float maxDistance,float3 scene,bool isSky){
    float3 background=isSky?atmosphere(ray):scene;
    // Distant cirrus: stretched fine noise, independently drifting above cumulus.
    if(isSky&&ray.y>.025){
        float2 cirrus=(cameraEye.xz+ray.xz*(4600-cameraEye.y)/ray.y-
            skyWeather.xy*.6)*.00006;
        float wisps=cloudNoise.SampleLevel(cloudSampler,float3(cirrus.x*2,.61,cirrus.y*.4),0).b;
        float filaments=cloudNoise.SampleLevel(cloudSampler,float3(cirrus.x*7,.23,cirrus.y*.8),0).r;
        float alpha=smoothstep(.54,.77,wisps)*smoothstep(.42,.68,filaments)*
            .23*(1-skyTop.w*.6)*smoothstep(.025,.16,ray.y);
        background=lerp(background,lerp(float3(.025,.035,.055),float3(.74,.79,.88),skyWeather.w),alpha);
    }
    float night=1-skyWeather.w;
    if(isSky&&night>.2&&ray.y>0){
        // Angular star field is fixed in world space, not nearby lit scene meshes.
        float2 angular=float2(atan2(ray.z,ray.x)/6.2831853+.5,asin(ray.y)/3.1415927+.5);
        float2 grid=angular*float2(1400,700),cell=floor(grid);
        float seed=skyHash(float3(cell,7));
        float2 starPosition=float2(skyHash(float3(cell,11)),skyHash(float3(cell,19)));
        float star=exp(-dot(frac(grid)-starPosition,frac(grid)-starPosition)*120)*step(.997,seed);
        background+=float3(.7,.8,1)*star*night*smoothstep(0,.16,ray.y)*.6;
        float3 moon=normalize(float3(-sunDirection.x,.4,-sunDirection.z));
        float separation=length(ray-moon);
        float disk=1-smoothstep(.008,.009,separation);
        float craters=.8+.2*cloudNoise.SampleLevel(cloudSampler,ray*25,0).g;
        background+=float3(.62,.69,.8)*disk*craters*night;
        background+=float3(.1,.15,.22)*exp(-separation*separation/.001)*night;
    }
    const float planet=250000;
    float3 origin=float3(cameraEye.x-5000,cameraEye.y+planet,cameraEye.z-5000);
    float2 outer=cloudSphere(origin,ray,planet+2700),inner=cloudSphere(origin,ray,planet+1800);
    float radius=length(origin);
    float start=radius<planet+1800?inner.y:max(0,outer.x);
    float end=radius>=planet+1800&&inner.x>0?inner.x:outer.y;
    end=min(end,min(50000,maxDistance));
    if(start>=end||end<=0||skyTop.w<.001)return float4(background,1);
    int steps=(int)skyWeather.z;
    float stepSize=(end-start)/steps;
    float jitter=frac(52.9829189*frac(dot(pixel,float2(.06711056,.00583715))));
    float transmission=1;float3 scattering=0;
    float day=skyWeather.w;
    float mu=dot(ray,sunDirection.xyz);
    float phase=.6+.4*pow(saturate(mu),8);
    float3 sunColor=lerp(float3(1,.35,.1),float3(1,.94,.83),saturate(sunDirection.y*3));
    [loop] for(int i=0;i<steps;++i){
        float distance=start+(i+jitter)*stepSize;
        float3 p=cameraEye.xyz+ray*distance;
        float density=cloudDensity(p,true);
        if(density>.002){
            float shadow=0;
            [unroll] for(int j=1;j<=3;++j)
                shadow+=cloudDensity(p+sunDirection.xyz*(j*170),false)*170;
            float sun=exp(-shadow*.006);
            float3 ambient=lerp(float3(.009,.014,.027),float3(.23,.30,.4),day);
            float3 lighting=ambient+sunColor*(.32+.95*sun)*phase*day;
            float haze=1-exp(-distance*.000035);
            lighting=lerp(lighting,atmosphere(ray),haze);
            float opacity=1-exp(-density*stepSize*.006);
            scattering+=transmission*opacity*lighting;
            transmission*=1-opacity;
            if(transmission<.008)break;
        }
    }
    return float4(scattering+background*transmission,transmission);
}
float4 volumetricSky(float3 ray,float2 pixel){
    return volumetricSky(ray,pixel,50000,float3(0,0,0),true);
}
)HLSL";
}
