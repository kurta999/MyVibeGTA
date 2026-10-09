#include "fsr2.h"
#include <dx12/ffx_fsr2_dx12.h>
#include "logging.h"
#include <algorithm>
#include <stdexcept>
#include <cstdio>
namespace dx12 {
namespace {
void checked(FfxErrorCode code){if(code!=FFX_OK)throw std::runtime_error("AMD FSR2 failed with error "+std::to_string(int(code)));}
}
void Fsr2::destroy(Device& device){
    device.submit();if(active){checked(ffxFsr2ContextDestroy(&context));active=false;}
    if(outputView){outputView->Release();outputView=nullptr;}if(output){output->Release();output=nullptr;}scratch.clear();mode=0;
}
void Fsr2::configure(Device& device,int w,int h,int quality){
    quality=std::clamp(quality,0,4);if(width==w&&height==h&&mode==quality)return;
    destroy(device);width=w;height=h;renderWidth=w;renderHeight=h;mode=quality;if(!mode)return;
    FfxFsr2ContextDescription desc{};desc.device=ffxGetDeviceDX12(device.native.Get());
    desc.flags=FFX_FSR2_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION;desc.displaySize={uint32_t(w),uint32_t(h)};
    uint32_t rw=0,rh=0;checked(ffxFsr2GetRenderResolutionFromQualityMode(&rw,&rh,w,h,FfxFsr2QualityMode(mode)));renderWidth=int(rw);renderHeight=int(rh);
    desc.maxRenderSize={uint32_t(renderWidth),uint32_t(renderHeight)};scratch.resize(ffxFsr2GetScratchMemorySizeDX12());
    checked(ffxFsr2GetInterfaceDX12(&desc.callbacks,device.native.Get(),scratch.data(),scratch.size()));checked(ffxFsr2ContextCreate(&context,&desc));active=true;
    TextureDesc texture{};texture.Width=w;texture.Height=h;texture.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;texture.BindFlags=Unordered|ShaderInput;
    if(FAILED(device.CreateTexture2D(&texture,nullptr,&output))||FAILED(device.CreateShaderResourceView(output,nullptr,&outputView)))throw std::runtime_error("FSR2 output allocation failed");
    char info[180];std::snprintf(info,sizeof(info),"AMD FSR2 2.2.1 %s: %dx%d -> %dx%d (native DX12 compute)",fsr2Modes[mode],renderWidth,renderHeight,w,h);logging::write(info);
}
void Fsr2::jitter(unsigned frame){if(!active){jitterX=jitterY=0;return;}const int phases=ffxFsr2GetJitterPhaseCount(renderWidth,width);checked(ffxFsr2GetJitterOffset(&jitterX,&jitterY,int(frame%unsigned(phases)),phases));}
View* Fsr2::dispatch(Device& device,Resource* color,Resource* depth,Resource* motion,Resource* reactive,bool reset,float deltaMs,float nearPlane,float farPlane,float fov,float sharpness){
    device.transition(color,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);device.transition(depth,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);device.transition(motion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);device.transition(reactive,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);device.transition(output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    FfxFsr2DispatchDescription desc{};desc.commandList=ffxGetCommandListDX12(device.commands.Get());
    desc.color=ffxGetResourceDX12(&context,color->native.Get(),L"Scene color");desc.depth=ffxGetResourceDX12(&context,depth->native.Get(),L"Scene depth");desc.motionVectors=ffxGetResourceDX12(&context,motion->native.Get(),L"Motion vectors");desc.reactive=ffxGetResourceDX12(&context,reactive->native.Get(),L"Reactive mask");
    desc.output=ffxGetResourceDX12(&context,output->native.Get(),L"FSR2 output",FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    desc.transparencyAndComposition=desc.reactive;
    desc.jitterOffset={jitterX,jitterY};desc.motionVectorScale={-float(renderWidth),-float(renderHeight)};
    desc.renderSize={uint32_t(renderWidth),uint32_t(renderHeight)};desc.enableSharpening=sharpness>0;desc.sharpness=std::clamp(sharpness,0.0f,1.0f);desc.frameTimeDelta=std::clamp(deltaMs,1.0f,1000.0f);desc.preExposure=1;desc.reset=reset;desc.cameraNear=nearPlane;desc.cameraFar=farPlane;desc.cameraFovAngleVertical=fov;desc.viewSpaceToMetersFactor=1;
    checked(ffxFsr2ContextDispatch(&context,&desc));
    if(device.context)device.context->invalidateBindings();
    // AMD restores external resources to these states; the next graphics pass
    // explicitly rebinds the native root signature, descriptor heaps and PSO.
    return outputView;
}
}
