#pragma once

// Shared by the DX11 game and the GPU regression test. All intermediate RGB
// is linear; only CopyPS encodes the final SDR image for a UNORM back buffer.
namespace dx11::tone {
constexpr unsigned meterLevels=8, meterSize=128;
struct Constants {
    float grade[4]{1,1,1,1};
    float adaptation[4]{1.0f/60,1,1,0}; // seconds, reset, automatic, debug bypass
    float options[4]{}; // legacy reference curve
};
static_assert(sizeof(Constants)==48);
inline constexpr const char* shader=R"HLSL(
cbuffer Tone : register(b1) {float4 grade;float4 adaptation;float4 options;};
Texture2D sourceImage : register(t0);
Texture2D auxiliary : register(t1);
SamplerState linearSampler : register(s0);
struct Input {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
float luminance(float3 c){return dot(c,float3(.2126,.7152,.0722));}
float2 MeterPS(Input input):SV_TARGET {
    // Four stratified samples per cell, centre weighting and a reduced sky
    // weight. Meter the composed HDR image before grading, bloom clipping or
    // the HUD. Log luminance prevents a small sun/emissive area dominating.
    float2 sum=0;
    [unroll] for(int y=0;y<2;++y)[unroll] for(int x=0;x<2;++x){
        float2 uv=input.uv+(float2(x,y)-.5)/256.0;
        float3 color=max(0,sourceImage.SampleLevel(linearSampler,uv,0).rgb);
        float depth=auxiliary.SampleLevel(linearSampler,uv,0).r;
        float2 at=(uv-.5)*2;
        float weight=(.25+.75*exp(-dot(at,at)*2))*(depth>=.9999?.35:1);
        sum+=float2(log2(clamp(luminance(color),.0001,16))*weight,weight);
    }
    return sum*.25;
}
float2 ReducePS(Input input):SV_TARGET {
    int2 at=int2(input.position.xy)*2;
    return (sourceImage.Load(int3(at,0)).rg+
        sourceImage.Load(int3(at+int2(1,0),0)).rg+
        sourceImage.Load(int3(at+int2(0,1),0)).rg+
        sourceImage.Load(int3(at+int2(1,1),0)).rg)*.25;
}
float2 ExposurePS(Input input):SV_TARGET {
    float2 meter=sourceImage.Load(int3(0,0,0)).rg;
    float target=clamp(log2(.18)-meter.x/max(.00001,meter.y),log2(.65),log2(2.25));
    if(adaptation.z<.5)return float2(0,0);
    float previous=auxiliary.Load(int3(0,0,0)).r;
    float rate=target<previous?3:1; // bright adaptation is quicker than dark
    float ev=adaptation.y>.5?target:lerp(previous,target,1-exp(-clamp(adaptation.x,0,.1)*rate));
    return float2(ev,target);
}
float filmic(float x){
    // Hable's rational shoulder/toe, normalized to white at 11.2. Apply the
    // curve to luminance to retain hue instead of bending each RGB channel.
    return ((x*(.15*x+.05)+.004)/(x*(.15*x+.5)+.06))-.066666667;
}
float4 TonePS(Input input):SV_TARGET {
    float3 color=max(0,sourceImage.SampleLevel(linearSampler,input.uv,0).rgb);
    if(adaptation.w>.5)return float4(color,1);
    color*=grade.w*exp2(auxiliary.Load(int3(0,0,0)).r);
    if(options.x>.5)return float4(saturate(color/(1+color)*grade.rgb),1);
    color*=grade.rgb;
    float luma=luminance(color);
    float mapped=saturate(filmic(luma*2)/filmic(11.2));
    color*=mapped/max(.00001,luma);
    // Compress out-of-gamut highlights toward neutral at the same luminance.
    // This preserves colored lamps below the shoulder and lets hot cores whiten.
    float peak=max(color.r,max(color.g,color.b));
    float chroma=peak>1?saturate((1-mapped)/max(.00001,peak-mapped)):1;
    return float4(saturate(lerp(mapped.xxx,color,chroma)),1);
}
float3 encodeSRGB(float3 value){
    value=saturate(value);
    return float3(value.r<=.0031308?value.r*12.92:1.055*pow(value.r,1/2.4)-.055,
        value.g<=.0031308?value.g*12.92:1.055*pow(value.g,1/2.4)-.055,
        value.b<=.0031308?value.b*12.92:1.055*pow(value.b,1/2.4)-.055);
}
float4 CopyPS(Input input):SV_TARGET {
    float3 color=sourceImage.SampleLevel(linearSampler,input.uv,0).rgb;
    if(adaptation.w>.5)return float4(color,1);
    return float4(options.x>.5?pow(saturate(color),1/2.2):encodeSRGB(color),1);
}
)HLSL";
}
