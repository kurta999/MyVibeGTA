#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "physics.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include "game.h"
#include "camera.h"
#include "dx11_assets.h"
#include "dx11_texture_mips.h"
#include "dx11_texture_loading.h"
#include "dx11_shader_loading.h"
#include "cpu_jobs.h"
#include "dx11_probes.h"
#include "dx11_tonemapping.h"
#include "dx11_sky.h"
#include "ui.h"
#include "weather.h"
#include "regions.h"
#include "commerce.h"
#include "fire.h"
#include "logging.h"
#include "startup.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {
namespace {
using namespace DirectX;
IDXGISwapChain* swapChain=nullptr;
ID3D11Device* device=nullptr;
ID3D11DeviceContext* context=nullptr;
std::unique_ptr<cpu::Pool> scenePool;
ID3D11RenderTargetView* target=nullptr;
ID3D11Texture2D* sceneTexture=nullptr;
ID3D11RenderTargetView* sceneTarget=nullptr;
ID3D11ShaderResourceView* sceneView=nullptr;
ID3D11Texture2D* surfaceTexture=nullptr;
ID3D11RenderTargetView* surfaceTarget=nullptr;
ID3D11ShaderResourceView* surfaceView=nullptr;
ID3D11Texture2D* indirectTexture=nullptr;
ID3D11RenderTargetView* indirectTarget=nullptr;
ID3D11ShaderResourceView* indirectView=nullptr;
ID3D11Texture2D* reflectionResponseTexture=nullptr;
ID3D11RenderTargetView* reflectionResponseTarget=nullptr;
ID3D11ShaderResourceView* reflectionResponseView=nullptr;
ID3D11ShaderResourceView* probeView=nullptr;
ID3D11ShaderResourceView* brdfView=nullptr;
ID3D11Buffer* probeBuffer=nullptr;
dx11::probes::Set probeSet;
struct ProbeConstants {
    XMFLOAT4 positions[2],sh[3][9],timeWeights,info;
};
bool probeBakeActive=false,probeBakeFailed=false;
unsigned probeBakeFrame=0;
constexpr XMFLOAT4 probePositions[2]={{100,42,100,750},{300,42,350,650}};
constexpr float probeHours[3]={12,18.5f,22};
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
ID3D11PixelShader* bloomPS=nullptr;
ID3D11PixelShader* reflectionPS=nullptr;
ID3D11PixelShader* motionPS=nullptr;
ID3D11PixelShader* temporalPS=nullptr;
ID3D11PixelShader* temporalCopyPS=nullptr;
ID3D11PixelShader *meterPS=nullptr,*meterReducePS=nullptr,*exposurePS=nullptr,*tonePS=nullptr;
ID3D11Buffer* toneBuffer=nullptr;
ID3D11Texture2D* composedTexture=nullptr;
ID3D11RenderTargetView* composedTarget=nullptr;
ID3D11ShaderResourceView* composedView=nullptr;
ID3D11Texture2D* meterTexture[dx11::tone::meterLevels]{};
ID3D11RenderTargetView* meterTarget[dx11::tone::meterLevels]{};
ID3D11ShaderResourceView* meterView[dx11::tone::meterLevels]{};
ID3D11Texture2D* exposureTexture[2]{};
ID3D11RenderTargetView* exposureTarget[2]{};
ID3D11ShaderResourceView* exposureView[2]{};
int exposureIndex=0;
bool exposureValid=false;
unsigned exposureFrame=0;
std::chrono::steady_clock::time_point exposureClock{};
ID3D11Buffer* postBuffer=nullptr;
ID3D11ShaderResourceView* cloudNoiseView=nullptr;
ID3D11SamplerState* cloudSampler=nullptr;
ID3D11Buffer* bloomBuffer=nullptr;
ID3D11Texture2D* bloomTexture[3]{};
ID3D11RenderTargetView* bloomTarget[3]{};
ID3D11ShaderResourceView* bloomView[3]{};
ID3D11Texture2D* reflectionTexture=nullptr;
ID3D11RenderTargetView* reflectionTarget=nullptr;
ID3D11ShaderResourceView* reflectionView=nullptr;
ID3D11Texture2D* postTexture=nullptr;
ID3D11RenderTargetView* postTarget=nullptr;
ID3D11ShaderResourceView* postView=nullptr;
ID3D11Texture2D* motionTexture=nullptr;
ID3D11RenderTargetView* motionTarget=nullptr;
ID3D11ShaderResourceView* motionView=nullptr;
ID3D11Texture2D* objectMotionTexture=nullptr;
ID3D11RenderTargetView* objectMotionTarget=nullptr;
ID3D11ShaderResourceView* objectMotionView=nullptr;
ID3D11Texture2D* historyTexture[2]{};
ID3D11RenderTargetView* historyTarget[2]{};
ID3D11ShaderResourceView* historyView[2]{};
int historyIndex=0;
bool historyValid=false;
unsigned temporalFrame=0;
XMFLOAT4X4 previousViewProjection{};
XMFLOAT3 previousCameraEye{},previousCameraForward{};
ID3D11InputLayout* inputLayout=nullptr;
ID3D11InputLayout* instanceLayout=nullptr;
ID3D11Buffer* vertexBuffer=nullptr;
ID3D11Buffer* staticBuffer=nullptr;
ID3D11Buffer* instanceBuffer=nullptr;
ID3D11ComputeShader* skinCS=nullptr;
ID3D11Buffer* skinConstants=nullptr;
ID3D11Buffer* skinOutput=nullptr;
ID3D11UnorderedAccessView* skinOutputUav=nullptr;
ID3D11Buffer* skinPreviousOutput=nullptr;
ID3D11UnorderedAccessView* skinPreviousUav=nullptr;
ID3D11VertexShader* skinVS=nullptr;
ID3D11InputLayout* skinLayout=nullptr;
struct PreviousSkin {dx11::SkinInstance pose;std::uint64_t frame;};
std::unordered_map<std::uint64_t,PreviousSkin> previousSkins;
std::uint64_t skinFrame=0;
std::unordered_map<const dx11::SkinMesh*,ID3D11ShaderResourceView*> skinSources;
std::vector<dx11::SkinInstance> gpuSkins;
size_t skinCapacity=0,skinVertexCount=0;
bool skinValidationDone=false;
struct SkinConstants {
    std::array<float,16> palette[dx11::MAX_GPU_SKIN_JOINTS];
    std::array<float,16> previousPalette[dx11::MAX_GPU_SKIN_JOINTS];
    std::array<float,4> scale,origin,transform,yaw;
    std::array<float,4> previousScale,previousOrigin,previousTransform,previousYaw;
    UINT count,start,joints,padding;
};
static_assert(sizeof(SkinConstants)%16==0);
std::unordered_map<const dx11::Mesh*,ID3D11Buffer*> meshBuffers;
std::unordered_map<const dx11::Mesh*,ID3D11Buffer*> meshIndexBuffers;
std::unordered_map<const dx11::Mesh*,std::uint64_t> meshRevisions;
std::unordered_map<const dx11::Mesh*,ID3D11ShaderResourceView*> modelTextures;
std::unordered_map<std::wstring,ID3D11ShaderResourceView*> sharedModelTextures;
std::unordered_map<std::wstring,ID3D11ShaderResourceView*> pbrTextures;
std::uint64_t loadedTextureBytes=0,loadedDdsBytes=0;
unsigned loadedTextureCount=0,loadedDdsCount=0;
ID3D11Buffer* sceneBuffer=nullptr;
ID3D11RasterizerState* rasterState=nullptr;
ID3D11RasterizerState* shadowRaster=nullptr;
ID3D11Texture2D* shadowTexture=nullptr;
ID3D11DepthStencilView* shadowDepth[3]{};
ID3D11ShaderResourceView* shadowView=nullptr;
ID3D11Texture2D* headlightShadowTexture=nullptr;
ID3D11DepthStencilView* headlightShadowDepth=nullptr;
ID3D11ShaderResourceView* headlightShadowView=nullptr;
ID3D11Texture2D* streetShadowTexture=nullptr;
ID3D11DepthStencilView* streetShadowDepth=nullptr;
ID3D11ShaderResourceView* streetShadowView=nullptr;
ID3D11SamplerState* shadowSampler=nullptr;
int shadowSize=0;
int shadowCascadeCount=0;
ID3D11DepthStencilState* noDepth=nullptr;
ID3D11DepthStencilState* readDepth=nullptr;
ID3D11BlendState* alphaBlend=nullptr;
ID3D11BlendState* hudBlend=nullptr;
ID3D11SamplerState* sampler=nullptr;
ID3D11SamplerState* modelSampler=nullptr;
ID3D11SamplerState* clampSampler=nullptr;
int activeFiltering=-1;
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
struct InstanceData {XMFLOAT4 a,b,c,tint,quaternion;};
struct InstanceBatch {const dx11::Mesh* mesh;int material;UINT start,count;};
std::vector<InstanceData> instanceData;
std::vector<InstanceBatch> instanceBatches;
std::array<std::vector<InstanceBatch>,3> shadowInstanceBatches;
std::vector<InstanceBatch> headlightShadowInstanceBatches;
std::vector<InstanceBatch> streetShadowInstanceBatches;
std::vector<unsigned char> hudPixels;
unsigned int screenshotSequence=0;
bool deviceLost=false;
struct GpuQueries {
    ID3D11Query *disjoint=nullptr,*begin=nullptr,*shadowEnd=nullptr,
        *sceneEnd=nullptr,*postEnd=nullptr;
    bool pending=false;
};
std::array<GpuQueries,8> gpuQueries{};
GpuQueries* currentGpu=nullptr;
unsigned gpuCursor=0;
struct SceneConstants {
    XMFLOAT4X4 viewProjection;
    XMFLOAT4X4 shadowViewProjection[3];
    XMFLOAT4 sun;
    XMFLOAT4 ambient;
    XMFLOAT4 fogColor;
    XMFLOAT4 eye;
    XMFLOAT4 params;
    XMFLOAT4 weatherAndTime;
    XMFLOAT4 materialPbr;
    XMFLOAT4 materialSurface;
    XMFLOAT4 materialOptions;
    XMFLOAT4 shadowInfo;
    XMFLOAT4 localLightPosition[12];
    XMFLOAT4 localLightColor[12];
    XMFLOAT4X4 headlightViewProjection;
    XMFLOAT4 headlightShadowInfo;
    XMFLOAT4X4 streetViewProjection;
    XMFLOAT4 streetShadowInfo;
    XMFLOAT4 temporalInfo;
    XMFLOAT4X4 previousViewProjection;
};
struct PostConstants {XMFLOAT4X4 viewProjection,inverseViewProjection;
    XMFLOAT4 cameraEye,pixelSize,grade,effects,skyTop,skyHorizon,debug;
    XMFLOAT4X4 previousViewProjection;
    XMFLOAT4 temporal,sunDirection,sunScreen,skyWeather;};
template<class T> void release(T*& object){if(object){object->Release();object=nullptr;}}
void releaseGpuQueries(){
    for(auto& frame:gpuQueries){
        release(frame.disjoint);release(frame.begin);release(frame.shadowEnd);
        release(frame.sceneEnd);release(frame.postEnd);frame.pending=false;
    }
    currentGpu=nullptr;gpuCursor=0;
}
bool createGpuQueries(){
    D3D11_QUERY_DESC desc{};
    for(auto& frame:gpuQueries){
        desc.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;
        if(FAILED(device->CreateQuery(&desc,&frame.disjoint))){releaseGpuQueries();return false;}
        desc.Query=D3D11_QUERY_TIMESTAMP;
        if(FAILED(device->CreateQuery(&desc,&frame.begin))||
           FAILED(device->CreateQuery(&desc,&frame.shadowEnd))||
           FAILED(device->CreateQuery(&desc,&frame.sceneEnd))||
           FAILED(device->CreateQuery(&desc,&frame.postEnd))){
            releaseGpuQueries();return false;
        }
    }
    return true;
}
void pollGpuQueries(){
    for(auto& frame:gpuQueries)if(frame.pending){
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT data{};
        if(context->GetData(frame.disjoint,&data,sizeof(data),
            D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)continue;
        UINT64 timestamps[4]{};
        ID3D11Query* queries[]={frame.begin,frame.shadowEnd,frame.sceneEnd,frame.postEnd};
        bool ready=true;
        for(int i=0;i<4;++i)if(context->GetData(queries[i],timestamps+i,
            sizeof(UINT64),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK){ready=false;break;}
        if(!ready)continue;
        frame.pending=false;
        if(data.Disjoint||!data.Frequency)continue;
        double scale=1000.0/double(data.Frequency);
        gpuShadowMs=float((timestamps[1]-timestamps[0])*scale);
        gpuSceneMs=float((timestamps[2]-timestamps[1])*scale);
        gpuPostMs=float((timestamps[3]-timestamps[2])*scale);
    }
}
void beginGpuQueries(){
    currentGpu=nullptr;
    auto& frame=gpuQueries[gpuCursor++%gpuQueries.size()];
    if(!frame.disjoint||frame.pending)return;
    context->Begin(frame.disjoint);
    context->End(frame.begin);
    currentGpu=&frame;
}
void endGpuQueries(){
    if(!currentGpu)return;
    context->End(currentGpu->postEnd);
    context->End(currentGpu->disjoint);
    currentGpu->pending=true;currentGpu=nullptr;
}
void logAdapter(){
    IDXGIDevice* dxgi=nullptr;
    IDXGIAdapter* adapter=nullptr;
    if(FAILED(device->QueryInterface(__uuidof(IDXGIDevice),
        reinterpret_cast<void**>(&dxgi))))return;
    if(SUCCEEDED(dxgi->GetAdapter(&adapter))){
        DXGI_ADAPTER_DESC desc{};
        if(SUCCEEDED(adapter->GetDesc(&desc))){
            char name[160]{};
            WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,name,sizeof(name),nullptr,nullptr);
            char line[256]{};
            std::snprintf(line,sizeof(line),"DX11 adapter: %s (vendor %04X, device %04X, dedicated %llu MiB)",
                name,desc.VendorId,desc.DeviceId,
                static_cast<unsigned long long>(desc.DedicatedVideoMemory/(1024*1024)));
            logging::write(line);
        }
    }
    release(adapter);release(dxgi);
}
std::wstring executableFolder(){
    wchar_t filename[MAX_PATH]{};GetModuleFileNameW(nullptr,filename,MAX_PATH);
    std::wstring result(filename);auto slash=result.find_last_of(L"\\/");
    return result.substr(0,slash);
}
bool createProbes(){
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(ProbeConstants);
    buffer.Usage=D3D11_USAGE_DEFAULT;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&buffer,nullptr,&probeBuffer)))return false;
    std::string error;
    if(!dx11::probes::load(executableFolder()+L"\\assets\\lighting\\showcase.mcpb",probeSet,error)){
        logging::write((error+"; retaining legacy ambient lighting").c_str());return true;
    }
    std::vector<D3D11_SUBRESOURCE_DATA> data(probeSet.subresources.size());
    for(unsigned i=0;i<data.size();++i){
        data[i].pSysMem=probeSet.subresources[i].data();
        data[i].SysMemPitch=std::max(1u,probeSet.size>>(i%probeSet.mips))*8;
    }
    D3D11_TEXTURE2D_DESC texture{};texture.Width=texture.Height=probeSet.size;
    texture.MipLevels=probeSet.mips;texture.ArraySize=dx11::probes::CUBE_COUNT*6;
    texture.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;texture.SampleDesc.Count=1;
    texture.Usage=D3D11_USAGE_IMMUTABLE;texture.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    texture.MiscFlags=D3D11_RESOURCE_MISC_TEXTURECUBE;
    ID3D11Texture2D* resource=nullptr;
    HRESULT result=device->CreateTexture2D(&texture,data.data(),&resource);
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.Format=texture.Format;
    view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
    view.TextureCubeArray.MipLevels=probeSet.mips;
    view.TextureCubeArray.NumCubes=dx11::probes::CUBE_COUNT;
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(resource,&view,&probeView);
    release(resource);
    texture.Width=texture.Height=probeSet.lutSize;texture.MipLevels=texture.ArraySize=1;
    texture.Format=DXGI_FORMAT_R32G32_FLOAT;texture.MiscFlags=0;
    D3D11_SUBRESOURCE_DATA lut{};lut.pSysMem=probeSet.brdf.data();
    lut.SysMemPitch=probeSet.lutSize*2*sizeof(float);
    if(SUCCEEDED(result))result=device->CreateTexture2D(&texture,&lut,&resource);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(resource,nullptr,&brdfView);
    release(resource);
    if(FAILED(result)){
        release(probeView);release(brdfView);probeSet={};
        logging::write("Probe resource allocation failed; retaining legacy ambient lighting");
    }else{
        // Keep the compact SH/position metadata; release CPU texture copies.
        probeSet.subresources.clear();probeSet.brdf.clear();
        logging::write("HDR probes loaded: two local positions, day/dusk/night, nine cubes and BRDF LUT");
    }
    return true;
}
void updateProbes(){
    ProbeConstants constants{};
    bool enabled=probeView&&brdfView&&!probeBakeActive&&
        !std::strstr(GetCommandLineA(),"--no-probes");
    const auto weights=dx11::probes::timeWeights(gameHour);
    constants.timeWeights={weights[0],weights[1],weights[2],0};
    constants.info={float(probeSet.mips?probeSet.mips-1:0),
        1-weather::current().clouds*0.42f,enabled?1.0f:0.0f,
        std::strstr(GetCommandLineA(),"--probe-weight-view")?2.0f:
        std::strstr(GetCommandLineA(),"--probe-view")?1.0f:0.0f};
    for(unsigned p=0;p<2;++p){
        const auto& v=enabled?probeSet.positions[p]:
            std::array<float,4>{probePositions[p].x,probePositions[p].y,probePositions[p].z,probePositions[p].w};
        constants.positions[p]={v[0],v[1],v[2],v[3]};
    }
    for(unsigned provider=0;provider<3;++provider)for(unsigned i=0;i<9;++i){
        XMFLOAT4& sh=constants.sh[provider][i];
        unsigned first=provider==0?6:(provider-1)*3;
        for(unsigned time=0;time<3;++time){const auto& c=probeSet.sh[first+time][i];
            sh.x+=c[0]*weights[time];sh.y+=c[1]*weights[time];sh.z+=c[2]*weights[time];}
    }
    context->UpdateSubresource(probeBuffer,0,nullptr,&constants,0,0);
    context->PSSetConstantBuffers(1,1,&probeBuffer);
    ID3D11ShaderResourceView* views[]={probeView,brdfView};
    context->PSSetShaderResources(11,2,views);
    context->PSSetSamplers(3,1,&clampSampler);
}
bool captureProbe(const SceneConstants& constants){
    ID3D11Texture2D *colorCopy=nullptr,*depthCopy=nullptr;
    D3D11_TEXTURE2D_DESC desc{};sceneTexture->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    HRESULT result=device->CreateTexture2D(&desc,nullptr,&colorCopy);
    depthTexture->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    if(SUCCEEDED(result))result=device->CreateTexture2D(&desc,nullptr,&depthCopy);
    D3D11_MAPPED_SUBRESOURCE color{},depth{};
    bool colorMapped=false,depthMapped=false;
    if(SUCCEEDED(result)){
        context->OMSetRenderTargets(0,nullptr,nullptr);
        context->CopyResource(colorCopy,sceneTexture);context->CopyResource(depthCopy,depthTexture);
        result=context->Map(colorCopy,0,D3D11_MAP_READ,0,&color);colorMapped=SUCCEEDED(result);
        if(SUCCEEDED(result)){result=context->Map(depthCopy,0,D3D11_MAP_READ,0,&depth);depthMapped=SUCCEEDED(result);}
    }
    bool ok=false;
    if(SUCCEEDED(result)){
        unsigned probe=probeBakeFrame/18,state=(probeBakeFrame/6)%3,face=probeBakeFrame%6;
        std::vector<std::uint16_t> pixels(size_t(bufferW)*bufferH*4);
        XMMATRIX inverse=XMMatrixInverse(nullptr,XMLoadFloat4x4(&constants.viewProjection));
        float solar=std::sin((gameHour-6)*PI/12),day=std::clamp(solar*2.3f+0.42f,0.0f,1.0f);
        float twilight=std::max(0.0f,1.0f-std::abs(solar)*4.0f);
        float top[3]={0.015f+0.10f*day+twilight*0.10f,0.025f+0.32f*day-twilight*0.07f,0.09f+0.60f*day};
        float horizon[3]={0.055f+0.55f*day+twilight*0.34f,0.075f+0.68f*day-twilight*0.18f,0.15f+0.69f*day-twilight*0.27f};
        for(int y=0;y<bufferH;++y){
            auto* source=reinterpret_cast<const std::uint16_t*>(static_cast<const char*>(color.pData)+size_t(y)*color.RowPitch);
            auto* depths=reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(depth.pData)+size_t(y)*depth.RowPitch);
            for(int x=0;x<bufferW;++x){auto* output=pixels.data()+(size_t(y)*bufferW+x)*4;
                std::copy_n(source+x*4,4,output);output[3]=DirectX::PackedVector::XMConvertFloatToHalf(1);
                if((depths[x]&0xffffff)>=0xfffff0){
                    XMVECTOR farPoint=XMVector3TransformCoord(XMVectorSet((x+0.5f)*2/bufferW-1,
                        1-(y+0.5f)*2/bufferH,1,1),inverse);
                    XMVECTOR ray=XMVector3Normalize(XMVectorSubtract(farPoint,
                        XMVectorSet(constants.eye.x,constants.eye.y,constants.eye.z,1)));
                    float gradient=std::clamp(0.34f+XMVectorGetY(ray)*1.8f,0.0f,1.0f);
                    for(int c=0;c<3;++c)output[c]=DirectX::PackedVector::XMConvertFloatToHalf(
                        std::max(0.0f,horizon[c]*(1-gradient)+top[c]*gradient));
                }
            }
        }
        auto folder=std::filesystem::path(executableFolder())/"probe-captures";
        std::error_code error;std::filesystem::create_directories(folder,error);
        char filename[48]{};std::snprintf(filename,sizeof(filename),"probe-%u-time-%u-face-%u.hdrface",probe,state,face);
        std::ofstream file(folder/filename,std::ios::binary);
        const std::uint32_t header[]={unsigned(bufferW),probe,state,face};
        file.write("MCENV1\0\0",8);file.write(reinterpret_cast<const char*>(header),sizeof(header));
        file.write(reinterpret_cast<const char*>(&probePositions[probe]),sizeof(XMFLOAT4));
        file.write(reinterpret_cast<const char*>(&gameHour),sizeof(float));
        file.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()*2));
        ok=bool(file);
        if(ok)logging::write(("Captured HDR "+std::string(filename)).c_str());
    }
    if(depthMapped)context->Unmap(depthCopy,0);if(colorMapped)context->Unmap(colorCopy,0);
    release(depthCopy);release(colorCopy);return ok;
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
const char* skinShader=R"HLSL(
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
bool createSkinResources(){
    ID3DBlob* code=nullptr;
    if(!compile(skinShader,"CS","cs_5_0",&code)){release(code);return false;}
    HRESULT result=device->CreateComputeShader(code->GetBufferPointer(),
        code->GetBufferSize(),nullptr,&skinCS);
    release(code);
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=sizeof(SkinConstants);
    desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(SUCCEEDED(result))result=device->CreateBuffer(&desc,nullptr,&skinConstants);
    if(FAILED(result)){release(skinCS);release(skinConstants);return false;}
    return true;
}
bool cacheSkin(const dx11::SkinMesh* skin){
    if(skinSources.count(skin))return true;
    if(skin->jointCount==0||skin->jointCount>dx11::MAX_GPU_SKIN_JOINTS||
       skin->vertices.empty()||skin->vertices.size()>300000)return false;
    static_assert(sizeof(dx11::SkinVertex)==68&&sizeof(dx11::Vertex)==48);
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth=UINT(skin->vertices.size()*sizeof(dx11::SkinVertex));
    desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride=sizeof(dx11::SkinVertex);
    D3D11_SUBRESOURCE_DATA data{};data.pSysMem=skin->vertices.data();
    ID3D11Buffer* buffer=nullptr;ID3D11ShaderResourceView* view=nullptr;
    HRESULT result=device->CreateBuffer(&desc,&data,&buffer);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(buffer,nullptr,&view);
    release(buffer);
    if(FAILED(result)){release(view);return false;}
    skinSources.emplace(skin,view);return true;
}
bool growSkinBuffer(size_t count){
    if(count<=skinCapacity&&skinOutput&&skinOutputUav&&skinPreviousOutput&&skinPreviousUav)return true;
    if(count>3000000)return false;
    size_t capacity=std::min(size_t(3000000),std::max(count,skinCapacity*2+100000));
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=UINT(capacity*sizeof(dx11::Vertex));
    desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_VERTEX_BUFFER;
    desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    ID3D11Buffer* buffer=nullptr;ID3D11UnorderedAccessView* view=nullptr;
    HRESULT result=device->CreateBuffer(&desc,nullptr,&buffer);
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;
    uav.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=desc.ByteWidth/4;
    uav.Buffer.Flags=D3D11_BUFFER_UAV_FLAG_RAW;
    if(SUCCEEDED(result))result=device->CreateUnorderedAccessView(buffer,&uav,&view);
    ID3D11Buffer* previousBuffer=nullptr;ID3D11UnorderedAccessView* previousView=nullptr;
    desc.ByteWidth=UINT(capacity*16);uav.Buffer.NumElements=desc.ByteWidth/4;
    if(SUCCEEDED(result))result=device->CreateBuffer(&desc,nullptr,&previousBuffer);
    if(SUCCEEDED(result))result=device->CreateUnorderedAccessView(previousBuffer,&uav,&previousView);
    if(FAILED(result)){release(buffer);release(view);release(previousBuffer);release(previousView);return false;}
    release(skinOutputUav);release(skinOutput);
    release(skinPreviousUav);release(skinPreviousOutput);
    skinOutput=buffer;skinOutputUav=view;skinCapacity=capacity;
    skinPreviousOutput=previousBuffer;skinPreviousUav=previousView;
    return true;
}
bool validateSkinOutput(const std::vector<dx11::SkinInstance>& previous,
                        const std::vector<unsigned>& validity){
    D3D11_BUFFER_DESC desc{};skinOutput->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    ID3D11Buffer* staging=nullptr;
    if(FAILED(device->CreateBuffer(&desc,nullptr,&staging)))return false;
    context->CopyResource(staging,skinOutput);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT result=context->Map(staging,0,D3D11_MAP_READ,0,&mapped);
    bool valid=SUCCEEDED(result);float maxError=0;
    if(valid){
        std::vector<dx11::Vertex> expected;
        for(const auto& instance:gpuSkins)dx11::deformSkinCpu(instance,expected);
        const auto* actual=static_cast<const float*>(mapped.pData);
        for(size_t i=0;i<expected.size()&&valid;++i){
            float reference[12];std::memcpy(reference,&expected[i],sizeof(reference));
            for(int field=0;field<12;++field){
                float error=std::abs(actual[i*12+field]-reference[field]);
                maxError=std::max(maxError,error);
                float tolerance=field<3?0.003f:0.00003f;
                if(!std::isfinite(actual[i*12+field])||error>tolerance)valid=false;
            }
        }
        context->Unmap(staging,0);
    }
    release(staging);
    skinPreviousOutput->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;
    desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    if(FAILED(device->CreateBuffer(&desc,nullptr,&staging)))return false;
    context->CopyResource(staging,skinPreviousOutput);
    result=context->Map(staging,0,D3D11_MAP_READ,0,&mapped);
    if(SUCCEEDED(result)){
        const auto* actual=static_cast<const float*>(mapped.pData);size_t start=0;
        for(size_t pose=0;pose<previous.size();++pose){
            std::vector<dx11::Vertex> expected;dx11::deformSkinCpu(previous[pose],expected);
            for(size_t i=0;i<expected.size();++i){
                float reference[3]={expected[i].x,expected[i].y,expected[i].z};
                for(int axis=0;axis<3;++axis){
                    float value=actual[(start+i)*4+axis];
                    float error=std::abs(value-reference[axis]);maxError=std::max(maxError,error);
                    if(!std::isfinite(value)||error>0.003f)valid=false;
                }
                if(actual[(start+i)*4+3]!=float(validity[pose]))valid=false;
            }
            start+=expected.size();
        }
        context->Unmap(staging,0);
    }else valid=false;
    release(staging);
    char line[180]{};
    std::snprintf(line,sizeof(line),"GPU skin validation: %s; frame %llu, %zu vertices, %zu poses, %u prior poses, max error %.8f",
        valid?"PASS":"FAIL",static_cast<unsigned long long>(skinFrame),skinVertexCount,
        gpuSkins.size(),unsigned(std::count(validity.begin(),validity.end(),1u)),maxError);
    logging::write(line);return valid;
}
void prepareSkins(){
    ++skinFrame;
    skinVertexCount=0;
    for(const auto& instance:gpuSkins)skinVertexCount+=instance.source->vertices.size();
    if(skinVertexCount==0)return;
    bool ready=skinCS&&skinConstants&&growSkinBuffer(skinVertexCount);
    for(const auto& instance:gpuSkins)ready=ready&&
        instance.palette.size()==instance.source->jointCount&&cacheSkin(instance.source);
    if(ready){
        const bool validate=std::strstr(GetCommandLineA(),"--validate-gpu-skinning")&&
            (skinFrame==1||skinFrame==30||skinFrame==60);
        std::vector<dx11::SkinInstance> priorPoses;
        std::vector<unsigned> priorValidity;
        // A previous frame may still have this output bound to the input assembler.
        ID3D11Buffer* emptyBuffers[2]{};UINT zeros[2]{};
        context->IASetVertexBuffers(0,2,emptyBuffers,zeros,zeros);
        context->CSSetShader(skinCS,nullptr,0);
        context->CSSetConstantBuffers(0,1,&skinConstants);
        ID3D11UnorderedAccessView* outputViews[2]={skinOutputUav,skinPreviousUav};
        context->CSSetUnorderedAccessViews(0,2,outputViews,nullptr);
        UINT start=0;
        for(const auto& instance:gpuSkins){
            SkinConstants constants{};
            std::copy(instance.palette.begin(),instance.palette.end(),constants.palette);
            constants.scale=instance.scale;constants.origin=instance.origin;
            constants.transform=instance.transform;constants.yaw=instance.yaw;
            constants.count=UINT(instance.source->vertices.size());constants.start=start;
            constants.joints=instance.source->jointCount;
            auto prior=previousSkins.find(instance.identity);
            bool valid=instance.identity!=0&&prior!=previousSkins.end()&&
                prior->second.frame+1==skinFrame&&prior->second.pose.source==instance.source;
            if(valid){
                float distanceSquared=0;
                for(int axis=0;axis<3;++axis){
                    float delta=instance.transform[axis]-prior->second.pose.transform[axis];
                    distanceSquared+=delta*delta;
                }
                valid=distanceSquared<65*65;
            }
            const auto& previous=valid?prior->second.pose:instance;
            std::copy(previous.palette.begin(),previous.palette.end(),constants.previousPalette);
            constants.previousScale=previous.scale;constants.previousOrigin=previous.origin;
            constants.previousTransform=previous.transform;constants.previousYaw=previous.yaw;
            constants.padding=valid?1:0;
            if(validate){priorPoses.push_back(previous);priorValidity.push_back(constants.padding);}
            context->UpdateSubresource(skinConstants,0,nullptr,&constants,0,0);
            auto* source=skinSources.at(instance.source);
            context->CSSetShaderResources(0,1,&source);
            context->Dispatch((constants.count+63)/64,1,1);start+=constants.count;
        }
        ID3D11UnorderedAccessView* emptyUavs[2]{};
        ID3D11ShaderResourceView* emptyView=nullptr;
        context->CSSetUnorderedAccessViews(0,2,emptyUavs,nullptr);
        context->CSSetShaderResources(0,1,&emptyView);
        context->CSSetShader(nullptr,nullptr,0);
        if(!skinValidationDone){
            char line[140]{};
            std::snprintf(line,sizeof(line),"GPU skinning: %zu ordinary poses, %zu vertices; shared color/shadow buffer",
                gpuSkins.size(),skinVertexCount);logging::write(line);
            skinValidationDone=true;
        }
        if(validate)ready=validateSkinOutput(priorPoses,priorValidity);
        for(auto item=previousSkins.begin();item!=previousSkins.end();){
            if(item->second.frame+1<skinFrame)item=previousSkins.erase(item);
            else ++item;
        }
        for(const auto& instance:gpuSkins)if(instance.identity!=0)
            previousSkins.insert_or_assign(instance.identity,PreviousSkin{instance,skinFrame});
    }
    if(!ready){
        for(const auto& instance:gpuSkins)dx11::deformSkinCpu(instance,groups[5]);
        skinVertexCount=0;
        release(skinCS);logging::write("GPU skinning unavailable; using CPU deformation");
    }
}
const char* probeShader=R"HLSL(
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
// Keep each source literal below the Visual Studio 2022 compiler's size limit.
const char* sceneShaderPrelude=R"HLSL(
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
const char* sceneShaderPixel=R"HLSL(SceneOutput PS(Output input){
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
const std::string postShader=std::string(R"HLSL(
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
)HLSL")+dx11::sky::shader+R"HLSL(
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
const char* reflectionShader=R"HLSL(
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
const char* motionShader=R"HLSL(
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
const char* temporalShader=R"HLSL(
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
const char* bloomShader=R"HLSL(
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
unsigned loadingWorkers();
bool createShaders(){
    const std::string sceneSource=std::string(probeShader)+
        sceneShaderPrelude+sceneShaderPixel;
    const std::string reflectionSource=std::string(probeShader)+reflectionShader;
    std::vector<dx11::shader::Request> requests={
        {sceneSource,"VSSkinned","vs_5_0"},{sceneSource,"VS","vs_5_0"},
        {sceneSource,"PS","ps_5_0"},{sceneSource,"VSInstanced","vs_5_0"},
        {sceneSource,"PSShadowAlpha","ps_5_0"},{sceneSource,"HS","hs_5_0"},
        {sceneSource,"DS","ds_5_0"},{hudShader,"VS","vs_5_0"},{hudShader,"PS","ps_5_0"},
        {postShader,"PS","ps_5_0"},{bloomShader,"PS","ps_5_0"},
        {reflectionSource,"PS","ps_5_0"},{motionShader,"PS","ps_5_0"},
        {temporalShader,"PS","ps_5_0"},{dx11::tone::shader,"CopyPS","ps_5_0"},
        {dx11::tone::shader,"MeterPS","ps_5_0"},{dx11::tone::shader,"ReducePS","ps_5_0"},
        {dx11::tone::shader,"ExposurePS","ps_5_0"},{dx11::tone::shader,"TonePS","ps_5_0"}};
    std::vector<dx11::shader::Compiled> compiled;dx11::shader::Stats stats;
    bool ok=dx11::shader::compileBatch(requests,loadingWorkers(),[](size_t done,size_t total){
        return startup::report(5+int(25*done/std::max(size_t(1),total)),"Compiling graphics shaders");
    },compiled,stats);
    char timing[200]{};std::snprintf(timing,sizeof(timing),
        "Shader loading: workers %u, peak active %u, completed %zu/%zu, wall %.3f s, CPU %.3f s%s",
        stats.workers,stats.peakActive,stats.completed,requests.size(),stats.wallSeconds,stats.cpuSeconds,ok?"":" (stopped)");
    logging::write(timing);
    if(!ok){
        for(size_t i=0;i<compiled.size();++i)if(!compiled[i].error.empty()){
            std::ofstream log("shader-error.log",std::ios::app);
            if(log)log<<requests[i].entry<<": "<<compiled[i].error<<'\n';
            logging::write("Graphics shader compilation failed; see shader-error.log");
            if(!std::strstr(GetCommandLineA(),"--smoke"))
                MessageBoxA(win,compiled[i].error.c_str(),"Direct3D shader compile error",MB_ICONERROR);
            break;
        }
        return false;
    }
    if(std::strstr(GetCommandLineA(),"--validate-loading")){
        std::uint64_t checksum=14695981039346656037ULL;
        for(const auto& item:compiled){const auto* bytes=static_cast<const unsigned char*>(item.blob->GetBufferPointer());
            for(size_t i=0;i<item.blob->GetBufferSize();++i)checksum=(checksum^bytes[i])*1099511628211ULL;}
        std::snprintf(timing,sizeof(timing),"Shader checksum: %016llx",static_cast<unsigned long long>(checksum));logging::write(timing);
    }
    ID3DBlob *skinned=nullptr,*vs=nullptr,*instanced=nullptr,*ps=nullptr,*shadowAlpha=nullptr,*hull=nullptr,*domain=nullptr,*hudVertex=nullptr,*hudPixel=nullptr,*postPixel=nullptr,*bloomPixel=nullptr,*reflectionPixel=nullptr,*motionPixel=nullptr,*temporalPixel=nullptr,*temporalCopyPixel=nullptr;
    ID3DBlob *meterPixel=nullptr,*reducePixel=nullptr,*exposurePixel=nullptr,*tonePixel=nullptr;
    ID3DBlob** destinations[]={&skinned,&vs,&ps,&instanced,&shadowAlpha,&hull,&domain,&hudVertex,&hudPixel,
        &postPixel,&bloomPixel,&reflectionPixel,&motionPixel,&temporalPixel,&temporalCopyPixel,
        &meterPixel,&reducePixel,&exposurePixel,&tonePixel};
    for(size_t i=0;i<compiled.size();++i){*destinations[i]=compiled[i].blob.get();(*destinations[i])->AddRef();}
    HRESULT result=device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&sceneVS);
    if(SUCCEEDED(result))result=device->CreateVertexShader(skinned->GetBufferPointer(),skinned->GetBufferSize(),nullptr,&skinVS);
    if(SUCCEEDED(result))result=device->CreateVertexShader(instanced->GetBufferPointer(),instanced->GetBufferSize(),nullptr,&instanceVS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&scenePS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(shadowAlpha->GetBufferPointer(),shadowAlpha->GetBufferSize(),nullptr,&alphaShadowPS);
    if(SUCCEEDED(result))result=device->CreateHullShader(hull->GetBufferPointer(),hull->GetBufferSize(),nullptr,&sceneHS);
    if(SUCCEEDED(result))result=device->CreateDomainShader(domain->GetBufferPointer(),domain->GetBufferSize(),nullptr,&sceneDS);
    if(SUCCEEDED(result))result=device->CreateVertexShader(hudVertex->GetBufferPointer(),hudVertex->GetBufferSize(),nullptr,&hudVS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(hudPixel->GetBufferPointer(),hudPixel->GetBufferSize(),nullptr,&hudPS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(postPixel->GetBufferPointer(),postPixel->GetBufferSize(),nullptr,&postPS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(bloomPixel->GetBufferPointer(),bloomPixel->GetBufferSize(),nullptr,&bloomPS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(reflectionPixel->GetBufferPointer(),reflectionPixel->GetBufferSize(),nullptr,&reflectionPS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(motionPixel->GetBufferPointer(),motionPixel->GetBufferSize(),nullptr,&motionPS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(temporalPixel->GetBufferPointer(),temporalPixel->GetBufferSize(),nullptr,&temporalPS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(temporalCopyPixel->GetBufferPointer(),temporalCopyPixel->GetBufferSize(),nullptr,&temporalCopyPS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(meterPixel->GetBufferPointer(),meterPixel->GetBufferSize(),nullptr,&meterPS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(reducePixel->GetBufferPointer(),reducePixel->GetBufferSize(),nullptr,&meterReducePS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(exposurePixel->GetBufferPointer(),exposurePixel->GetBufferSize(),nullptr,&exposurePS);
    if(SUCCEEDED(result))result=device->CreatePixelShader(tonePixel->GetBufferPointer(),tonePixel->GetBufferSize(),nullptr,&tonePS);
    D3D11_INPUT_ELEMENT_DESC layout[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(SUCCEEDED(result))result=device->CreateInputLayout(layout,4,vs->GetBufferPointer(),
        vs->GetBufferSize(),&inputLayout);
    D3D11_INPUT_ELEMENT_DESC skinElements[5]{};
    std::copy(std::begin(layout),std::end(layout),skinElements);
    skinElements[4]={"POSITION",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,0,D3D11_INPUT_PER_VERTEX_DATA,0};
    if(SUCCEEDED(result))result=device->CreateInputLayout(skinElements,5,
        skinned->GetBufferPointer(),skinned->GetBufferSize(),&skinLayout);
    D3D11_INPUT_ELEMENT_DESC instanceElements[]={
        {"INSTANCE",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,16,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,32,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,48,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"INSTANCE",4,DXGI_FORMAT_R32G32B32A32_FLOAT,1,64,D3D11_INPUT_PER_INSTANCE_DATA,1}};
    D3D11_INPUT_ELEMENT_DESC fullLayout[9]{};
    std::copy(std::begin(layout),std::end(layout),fullLayout);
    std::copy(std::begin(instanceElements),std::end(instanceElements),fullLayout+4);
    if(SUCCEEDED(result))result=device->CreateInputLayout(fullLayout,9,instanced->GetBufferPointer(),
        instanced->GetBufferSize(),&instanceLayout);
    release(vs);release(instanced);release(ps);release(shadowAlpha);release(hull);release(domain);
    release(hudVertex);release(hudPixel);release(postPixel);release(bloomPixel);
    release(reflectionPixel);release(motionPixel);release(temporalPixel);
    release(temporalCopyPixel);release(meterPixel);release(reducePixel);release(exposurePixel);release(tonePixel);
    release(skinned);
    return SUCCEEDED(result);
}
std::wstring textureKey(const std::wstring& file,dx11::texture::Kind kind){
    return file+L"#"+std::to_wstring(int(kind));
}
bool loadTexture(const std::wstring& file,ID3D11ShaderResourceView** view,
    dx11::texture::Kind kind=dx11::texture::Kind::Color);
bool uploadTexture(const dx11::texture::Prepared& prepared,dx11::texture::Kind kind,
                   ID3D11ShaderResourceView** view){
    const auto& levels=prepared.levels;
    if(levels.empty())return false;
    std::vector<D3D11_SUBRESOURCE_DATA> initial(levels.size());
    for(size_t i=0;i<levels.size();++i){
        initial[i].pSysMem=levels[i].pixels.data();
        initial[i].SysMemPitch=dx11::texture::rowPitch(levels[i].width,prepared.format);
        if(levels[i].pixels.size()!=size_t(initial[i].SysMemPitch)*
            dx11::texture::rowCount(levels[i].height,prepared.format))return false;
    }
    D3D11_TEXTURE2D_DESC description{};description.Width=levels[0].width;description.Height=levels[0].height;
    description.MipLevels=UINT(levels.size());description.ArraySize=1;
    const bool color=kind==dx11::texture::Kind::Color||kind==dx11::texture::Kind::MaskedColor;
    switch(prepared.format){
    case dx11::texture::Format::Bgra8:description.Format=color?DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:DXGI_FORMAT_B8G8R8A8_UNORM;break;
    case dx11::texture::Format::Rgba8:description.Format=color?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;break;
    case dx11::texture::Format::Bc7:description.Format=color?DXGI_FORMAT_BC7_UNORM_SRGB:DXGI_FORMAT_BC7_UNORM;break;
    case dx11::texture::Format::Bc5:description.Format=DXGI_FORMAT_BC5_UNORM;break;
    case dx11::texture::Format::Bc4:description.Format=DXGI_FORMAT_BC4_UNORM;break;
    }
    description.SampleDesc.Count=1;description.Usage=D3D11_USAGE_IMMUTABLE;
    description.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D* texture=nullptr;
    HRESULT status=device->CreateTexture2D(&description,initial.data(),&texture);
    if(SUCCEEDED(status))status=device->CreateShaderResourceView(texture,nullptr,view);
    if(SUCCEEDED(status)&&prepared.format>=dx11::texture::Format::Bc7&&
       std::strstr(GetCommandLineA(),"--validate-loading")){
        // Verify every block of every immutable GPU mip against the cook.
        auto readbackDescription=description;readbackDescription.Usage=D3D11_USAGE_STAGING;
        readbackDescription.BindFlags=0;readbackDescription.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* readback=nullptr;
        status=device->CreateTexture2D(&readbackDescription,nullptr,&readback);
        if(SUCCEEDED(status)){
            context->CopyResource(readback,texture);
            for(UINT i=0;i<description.MipLevels&&SUCCEEDED(status);++i){
                D3D11_MAPPED_SUBRESOURCE mapped{};status=context->Map(readback,i,D3D11_MAP_READ,0,&mapped);
                if(FAILED(status))break;
                const auto pitch=initial[i].SysMemPitch;
                for(unsigned row=0;row<dx11::texture::rowCount(levels[i].height,prepared.format);++row)
                    if(std::memcmp(static_cast<const unsigned char*>(mapped.pData)+size_t(row)*mapped.RowPitch,
                        levels[i].pixels.data()+size_t(row)*pitch,pitch)!=0){status=E_FAIL;break;}
                context->Unmap(readback,i);
            }
        }
        release(readback);
        char line[180]{};std::snprintf(line,sizeof(line),
            "DDS GPU mip verification: %ux%u, format %u, %u mips: %s",description.Width,
            description.Height,unsigned(description.Format),description.MipLevels,SUCCEEDED(status)?"passed":"FAILED");
        logging::write(line);
        if(FAILED(status))release(*view);
    }
    if(SUCCEEDED(status)){
        std::uint64_t bytes=0;for(const auto& level:levels)bytes+=level.pixels.size();
        loadedTextureBytes+=bytes;++loadedTextureCount;
        if(prepared.format>=dx11::texture::Format::Bc7){loadedDdsBytes+=bytes;++loadedDdsCount;}
    }
    release(texture);return SUCCEEDED(status);
}
bool loadTexture(const std::wstring& file,ID3D11ShaderResourceView** view,
    dx11::texture::Kind kind){
    return uploadTexture(dx11::texture::prepare({file,kind}),kind,view);
}
unsigned loadingWorkers(){
    const char* option=std::strstr(GetCommandLineA(),"--loader-workers=");
    if(!option)return dx11::texture::defaultLoadingWorkers();
    char* end=nullptr;auto value=std::strtoul(option+17,&end,10);
    return end!=option+17&&value>=1&&value<=8?unsigned(value):dx11::texture::defaultLoadingWorkers();
}
unsigned sceneWorkers(){
    const char* option=std::strstr(GetCommandLineA(),"--scene-workers=");
    if(!option)return dx11::texture::defaultLoadingWorkers();
    char* end=nullptr;auto value=std::strtoul(option+16,&end,10);
    return end!=option+16&&value>=1&&value<=8?unsigned(value):dx11::texture::defaultLoadingWorkers();
}
bool preloadTextures(const std::vector<dx11::texture::Request>& requests,
    const std::function<bool(size_t,const dx11::texture::Prepared&)>& consume,
    int start,int span,const char* stage){
    dx11::texture::LoadingStats stats;
    const bool validate=std::strstr(GetCommandLineA(),"--validate-loading")!=nullptr;
    std::uint64_t checksum=14695981039346656037ULL;
    bool ok=dx11::texture::prepareBatch(requests,loadingWorkers(),
        [&](size_t index,const dx11::texture::Prepared& prepared){
            if(!prepared.error.empty()||prepared.levels.empty()){
                logging::write(("Texture preparation failed: "+std::filesystem::path(requests[index].file).u8string()+
                    ": "+prepared.error).c_str());return false;
            }
            if(validate)for(const auto& level:prepared.levels)for(auto byte:level.pixels)
                checksum=(checksum^byte)*1099511628211ULL;
            return consume(index,prepared);
        },[&](size_t done,size_t total){return startup::report(start+int(span*done/std::max(size_t(1),total)),stage);},stats);
    char line[320]{};
    std::snprintf(line,sizeof(line),
        "Texture loading: %s; workers %u, peak active %u, completed %zu/%zu, pending <= %zu, wall %.3f s, decode CPU %.3f s, mip CPU %.3f s, upload %.3f s%s",
        stage,stats.workers,stats.peakActive,stats.completed,requests.size(),stats.peakPending,
        stats.wallSeconds,stats.decodeSeconds,stats.mipSeconds,stats.consumeSeconds,ok?"":" (stopped)");
    logging::write(line);
    if(validate){std::snprintf(line,sizeof(line),"Texture checksum: %s %016llx",stage,
        static_cast<unsigned long long>(checksum));logging::write(line);}
    return ok;
}
bool cachePbrTexture(const std::wstring& file,
    dx11::texture::Kind kind=dx11::texture::Kind::Color){
    auto key=textureKey(file,kind);
    if(file.empty()||pbrTextures.count(key))return true;
    ID3D11ShaderResourceView* view=nullptr;
    auto cached=sharedModelTextures.find(key);
    if(cached!=sharedModelTextures.end()){view=cached->second;view->AddRef();}
    else if(!loadTexture(file,&view,kind))return false;
    pbrTextures.emplace(key,view);
    return true;
}
ID3D11ShaderResourceView* pbrTexture(const std::wstring& file,
    dx11::texture::Kind kind=dx11::texture::Kind::Color){
    auto found=pbrTextures.find(textureKey(file,kind));
    return found==pbrTextures.end()?nullptr:found->second;
}
bool bc5Normal(const std::wstring& file){
    auto* view=pbrTexture(file,dx11::texture::Kind::Normal);if(!view)return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC description{};view->GetDesc(&description);
    return description.Format==DXGI_FORMAT_BC5_UNORM;
}
bool createTargets(int width,int height){
    if(width<1||height<1)return false;
    if(bufferW&&FAILED(swapChain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)))return false;
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
    if(SUCCEEDED(result))result=device->CreateTexture2D(&sceneDescription,nullptr,&surfaceTexture);
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(surfaceTexture,nullptr,&surfaceTarget);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(surfaceTexture,nullptr,&surfaceView);
    if(SUCCEEDED(result))result=device->CreateTexture2D(&sceneDescription,nullptr,&indirectTexture);
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(indirectTexture,nullptr,&indirectTarget);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(indirectTexture,nullptr,&indirectView);
    if(SUCCEEDED(result))result=device->CreateTexture2D(&sceneDescription,nullptr,&reflectionResponseTexture);
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(reflectionResponseTexture,nullptr,&reflectionResponseTarget);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(reflectionResponseTexture,nullptr,&reflectionResponseView);
    if(SUCCEEDED(result))result=device->CreateTexture2D(&sceneDescription,nullptr,&objectMotionTexture);
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(objectMotionTexture,nullptr,&objectMotionTarget);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(objectMotionTexture,nullptr,&objectMotionView);
    if(FAILED(result))return false;
    for(int level=0;level<3;++level){
        sceneDescription.Width=UINT(std::max(1,width>>(level+1)));
        sceneDescription.Height=UINT(std::max(1,height>>(level+1)));
        result=device->CreateTexture2D(&sceneDescription,nullptr,&bloomTexture[level]);
        if(SUCCEEDED(result))result=device->CreateRenderTargetView(
            bloomTexture[level],nullptr,&bloomTarget[level]);
        if(SUCCEEDED(result))result=device->CreateShaderResourceView(
            bloomTexture[level],nullptr,&bloomView[level]);
        if(FAILED(result))return false;
    }
    sceneDescription.Width=UINT(std::max(1,width/2));
    sceneDescription.Height=UINT(std::max(1,height/2));
    result=device->CreateTexture2D(&sceneDescription,nullptr,&reflectionTexture);
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(
        reflectionTexture,nullptr,&reflectionTarget);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(
        reflectionTexture,nullptr,&reflectionView);
    if(FAILED(result))return false;
    sceneDescription.Width=UINT(width);
    sceneDescription.Height=UINT(height);
    result=device->CreateTexture2D(&sceneDescription,nullptr,&postTexture);
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(postTexture,nullptr,&postTarget);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(postTexture,nullptr,&postView);
    if(SUCCEEDED(result))result=device->CreateTexture2D(&sceneDescription,nullptr,&motionTexture);
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(motionTexture,nullptr,&motionTarget);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(motionTexture,nullptr,&motionView);
    for(int index=0;index<2&&SUCCEEDED(result);++index){
        result=device->CreateTexture2D(&sceneDescription,nullptr,&historyTexture[index]);
        if(SUCCEEDED(result))result=device->CreateRenderTargetView(
            historyTexture[index],nullptr,&historyTarget[index]);
        if(SUCCEEDED(result))result=device->CreateShaderResourceView(
            historyTexture[index],nullptr,&historyView[index]);
    }
    if(SUCCEEDED(result))result=device->CreateTexture2D(&sceneDescription,nullptr,&composedTexture);
    if(SUCCEEDED(result))result=device->CreateRenderTargetView(composedTexture,nullptr,&composedTarget);
    if(SUCCEEDED(result))result=device->CreateShaderResourceView(composedTexture,nullptr,&composedView);
    sceneDescription.Format=DXGI_FORMAT_R32G32_FLOAT;
    for(unsigned level=0;level<dx11::tone::meterLevels&&SUCCEEDED(result);++level){
        sceneDescription.Width=sceneDescription.Height=dx11::tone::meterSize>>level;
        result=device->CreateTexture2D(&sceneDescription,nullptr,&meterTexture[level]);
        if(SUCCEEDED(result))result=device->CreateRenderTargetView(meterTexture[level],nullptr,&meterTarget[level]);
        if(SUCCEEDED(result))result=device->CreateShaderResourceView(meterTexture[level],nullptr,&meterView[level]);
    }
    sceneDescription.Width=sceneDescription.Height=1;
    for(int index=0;index<2&&SUCCEEDED(result);++index){
        result=device->CreateTexture2D(&sceneDescription,nullptr,&exposureTexture[index]);
        if(SUCCEEDED(result))result=device->CreateRenderTargetView(exposureTexture[index],nullptr,&exposureTarget[index]);
        if(SUCCEEDED(result))result=device->CreateShaderResourceView(exposureTexture[index],nullptr,&exposureView[index]);
        if(SUCCEEDED(result)){const float zero[4]{};context->ClearRenderTargetView(exposureTarget[index],zero);}
    }
    if(FAILED(result))return false;
    exposureValid=false;exposureIndex=0;exposureFrame=0;exposureClock={};
    historyValid=false;
    historyIndex=0;
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
    auto prior=meshRevisions.find(source);
    if(prior!=meshRevisions.end()&&prior->second!=source->revision){
        auto vertices=meshBuffers.find(source);
        if(vertices!=meshBuffers.end()){release(vertices->second);meshBuffers.erase(vertices);}
        auto indices=meshIndexBuffers.find(source);
        if(indices!=meshIndexBuffers.end()){release(indices->second);meshIndexBuffers.erase(indices);}
    }
    meshRevisions[source]=source->revision;
    if(meshBuffers.find(source)==meshBuffers.end()){
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth=UINT(source->vertices.size()*sizeof(dx11::Vertex));
        desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=source->vertices.data();
        ID3D11Buffer* buffer=nullptr;
        if(FAILED(device->CreateBuffer(&desc,&data,&buffer)))return false;
        meshBuffers.emplace(source,buffer);
    }
    if(!source->indices.empty()&&meshIndexBuffers.find(source)==meshIndexBuffers.end()){
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth=UINT(source->indices.size()*sizeof(std::uint32_t));
        desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=source->indices.data();
        ID3D11Buffer* buffer=nullptr;
        if(FAILED(device->CreateBuffer(&desc,&data,&buffer)))return false;
        meshIndexBuffers.emplace(source,buffer);
    }
    if(source->materialRanges.empty()&&!source->textureFile.empty()&&
       modelTextures.find(source)==modelTextures.end()){
        ID3D11ShaderResourceView* texture=nullptr;
        auto key=textureKey(source->textureFile,source->alphaTest?
            dx11::texture::Kind::MaskedColor:dx11::texture::Kind::Color);
        auto cached=sharedModelTextures.find(key);
        if(cached!=sharedModelTextures.end()){
            texture=cached->second;texture->AddRef();
        }else{
            if(!loadTexture(source->textureFile,&texture,source->alphaTest?
                dx11::texture::Kind::MaskedColor:dx11::texture::Kind::Color))return false;
            texture->AddRef(); // The shared cache owns a reference independently of meshes.
            sharedModelTextures.emplace(key,texture);
        }
        modelTextures.emplace(source,texture);
    }
    for(const auto& range:source->materialRanges)
        if(!cachePbrTexture(range.baseFile,range.alphaTest?
                dx11::texture::Kind::MaskedColor:dx11::texture::Kind::Color)||
           !cachePbrTexture(range.normalFile,dx11::texture::Kind::Normal)||
           !cachePbrTexture(range.ormFile,dx11::texture::Kind::Linear)||
           !cachePbrTexture(range.occlusionFile,dx11::texture::Kind::Linear)||
           !cachePbrTexture(range.emissiveFile))return false;
    return true;
}
float shadowCascadeExtent(int cascade){
    const float scale=ui::drawDistanceScale();
    const float nearDistances[3]={2.0f,250.0f*scale,650.0f*scale};
    const float farDistances[3]={250.0f*scale,650.0f*scale,1250.0f*scale};
    const float halfDepth=(farDistances[cascade]-nearDistances[cascade])*0.5f;
    const float halfHeight=farDistances[cascade]*
        std::tan(XMConvertToRadians(camera::fieldOfView())*0.5f);
    const float halfWidth=halfHeight*float(bufferW)/float(bufferH);
    // Fit the complete camera-frustum segment in a light-space sphere so a
    // turn cannot expose unshadowed corners at a cascade boundary.
    return 2.12f*std::sqrt(halfDepth*halfDepth+
        halfHeight*halfHeight+halfWidth*halfWidth);
}
bool prepareInstances(const camera::Pose& pose,const SceneConstants& constants){
    instanceData.clear();instanceBatches.clear();
    for(auto& batches:shadowInstanceBatches)batches.clear();
    headlightShadowInstanceBatches.clear();
    streetShadowInstanceBatches.clear();
    const bool disableCulling=std::strstr(GetCommandLineA(),"--no-frustum-cull")!=nullptr;
    XMVECTOR eye=XMVectorSet(pose.eye.x,pose.eye.y,pose.eye.z,1);
    XMVECTOR forward=XMVector3Normalize(XMVectorSet(
        pose.target.x-pose.eye.x,pose.target.y-pose.eye.y,
        pose.target.z-pose.eye.z,0));
    XMVECTOR cameraUp=XMVectorSet(0,1,0,0);
    if(probeBakeActive&&probeBakeFrame%6==2)cameraUp=XMVectorSet(0,0,-1,0);
    if(probeBakeActive&&probeBakeFrame%6==3)cameraUp=XMVectorSet(0,0,1,0);
    XMVECTOR right=XMVector3Normalize(XMVector3Cross(forward,cameraUp));
    XMVECTOR up=XMVector3Cross(right,forward);
    const float tangent=probeBakeActive?1.0f:
        std::tan(XMConvertToRadians(camera::fieldOfView())*0.5f);
    const float aspect=float(bufferW)/bufferH;
    const float farPlane=1250.0f*ui::drawDistanceScale();
    XMMATRIX shadowMatrices[3]{
        XMLoadFloat4x4(&constants.shadowViewProjection[0]),
        XMLoadFloat4x4(&constants.shadowViewProjection[1]),
        XMLoadFloat4x4(&constants.shadowViewProjection[2])};
    XMMATRIX headlightMatrix=XMLoadFloat4x4(
        &constants.headlightViewProjection);
    XMMATRIX streetMatrix=XMLoadFloat4x4(
        &constants.streetViewProjection);
    auto visibleInCamera=[&](const dx11::BoundingSphere& sphere){
        XMVECTOR delta=XMVectorSubtract(XMVectorSet(sphere.x,sphere.y,sphere.z,1),eye);
        float depth=XMVectorGetX(XMVector3Dot(delta,forward));
        float radius=sphere.radius*1.5f;
        if(depth<-radius||depth>farPlane+radius)return false;
        float halfHeight=std::max(0.0f,depth)*tangent;
        return std::abs(XMVectorGetX(XMVector3Dot(delta,right)))<=
                   halfHeight*aspect+radius&&
               std::abs(XMVectorGetX(XMVector3Dot(delta,up)))<=
                   halfHeight+radius;
    };
    auto visibleInShadow=[&](const dx11::BoundingSphere& sphere,int cascade){
        XMVECTOR projected=XMVector3TransformCoord(
            XMVectorSet(sphere.x,sphere.y,sphere.z,1),shadowMatrices[cascade]);
        float extent=shadowCascadeCount>1?
            shadowCascadeExtent(cascade):1800.0f*ui::drawDistanceScale();
        float xyRadius=sphere.radius*2.0f/extent;
        float zRadius=sphere.radius/3300.0f;
        return std::abs(XMVectorGetX(projected))<=1+xyRadius&&
               std::abs(XMVectorGetY(projected))<=1+xyRadius&&
               XMVectorGetZ(projected)>=-zRadius&&
               XMVectorGetZ(projected)<=1+zRadius;
    };
    auto visibleInLocal=[&](const dx11::BoundingSphere& sphere,
                            XMMATRIX matrix,float farPlane){
        XMVECTOR clip=XMVector4Transform(
            XMVectorSet(sphere.x,sphere.y,sphere.z,1),matrix);
        float w=XMVectorGetW(clip),radius=sphere.radius*2.0f;
        return w>=-radius&&w<=farPlane+radius&&
            std::abs(XMVectorGetX(clip))<=w+radius&&
            std::abs(XMVectorGetY(clip))<=w+radius&&
            XMVectorGetZ(clip)>=-radius&&XMVectorGetZ(clip)<=w+radius;
    };
    auto append=[&](const dx11::ModelInstance& model,
                    std::vector<InstanceBatch>& batches,bool separate){
        if(batches.empty()||separate||batches.back().mesh!=model.source||
           batches.back().material!=model.material)
            batches.push_back({model.source,model.material,UINT(instanceData.size()),0});
        ++batches.back().count;
        instanceData.push_back({{model.scaleX,model.scaleY,model.scaleZ,model.cosYaw},
            {model.sinYaw,model.x,model.y,model.z},
            {model.centerX,model.minY,model.centerZ,model.sinPitch},
            {model.r,model.g,model.b,model.cosPitch},
            {model.qx,model.qy,model.qz,model.qw}});
    };
    for(const auto& model:models){
        if(!disableCulling&&!visibleInCamera(dx11::instanceBounds(model)))continue;
        if(!cacheModel(model.source))return false;
        append(model,instanceBatches,model.source->transparent);
    }
    if(constants.params.w>0){
        for(int cascade=0;cascade<shadowCascadeCount;++cascade){
            for(const auto& model:models){
                if(!model.source->castsShadow||
                   (!disableCulling&&
                    !visibleInShadow(dx11::instanceBounds(model),cascade)))continue;
                if(!cacheModel(model.source))return false;
                if(model.source->shadowProxy&&!cacheModel(model.source->shadowProxy))return false;
                append(model,shadowInstanceBatches[cascade],false);
            }
        }
    }
    if(constants.headlightShadowInfo.z>0.5f){
        for(const auto& model:models){
            if(!model.source->castsShadow||
               (!disableCulling&&
                !visibleInLocal(dx11::instanceBounds(model),
                    headlightMatrix,145.0f)))continue;
            if(!cacheModel(model.source))return false;
            if(model.source->shadowProxy&&
               !cacheModel(model.source->shadowProxy))return false;
            append(model,headlightShadowInstanceBatches,false);
        }
    }
    if(constants.streetShadowInfo.y>0.5f){
        for(const auto& model:models){
            if(!model.source->castsShadow||
               (!disableCulling&&
                !visibleInLocal(dx11::instanceBounds(model),
                    streetMatrix,130.0f)))continue;
            if(!cacheModel(model.source))return false;
            if(model.source->shadowProxy&&
               !cacheModel(model.source->shadowProxy))return false;
            append(model,streetShadowInstanceBatches,false);
        }
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
void drawSkins(bool shadow,const SceneConstants& frame){
    if(!skinVertexCount)return;
    SceneConstants constants=frame;constants.params.x=5;
    constants.temporalInfo.x=0;constants.temporalInfo.y=shadow?0.0f:1.0f;
    constants.materialPbr=pbrForGroup(5);
    constants.materialSurface={0,0.1f,0,0};
    constants.materialOptions={0,0,0,0};
    context->UpdateSubresource(sceneBuffer,0,nullptr,&constants,0,0);
    context->IASetInputLayout(skinLayout);
    UINT strides[2]={sizeof(dx11::Vertex),16},offsets[2]{};
    ID3D11Buffer* buffers[2]={skinOutput,skinPreviousOutput};
    context->IASetVertexBuffers(0,2,buffers,strides,offsets);
    context->VSSetShader(skinVS,nullptr,0);
    context->PSSetShader(shadow?nullptr:scenePS,nullptr,0);
    setTessellation(5);
    if(!shadow){
        ID3D11ShaderResourceView* resources[4]={nullptr,detailTextures[5],normalTextures[5],detailTextures[9]};
        context->PSSetShaderResources(0,4,resources);
    }
    context->Draw(UINT(skinVertexCount),0);++drawCalls;
    triangleCount+=skinVertexCount/3;
}
void drawInstances(bool shadow,SceneConstants& constants,bool transparent=false,
                   int cascade=0,int localShadow=0){
    context->IASetInputLayout(instanceLayout);
    context->VSSetShader(instanceVS,nullptr,0);
    const auto& batches=localShadow==1?headlightShadowInstanceBatches:
        localShadow==2?streetShadowInstanceBatches:
        shadow?shadowInstanceBatches[cascade]:instanceBatches;
    for(const auto& batch:batches){
        if(batch.mesh->transparent!=transparent)continue;
        if(shadow&&!batch.mesh->castsShadow)continue;
        const dx11::Mesh* drawn=shadow&&batch.mesh->shadowProxy?
            batch.mesh->shadowProxy:batch.mesh;
        ID3D11SamplerState* meshSampler=drawn->wrapTextures?sampler:modelSampler;
        context->PSSetSamplers(2,1,&meshSampler);
        // Imported foliage already has dense leaf geometry. Hull/domain
        // tessellation multiplies its cost without improving the silhouette.
        setTessellation(!drawn->allowTessellation||(drawn->textured&&batch.material==4)?0:batch.material);
        ID3D11Buffer* buffers[]={meshBuffers.at(drawn),instanceBuffer};
        UINT strides[]={sizeof(dx11::Vertex),sizeof(InstanceData)},offsets[]={0,0};
        context->IASetVertexBuffers(0,2,buffers,strides,offsets);
        ID3D11Buffer* indexBuffer=drawn->indices.empty()?nullptr:meshIndexBuffers.at(drawn);
        context->IASetIndexBuffer(indexBuffer,DXGI_FORMAT_R32_UINT,0);
        const auto& ranges=drawn->materialRanges;
        for(size_t part=0;part<std::max<size_t>(1,ranges.size());++part){
            const dx11::MaterialRange* range=ranges.empty()?nullptr:&ranges[part];
            auto texture=modelTextures.find(drawn);
            const bool masked=range?range->alphaTest:drawn->alphaTest;
            ID3D11ShaderResourceView* base=range?pbrTexture(range->baseFile,
                masked?dx11::texture::Kind::MaskedColor:dx11::texture::Kind::Color):
                texture==modelTextures.end()?nullptr:texture->second;
            bool hasModelTexture=base!=nullptr;
            constants.params.x=float(batch.material)+(hasModelTexture?0.25f:0.0f);
            constants.temporalInfo.x=batch.mesh->temporalStable?1.0f:0.0f;
            constants.temporalInfo.y=0;
            constants.materialPbr=pbrForGroup(batch.material);
            constants.materialSurface={range?range->clearcoat:0,
                range?range->clearcoatRoughness:0.1f,range?range->glassIor:0,
                drawn->grassFoliage?1.0f:0.0f};
            if(std::strstr(GetCommandLineA(),"--no-clearcoat"))constants.materialSurface.x=0;
            constants.materialOptions.x=range?range->alphaCutoff:
                batch.material==4?0.42f:0.35f;
            if(range){
                constants.materialPbr.x=range->roughness;
                constants.materialPbr.y=range->metallic;
                constants.materialPbr.z=range->emissive;
                constants.materialPbr.w=float((!range->normalFile.empty()?1:0)|
                    (!range->ormFile.empty()?2:0)|
                    (!range->occlusionFile.empty()?4:0)|
                    (!range->emissiveFile.empty()?8:0)|
                    (masked?16:0)|(bc5Normal(range->normalFile)?32:0));
            }else if(batch.material!=14&&drawn->textured){
                constants.materialPbr.x=drawn->roughness;
                constants.materialPbr.y=drawn->metallic;
            }
            if(!range&&masked)constants.materialPbr.w=16;
            if(drawn->unlit)constants.materialPbr.z=-1;
            context->UpdateSubresource(sceneBuffer,0,nullptr,&constants,0,0);
            if(shadow){
                bool alpha=masked&&hasModelTexture;
                context->PSSetShader(alpha?alphaShadowPS:nullptr,nullptr,0);
                if(alpha)context->PSSetShaderResources(0,1,&base);
            }else{
                int group=batch.material;
                ID3D11ShaderResourceView* resources[9]={base,
                    group<dx11::MATERIAL_GROUPS?detailTextures[group]:nullptr,
                    group<dx11::MATERIAL_GROUPS?normalTextures[group]:nullptr,
                    detailTextures[9],nullptr,
                    range?pbrTexture(range->normalFile,dx11::texture::Kind::Normal):nullptr,
                    range?pbrTexture(range->ormFile,dx11::texture::Kind::Linear):nullptr,
                    range?pbrTexture(range->occlusionFile,dx11::texture::Kind::Linear):nullptr,
                    range?pbrTexture(range->emissiveFile):nullptr};
                context->PSSetShaderResources(0,4,resources);
                context->PSSetShaderResources(5,4,resources+5);
            }
            UINT elementCount=range?range->count:UINT(indexBuffer?
                drawn->indices.size():drawn->vertices.size());
            if(indexBuffer)context->DrawIndexedInstanced(elementCount,batch.count,
                range?range->start:0,0,batch.start);
            else context->DrawInstanced(elementCount,batch.count,
                range?range->start:0,batch.start);
            ++drawCalls;
            triangleCount+=std::uint64_t(elementCount/3)*batch.count;
        }
    }
    context->PSSetSamplers(2,1,&modelSampler);
}
bool createStates(){
    auto noise=dx11::sky::noiseVolume();
    D3D11_TEXTURE3D_DESC volume{};
    volume.Width=volume.Height=volume.Depth=dx11::sky::noiseSize;
    volume.MipLevels=1;volume.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    volume.Usage=D3D11_USAGE_IMMUTABLE;volume.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA noiseData{noise.data(),dx11::sky::noiseSize*4,
        dx11::sky::noiseSize*dx11::sky::noiseSize*4};
    ID3D11Texture3D* noiseTexture=nullptr;
    HRESULT noiseResult=device->CreateTexture3D(&volume,&noiseData,&noiseTexture);
    if(SUCCEEDED(noiseResult))noiseResult=device->CreateShaderResourceView(noiseTexture,nullptr,&cloudNoiseView);
    release(noiseTexture);if(FAILED(noiseResult))return false;
    D3D11_SAMPLER_DESC noiseSampling{};
    noiseSampling.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    noiseSampling.AddressU=noiseSampling.AddressV=noiseSampling.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
    noiseSampling.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(device->CreateSamplerState(&noiseSampling,&cloudSampler)))return false;
    D3D11_BUFFER_DESC constant{};constant.ByteWidth=sizeof(SceneConstants);
    constant.Usage=D3D11_USAGE_DEFAULT;constant.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&constant,nullptr,&sceneBuffer)))return false;
    constant.ByteWidth=sizeof(PostConstants);
    if(FAILED(device->CreateBuffer(&constant,nullptr,&postBuffer)))return false;
    constant.ByteWidth=sizeof(dx11::tone::Constants);
    if(FAILED(device->CreateBuffer(&constant,nullptr,&toneBuffer)))return false;
    constant.ByteWidth=sizeof(XMFLOAT4);
    if(FAILED(device->CreateBuffer(&constant,nullptr,&bloomBuffer)))return false;
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
    sample.AddressU=sample.AddressV=sample.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sample.MaxLOD=D3D11_FLOAT32_MAX;
    return SUCCEEDED(device->CreateSamplerState(&sample,&clampSampler));
}
bool updateTextureSampler(){
    int requested=ui::textureQuality*3+ui::filteringQuality;
    if(sampler&&requested==activeFiltering)return true;
    D3D11_SAMPLER_DESC sample{};
    sample.Filter=D3D11_FILTER_ANISOTROPIC;
    sample.AddressU=sample.AddressV=sample.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
    sample.MaxAnisotropy=ui::filteringQuality==2?16:ui::filteringQuality==1?8:4;
    sample.MinLOD=float(2-ui::textureQuality);
    sample.MaxLOD=D3D11_FLOAT32_MAX;
    ID3D11SamplerState* replacement=nullptr;
    if(FAILED(device->CreateSamplerState(&sample,&replacement)))return false;
    sample.AddressU=sample.AddressV=sample.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    ID3D11SamplerState* modelReplacement=nullptr;
    if(FAILED(device->CreateSamplerState(&sample,&modelReplacement))){
        release(replacement);return false;
    }
    release(sampler);sampler=replacement;activeFiltering=requested;
    release(modelSampler);modelSampler=modelReplacement;
    return true;
}
bool createShadowTargets(int size,int layers){
    ID3D11ShaderResourceView* empty=nullptr;
    context->PSSetShaderResources(4,1,&empty);
    auto clear=[&](){
        release(shadowView);
        for(auto& depth:shadowDepth)release(depth);
        release(shadowTexture);
        shadowSize=shadowCascadeCount=0;
    };
    clear();
    if(size==0||layers==0)return true;
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width=texture.Height=UINT(size);texture.MipLevels=1;
    texture.ArraySize=UINT(layers);
    texture.Format=DXGI_FORMAT_R32_TYPELESS;texture.SampleDesc.Count=1;
    texture.Usage=D3D11_USAGE_DEFAULT;
    texture.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    if(FAILED(device->CreateTexture2D(&texture,nullptr,&shadowTexture)))return false;
    D3D11_DEPTH_STENCIL_VIEW_DESC depth{};
    depth.Format=DXGI_FORMAT_D32_FLOAT;
    depth.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    depth.Texture2DArray.ArraySize=1;
    for(int layer=0;layer<layers;++layer){
        depth.Texture2DArray.FirstArraySlice=UINT(layer);
        if(FAILED(device->CreateDepthStencilView(shadowTexture,&depth,&shadowDepth[layer]))){
            clear();return false;
        }
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format=DXGI_FORMAT_R32_FLOAT;
    view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    view.Texture2DArray.MipLevels=1;
    view.Texture2DArray.ArraySize=UINT(layers);
    if(FAILED(device->CreateShaderResourceView(shadowTexture,&view,&shadowView))){
        clear();return false;
    }
    shadowSize=size;shadowCascadeCount=layers;return true;
}
bool createHeadlightShadowTarget(){
    constexpr UINT size=1024;
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width=texture.Height=size;texture.MipLevels=texture.ArraySize=1;
    texture.Format=DXGI_FORMAT_R32_TYPELESS;texture.SampleDesc.Count=1;
    texture.Usage=D3D11_USAGE_DEFAULT;
    texture.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    HRESULT result=device->CreateTexture2D(&texture,nullptr,&headlightShadowTexture);
    if(SUCCEEDED(result)){
        D3D11_DEPTH_STENCIL_VIEW_DESC depth{};
        depth.Format=DXGI_FORMAT_D32_FLOAT;
        depth.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
        result=device->CreateDepthStencilView(headlightShadowTexture,&depth,
            &headlightShadowDepth);
    }
    if(SUCCEEDED(result)){
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format=DXGI_FORMAT_R32_FLOAT;
        view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels=1;
        result=device->CreateShaderResourceView(headlightShadowTexture,&view,
            &headlightShadowView);
    }
    if(FAILED(result)){
        release(headlightShadowView);release(headlightShadowDepth);
        release(headlightShadowTexture);
    }
    return SUCCEEDED(result);
}
bool createStreetShadowTarget(){
    constexpr UINT size=512;
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width=texture.Height=size;texture.MipLevels=texture.ArraySize=1;
    texture.Format=DXGI_FORMAT_R32_TYPELESS;texture.SampleDesc.Count=1;
    texture.Usage=D3D11_USAGE_DEFAULT;
    texture.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    HRESULT result=device->CreateTexture2D(&texture,nullptr,&streetShadowTexture);
    if(SUCCEEDED(result)){
        D3D11_DEPTH_STENCIL_VIEW_DESC depth{};
        depth.Format=DXGI_FORMAT_D32_FLOAT;
        depth.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
        result=device->CreateDepthStencilView(streetShadowTexture,&depth,
            &streetShadowDepth);
    }
    if(SUCCEEDED(result)){
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format=DXGI_FORMAT_R32_FLOAT;
        view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels=1;
        result=device->CreateShaderResourceView(streetShadowTexture,&view,
            &streetShadowView);
    }
    if(FAILED(result)){
        release(streetShadowView);release(streetShadowDepth);
        release(streetShadowTexture);
    }
    return SUCCEEDED(result);
}
void releaseTargets(){
    context->OMSetRenderTargets(0,nullptr,nullptr);
    ID3D11ShaderResourceView* nullViews[13]{};context->PSSetShaderResources(0,13,nullViews);
    for(int index=0;index<2;++index){
        release(historyView[index]);release(historyTarget[index]);release(historyTexture[index]);
    }
    release(motionView);release(motionTarget);release(motionTexture);
    release(objectMotionView);release(objectMotionTarget);release(objectMotionTexture);
    release(postView);release(postTarget);release(postTexture);
    release(composedView);release(composedTarget);release(composedTexture);
    for(unsigned level=0;level<dx11::tone::meterLevels;++level){
        release(meterView[level]);release(meterTarget[level]);release(meterTexture[level]);
    }
    for(int index=0;index<2;++index){
        release(exposureView[index]);release(exposureTarget[index]);release(exposureTexture[index]);
    }
    exposureValid=false;
    release(reflectionView);release(reflectionTarget);release(reflectionTexture);
    for(int level=0;level<3;++level){
        release(bloomView[level]);release(bloomTarget[level]);release(bloomTexture[level]);
    }
    release(hudView);release(hudTexture);release(depthViewSRV);release(depthView);
    release(depthTexture);release(indirectView);release(indirectTarget);release(indirectTexture);
    release(reflectionResponseView);release(reflectionResponseTarget);release(reflectionResponseTexture);
    release(surfaceView);release(surfaceTarget);release(surfaceTexture);
    release(sceneView);release(sceneTarget);release(sceneTexture);release(target);
    historyValid=false;
}
SceneConstants constantsForFrame(const camera::Pose& pose,float solar,float daylight,
                                 bool temporalAA){
    SceneConstants constants{};
    const auto& conditions=weather::current();
    float light=daylight*(1.0f-conditions.clouds*0.42f);
    XMVECTOR eye=XMVectorSet(pose.eye.x,pose.eye.y,pose.eye.z,1);
    XMVECTOR targetPoint=XMVectorSet(pose.target.x,pose.target.y,pose.target.z,1);
    // Movement, steering, and mouse yaw use the same right-handed view as the OpenGL build.
    XMVECTOR up=XMVectorSet(0,1,0,0);
    if(probeBakeActive&&probeBakeFrame%6==2)up=XMVectorSet(0,0,-1,0);
    if(probeBakeActive&&probeBakeFrame%6==3)up=XMVectorSet(0,0,1,0);
    XMMATRIX view=probeBakeActive?XMMatrixLookAtLH(eye,targetPoint,up):
        XMMatrixLookAtRH(eye,targetPoint,up);
    float drawScale=ui::drawDistanceScale();
    XMMATRIX projection=probeBakeActive?XMMatrixPerspectiveFovLH(XM_PIDIV2,1,2,1250*drawScale):
        XMMatrixPerspectiveFovRH(XMConvertToRadians(camera::fieldOfView()),
        float(bufferW)/bufferH,2.0f,1250.0f*drawScale);
    if(temporalAA){
        // Eight subpixel sample positions. Projection jitter is included in
        // both the current and previous matrices used for reprojection.
        constexpr float jitter[8][2]={{0.0f,-0.166667f},{-0.25f,0.166667f},
            {0.25f,-0.388889f},{-0.375f,-0.055556f},
            {0.125f,0.277778f},{-0.125f,-0.277778f},
            {0.375f,0.055556f},{-0.4375f,0.388889f}};
        const auto& sample=jitter[temporalFrame%8];
        projection=XMMatrixMultiply(projection,XMMatrixTranslation(
            2.0f*sample[0]/bufferW,-2.0f*sample[1]/bufferH,0));
    }
    XMStoreFloat4x4(&constants.viewProjection,XMMatrixMultiply(view,projection));
    constants.sun={std::cos((gameHour-6)*PI/12),solar,0.3f,
        std::clamp(solar*6.0f,0.0f,1.0f)*light};
    if(std::strstr(GetCommandLineA(),"--no-direct-sun"))constants.sun.w=0;
    XMVECTOR focus=XMVectorSet(player.x,0,player.z,1);
    XMVECTOR sunDirection=XMVector3Normalize(XMVectorSet(constants.sun.x,constants.sun.y,constants.sun.z,0));
    XMVECTOR cameraForward=XMVector3Normalize(XMVectorSubtract(targetPoint,eye));
    const float splitNear[3]={2.0f,250.0f*drawScale,650.0f*drawScale};
    const float splitFar[3]={250.0f*drawScale,650.0f*drawScale,
                             1250.0f*drawScale};
    for(int cascade=0;cascade<3;++cascade){
        const bool high=shadowCascadeCount>1;
        float extent=high?shadowCascadeExtent(cascade):1800.0f*drawScale;
        XMVECTOR center=high?XMVectorAdd(eye,XMVectorScale(cameraForward,
            (splitNear[cascade]+splitFar[cascade])*0.5f)):
            focus;
        XMVECTOR lightEye=XMVectorAdd(center,XMVectorScale(sunDirection,1400));
        XMMATRIX lightView=XMMatrixLookAtRH(lightEye,center,XMVectorSet(0,1,0,0));
        XMVECTOR origin=XMVector3TransformCoord(XMVectorZero(),lightView);
        float texel=extent/float(std::max(1,shadowSize));
        float snapX=std::round(XMVectorGetX(origin)/texel)*texel-XMVectorGetX(origin);
        float snapY=std::round(XMVectorGetY(origin)/texel)*texel-XMVectorGetY(origin);
        lightView=XMMatrixMultiply(lightView,XMMatrixTranslation(snapX,snapY,0));
        XMMATRIX lightProjection=XMMatrixOrthographicRH(extent,extent,1,3300);
        XMStoreFloat4x4(&constants.shadowViewProjection[cascade],
            XMMatrixMultiply(lightView,lightProjection));
    }
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
    struct LocalLight {XMFLOAT4 position,color;float distance;
        bool playerHeadlight,streetLight;};
    std::vector<LocalLight> lights;
    auto addLight=[&](float x,float y,float z,float radius,
                      float r,float g,float b,float brightness,
                      bool playerHeadlight=false,bool streetLight=false){
        float distance=std::hypot(x-pose.eye.x,z-pose.eye.z);
        if(distance>radius+330)return;
        lights.push_back({{x,y,z,radius},{r,g,b,brightness},distance,
            playerHeadlight,streetLight});
    };
    if(night>0.1f){
        const bool modernLamps=dx11::mesh("modern/street-lamp")!=nullptr;
        for(int column=0;column<5;++column)for(int row=0;row<7;++row)
            addLight(368.0f+column*450-(modernLamps?13.1f:0),
                modernLamps?44.65f:43,115.0f+row*215,115,
                1.0f,0.74f,0.41f,1.7f*night,false,true);
        for(int z=8860;z<10010;z+=116)
            addLight(8066.0f,36,float(z),95,1.0f,0.78f,0.50f,
                1.4f*night,false,true);
        for(const auto& shop:commerce::shops)
            addLight(shop.p.x,20,shop.p.z,65,0.35f,0.90f,1.0f,0.95f*night);
    }
    if(!probeBakeActive){
    for(const auto& vehicle:vehicles){
        if(!vehicleLightsOn(vehicle)||
           std::hypot(vehicle.p.x-player.x,vehicle.p.z-player.z)>390)continue;
        Vec2 facing=forward(vehicle.angle);
        Vec2 side{-facing.z,facing.x};
        float scale=physics::vehicleScale(vehicle.kind);
        float front=(vehicle.kind==Kind::Bike?13.0f:24.05f)*scale;
        float width=(vehicle.kind==Kind::Bike?0.0f:8.0f)*scale;
        for(float sign:{-1.0f,1.0f}){
            if(width==0&&sign>0)continue;
            addLight(vehicle.p.x+facing.x*front+side.x*width*sign,
                vehicle.rideHeight+(width==0?17.0f:9.5f)*scale,
                vehicle.p.z+facing.z*front+side.z*width*sign,
                125,1.0f,0.94f,0.72f,1.7f,
                occupied>=0&&occupied<int(vehicles.size())&&
                    &vehicle==&vehicles[occupied]);
            if(width>0)
                addLight(vehicle.p.x-facing.x*front+side.x*width*sign,
                    vehicle.rideHeight+9.7f*scale,
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
    }
    std::sort(lights.begin(),lights.end(),[](const LocalLight& a,const LocalLight& b){
        return a.distance<b.distance;});
    constants.headlightShadowInfo={-1,-1,0,1.0f/1024.0f};
    XMStoreFloat4x4(&constants.headlightViewProjection,XMMatrixIdentity());
    constants.streetShadowInfo={-1,0,1.0f/512.0f,0};
    XMStoreFloat4x4(&constants.streetViewProjection,XMMatrixIdentity());
    for(size_t i=0;i<std::min<size_t>(lights.size(),12);++i){
        constants.localLightPosition[i]=lights[i].position;
        constants.localLightColor[i]=lights[i].color;
        if(lights[i].playerHeadlight){
            if(constants.headlightShadowInfo.x<0)
                constants.headlightShadowInfo.x=float(i);
            else constants.headlightShadowInfo.y=float(i);
        }
        if(lights[i].streetLight&&constants.streetShadowInfo.x<0)
            constants.streetShadowInfo.x=float(i);
    }
    if(headlightShadowDepth&&ui::shadowQuality>1&&
       constants.headlightShadowInfo.x>=0&&occupied>=0&&
       occupied<int(vehicles.size())){
        const Vehicle& vehicle=vehicles[occupied];
        Vec2 facing=forward(vehicle.angle);
        float scale=physics::vehicleScale(vehicle.kind);
        float front=(vehicle.kind==Kind::Bike?17.0f:28.0f)*scale;
        XMVECTOR lamp=XMVectorSet(vehicle.p.x+facing.x*front,
            vehicle.rideHeight+(vehicle.kind==Kind::Bike?17.0f:10.0f)*scale,
            vehicle.p.z+facing.z*front,1);
        XMVECTOR target=XMVectorAdd(lamp,XMVectorSet(
            facing.x*100,-14,facing.z*100,0));
        XMMATRIX view=XMMatrixLookAtRH(lamp,target,XMVectorSet(0,1,0,0));
        XMMATRIX projection=XMMatrixPerspectiveFovRH(
            XMConvertToRadians(105.0f),1.0f,2.0f,145.0f);
        XMStoreFloat4x4(&constants.headlightViewProjection,
            XMMatrixMultiply(view,projection));
        constants.headlightShadowInfo.z=1;
    }
    if(streetShadowDepth&&ui::shadowQuality>1&&
       constants.streetShadowInfo.x>=0){
        const auto& lamp=lights[size_t(constants.streetShadowInfo.x)].position;
        XMVECTOR position=XMVectorSet(lamp.x,lamp.y,lamp.z,1);
        XMVECTOR target=XMVectorSet(lamp.x,0,lamp.z,1);
        XMMATRIX view=XMMatrixLookAtRH(position,target,XMVectorSet(0,0,1,0));
        XMMATRIX projection=XMMatrixPerspectiveFovRH(
            XMConvertToRadians(135.0f),1.0f,2.0f,130.0f);
        XMStoreFloat4x4(&constants.streetViewProjection,
            XMMatrixMultiply(view,projection));
        constants.streetShadowInfo.y=1;
    }
    constants.eye={pose.eye.x,pose.eye.y,pose.eye.z,float(ui::graphicsQuality)};
    float fogEnd=1250.0f*drawScale*0.94f*conditions.visibility;
    constants.params={0,fogEnd*0.51f,fogEnd,
        shadowDepth[0]&&ui::shadowQuality>0&&daylight>0.05f?1.0f:0.0f};
    constants.shadowInfo={1.0f/float(std::max(1,shadowSize)),
        250.0f*drawScale,650.0f*drawScale,float(shadowCascadeCount)};
    constants.materialOptions.y=std::strstr(GetCommandLineA(),
        "--shadow-cascade-view")?1.0f:0.0f;
    constants.materialOptions.z=std::strstr(GetCommandLineA(),
        "--material-view")?1.0f:0.0f;
    constants.materialOptions.w=std::strstr(GetCommandLineA(),
        "--mip-view")?1.0f:0.0f;
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
bool exclusiveFullscreenEnabled(){
    BOOL enabled=FALSE;return swapChain&&SUCCEEDED(swapChain->GetFullscreenState(&enabled,nullptr))&&enabled;
}
bool setExclusiveFullscreen(bool enabled,int width,int height){
    if(!swapChain)return !enabled;
    BOOL current=FALSE;if(FAILED(swapChain->GetFullscreenState(&current,nullptr)))return false;
    if(bool(current)==enabled)return true;
    HRESULT result=swapChain->SetFullscreenState(enabled,nullptr);
    bool ok=SUCCEEDED(result);
    if(ok&&enabled){DXGI_MODE_DESC mode{};mode.Width=width;mode.Height=height;mode.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        result=swapChain->ResizeTarget(&mode);ok=SUCCEEDED(result);
        if(!ok)swapChain->SetFullscreenState(FALSE,nullptr);}
    if(!ok){char message[128];std::snprintf(message,sizeof(message),"DXGI fullscreen transition failed: 0x%08lx",static_cast<unsigned long>(result));logging::write(message);}
    if(ok){RECT client{};GetClientRect(win,&client);screenW=std::max(1,int(client.right));screenH=std::max(1,int(client.bottom));}
    return ok;
}
bool initRenderer(){
    if(!startup::report(2,"Starting Direct3D 11"))return false;
    DXGI_SWAP_CHAIN_DESC description{};description.BufferCount=2;
    description.BufferDesc.Width=UINT(std::max(1,screenW));
    description.BufferDesc.Height=UINT(std::max(1,screenH));
    description.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    description.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow=win;description.SampleDesc.Count=1;
    description.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    description.Windowed=TRUE;description.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL requested=D3D_FEATURE_LEVEL_11_0,created{};
    HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,&requested,1,D3D11_SDK_VERSION,&description,
        &swapChain,&device,&created,&context);
    if(FAILED(result))result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,&requested,1,D3D11_SDK_VERSION,&description,
        &swapChain,&device,&created,&context);
    if(FAILED(result))return false;
    IDXGIFactory* displayFactory=nullptr;
    if(SUCCEEDED(swapChain->GetParent(__uuidof(IDXGIFactory),reinterpret_cast<void**>(&displayFactory)))){
        displayFactory->MakeWindowAssociation(win,DXGI_MWA_NO_ALT_ENTER);displayFactory->Release();}
    logAdapter();
    Gdiplus::GdiplusStartupInput startup;
    if(Gdiplus::GdiplusStartup(&gdiplusToken,&startup,nullptr)!=Gdiplus::Ok)return false;
    if(!startup::report(5,"Compiling graphics shaders"))return false;
    if(!createShaders())return false;
    if(!startup::report(30,"Creating graphics buffers and shadows"))return false;
    if(!createStates()||!createTargets(std::max(1,screenW),std::max(1,screenH)))return false;
    if(!std::strstr(GetCommandLineA(),"--cpu-skinning")&&!createSkinResources())
        logging::write("GPU skin resources unavailable; using CPU deformation");
    if(!createHeadlightShadowTarget())
        logging::write("Headlight shadow target unavailable; continuing without local shadows");
    if(!createStreetShadowTarget())
        logging::write("Streetlight shadow target unavailable; continuing without its shadow");
    if(!createGpuQueries())logging::write("GPU timestamp queries unavailable");
    std::wstring base=executableFolder();
    if(!startup::report(35,"Loading surface textures"))return false;
    std::vector<dx11::texture::Request> surfaceRequests;
    std::vector<ID3D11ShaderResourceView**> surfaceViews;
    auto surface=[&](const std::wstring& file,ID3D11ShaderResourceView** view,
                     dx11::texture::Kind kind=dx11::texture::Kind::Color){
        surfaceRequests.push_back({file,kind});surfaceViews.push_back(view);
    };
    surface(base+L"\\assets\\texture_atlas.png",&textures[1]);
    const wchar_t* materials[]={nullptr,nullptr,L"Bricks001",L"Concrete001",
        L"Bark001",L"Fabric001",L"Metal001",L"Asphalt001",L"Ground054",L"Grass001",nullptr};
    for(int i=2;i<10;++i){
        std::wstring path=base+L"\\assets\\materials\\"+materials[i];
        surface(path+L"_Color.jpg",&detailTextures[i]);
        surface(path+L"_NormalDX.jpg",&normalTextures[i],dx11::texture::Kind::Normal);
    }
    surface(base+L"\\assets\\models\\baked\\marina\\MarinaFacade_NormalDX.png",
        &normalTextures[10],dx11::texture::Kind::Normal);
    if(!preloadTextures(surfaceRequests,[&](size_t index,const dx11::texture::Prepared& prepared){
        return uploadTexture(prepared,surfaceRequests[index].kind,surfaceViews[index]);
    },35,9,"Loading surface textures"))return false;
    detailTextures[11]=detailTextures[3];detailTextures[11]->AddRef();
    normalTextures[11]=normalTextures[3];normalTextures[11]->AddRef();
    detailTextures[12]=detailTextures[3];detailTextures[12]->AddRef();
    normalTextures[12]=normalTextures[3];normalTextures[12]->AddRef();
    if(!startup::report(45,"Loading city models and animations"))return false;
    dx11::loadMeshes(base+L"\\assets\\models\\baked");
    for(const auto& issue:dx11::assetIssues())logging::write(issue.c_str());
    if(!startup::report(55,"Preparing city geometry"))return false;
    if(!createStaticGeometry())return false;
    // A region jump must not synchronously upload dozens of nature and city
    // meshes on its first visible frame. Upload them behind the loading window;
    // the immutable buffers are shared by later instances.
    auto regionalMeshes=dx11::regionalMeshes();
    // Equipping a pickup must not decode its held-weapon texture or create its
    // static buffers on the first gameplay frame that uses that weapon.
    for(const char* name:{"weapons/pistol","weapons/ak","weapons/lightning"})
        if(const auto* weaponMesh=dx11::mesh(name))regionalMeshes.push_back(weaponMesh);
    // Gather and deduplicate before workers start. Only the main thread touches
    // renderer caches and D3D; the CPU pipeline holds at most N prepared images.
    std::map<std::wstring,dx11::texture::Request> uniqueTextures;
    auto request=[&](const std::wstring& file,dx11::texture::Kind kind){
        if(!file.empty()&&!sharedModelTextures.count(textureKey(file,kind)))
            uniqueTextures.emplace(textureKey(file,kind),dx11::texture::Request{file,kind});
    };
    for(const auto* mesh:regionalMeshes){
        if(mesh->materialRanges.empty())request(mesh->textureFile,mesh->alphaTest?
            dx11::texture::Kind::MaskedColor:dx11::texture::Kind::Color);
        for(const auto& range:mesh->materialRanges){
            request(range.baseFile,range.alphaTest?dx11::texture::Kind::MaskedColor:dx11::texture::Kind::Color);
            request(range.normalFile,dx11::texture::Kind::Normal);
            request(range.ormFile,dx11::texture::Kind::Linear);
            request(range.occlusionFile,dx11::texture::Kind::Linear);
            request(range.emissiveFile,dx11::texture::Kind::Color);
        }
    }
    std::vector<dx11::texture::Request> regionalRequests;
    for(const auto& item:uniqueTextures)regionalRequests.push_back(item.second);
    if(!preloadTextures(regionalRequests,[&](size_t index,const dx11::texture::Prepared& prepared){
        ID3D11ShaderResourceView* view=nullptr;const auto& request=regionalRequests[index];
        if(!uploadTexture(prepared,request.kind,&view))return false;
        sharedModelTextures.emplace(textureKey(request.file,request.kind),view);return true;
    },58,18,"Preparing regional textures"))return false;
    size_t uploaded=0;
    for(const dx11::Mesh* mesh:regionalMeshes){
        if(!startup::report(76+int(3*uploaded/std::max(size_t(1),regionalMeshes.size())),
            "Uploading regional graphics"))return false;
        if(!cacheModel(mesh)){
            logging::write("Regional resource prewarm incomplete; using on-demand loading");
            break;
        }
        ++uploaded;
    }
    if(!startup::report(80,"Loading HDR lighting"))return false;
    char textureSummary[240]{};std::snprintf(textureSummary,sizeof(textureSummary),
        "Loaded image textures: %u, %.2f MiB payload; block-compressed DDS: %u, %.2f MiB (excludes HDR probes and render targets)",
        loadedTextureCount,double(loadedTextureBytes)/(1024*1024),loadedDdsCount,double(loadedDdsBytes)/(1024*1024));
    logging::write(textureSummary);
    scenePool=std::make_unique<cpu::Pool>(sceneWorkers());
    logging::write(("CPU scene workers: "+std::to_string(scenePool->concurrency())).c_str());
    return createProbes();
}
void render(){
    if(deviceLost||!device||!context)return;
    if(std::strstr(GetCommandLineA(),"--smoke")&&
       std::strstr(GetCommandLineA(),"--high-shadows"))
        ui::shadowQuality=2;
    if(std::strstr(GetCommandLineA(),"--smoke")&&
       std::strstr(GetCommandLineA(),"--high-taa"))
        ui::antiAliasingQuality=2;
    if(std::strstr(GetCommandLineA(),"--smoke")&&
       std::strstr(GetCommandLineA(),"--low-aa"))
        ui::antiAliasingQuality=0;
    auto renderBegin=std::chrono::steady_clock::now();
    drawCalls=0;triangleCount=0;
    pollGpuQueries();
    if(!updateTextureSampler())return;
    int width=std::max(1,screenW),height=std::max(1,screenH);
    if(width!=bufferW||height!=bufferH){releaseTargets();
        if(!createTargets(width,height))return;}
    // Scene LOD and draw culling share the same interpolated camera pose.
    Vec2 focus=(occupied>=0||rightMouse)?player:
        previousPlayer*(1.0f-renderAlpha)+player*renderAlpha;
    camera::Pose pose=camera::compute(focus,playerY,
        rightMouse&&occupied<0&&!ui::paused(),occupied);
    if(probeBakeActive){
        unsigned probe=probeBakeFrame/18,state=(probeBakeFrame/6)%3,face=probeBakeFrame%6;
        constexpr Vec3 directions[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        const auto& origin=probePositions[probe];
        player=previousPlayer={origin.x,origin.z};gameHour=probeHours[state];
        pose.eye={origin.x,origin.y,origin.z};
        pose.target={origin.x+directions[face].x,origin.y+directions[face].y,
            origin.z+directions[face].z};
    }
    dx11::buildScene(groups,models,pose.eye.x,pose.eye.y,pose.eye.z,
        skinCS?&gpuSkins:nullptr,probeBakeActive,scenePool.get());
    if(skinCS)prepareSkins();
    else {gpuSkins.clear();skinVertexCount=0;}
    auto sceneBuilt=std::chrono::steady_clock::now();
    vertices.clear();size_t starts[dx11::MATERIAL_GROUPS]{},counts[dx11::MATERIAL_GROUPS]{};
    for(int group=0;group<dx11::MATERIAL_GROUPS;++group){starts[group]=vertices.size();counts[group]=groups[group].size();
        vertices.insert(vertices.end(),groups[group].begin(),groups[group].end());}
    if(!growVertexBuffer(vertices.size()))return;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(vertexBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(dx11::Vertex));
    context->Unmap(vertexBuffer,0);
    float solar=std::sin((gameHour-6)*PI/12.0f);
    float daylight=std::clamp(solar*2.3f+0.42f,0.0f,1.0f);
    int requestedShadowSize=ui::shadowQuality==0?0:ui::shadowQuality==1?1024:2048;
    int requestedCascades=ui::shadowQuality==0?0:ui::shadowQuality==1?1:3;
    if((requestedShadowSize!=shadowSize||requestedCascades!=shadowCascadeCount)&&
       !createShadowTargets(requestedShadowSize,requestedCascades))
        createShadowTargets(0,0);
    dx11::sortInstancesForRendering(models,pose.eye.x,pose.eye.y,pose.eye.z);
    const char* commandLine=GetCommandLineA();
    const bool temporalAA=ui::graphicsQuality>0&&ui::antiAliasingQuality==2&&
        !std::strstr(commandLine,"--no-taa")&&!probeBakeActive;
    const bool motionDebug=std::strstr(commandLine,"--motion-view")!=nullptr;
    SceneConstants constants=constantsForFrame(pose,solar,daylight,temporalAA);
    updateProbes();
    constants.previousViewProjection=previousViewProjection;
    constants.temporalInfo={0,0,1.0f/width,1.0f/height};
    XMFLOAT3 forward{pose.target.x-pose.eye.x,pose.target.y-pose.eye.y,
        pose.target.z-pose.eye.z};
    float forwardLength=std::sqrt(forward.x*forward.x+forward.y*forward.y+
        forward.z*forward.z);
    if(forwardLength>0.0001f){forward.x/=forwardLength;
        forward.y/=forwardLength;forward.z/=forwardLength;}
    const float eyeDistance=std::sqrt(
        (pose.eye.x-previousCameraEye.x)*(pose.eye.x-previousCameraEye.x)+
        (pose.eye.y-previousCameraEye.y)*(pose.eye.y-previousCameraEye.y)+
        (pose.eye.z-previousCameraEye.z)*(pose.eye.z-previousCameraEye.z));
    const float forwardDot=forward.x*previousCameraForward.x+
        forward.y*previousCameraForward.y+forward.z*previousCameraForward.z;
    if((!temporalAA&&!motionDebug)||eyeDistance>65.0f||forwardDot<0.93f)
        historyValid=false;
    if(!prepareInstances(pose,constants))return;
    auto instancesReady=std::chrono::steady_clock::now();
    UINT stride=sizeof(dx11::Vertex),offset=0;
    context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
    context->IASetInputLayout(inputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(sceneVS,nullptr,0);
    context->VSSetConstantBuffers(0,1,&sceneBuffer);
    context->HSSetConstantBuffers(0,1,&sceneBuffer);
    context->DSSetConstantBuffers(0,1,&sceneBuffer);
    beginGpuQueries();
    if(constants.params.w>0){
        ID3D11ShaderResourceView* empty=nullptr;
        context->PSSetShaderResources(4,1,&empty);
        D3D11_VIEWPORT shadowViewport{};
        shadowViewport.Width=shadowViewport.Height=float(shadowSize);
        shadowViewport.MaxDepth=1;
        context->RSSetViewports(1,&shadowViewport);
        context->RSSetState(shadowRaster);
        context->PSSetShader(nullptr,nullptr,0);
        context->PSSetSamplers(0,1,&sampler);
        context->PSSetSamplers(2,1,&modelSampler);
        SceneConstants shadowConstants=constants;
        for(int cascade=0;cascade<shadowCascadeCount;++cascade){
            context->OMSetRenderTargets(0,nullptr,shadowDepth[cascade]);
            context->ClearDepthStencilView(shadowDepth[cascade],D3D11_CLEAR_DEPTH,1,0);
            shadowConstants.viewProjection=constants.shadowViewProjection[cascade];
            context->IASetInputLayout(inputLayout);
            context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
            context->VSSetShader(sceneVS,nullptr,0);
            context->PSSetShader(nullptr,nullptr,0);
            for(int group=0;group<dx11::MATERIAL_GROUPS;++group)if(counts[group]){
                setTessellation(group);
                shadowConstants.params.x=float(group);
                context->UpdateSubresource(sceneBuffer,0,nullptr,&shadowConstants,0,0);
                context->Draw(UINT(counts[group]),UINT(starts[group]));++drawCalls;
                triangleCount+=counts[group]/3;
            }
            drawSkins(true,shadowConstants);
            drawInstances(true,shadowConstants,false,cascade);
        }
    }
    if(constants.headlightShadowInfo.z>0.5f){
        ID3D11ShaderResourceView* empty=nullptr;
        context->PSSetShaderResources(9,1,&empty);
        D3D11_VIEWPORT viewport{};
        viewport.Width=viewport.Height=1024.0f;viewport.MaxDepth=1;
        context->RSSetViewports(1,&viewport);
        context->RSSetState(shadowRaster);
        context->OMSetRenderTargets(0,nullptr,headlightShadowDepth);
        context->ClearDepthStencilView(headlightShadowDepth,D3D11_CLEAR_DEPTH,1,0);
        context->PSSetShader(nullptr,nullptr,0);
        context->PSSetSamplers(0,1,&sampler);
        context->PSSetSamplers(2,1,&modelSampler);
        context->IASetInputLayout(inputLayout);
        context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
        context->VSSetShader(sceneVS,nullptr,0);
        SceneConstants shadowConstants=constants;
        shadowConstants.viewProjection=constants.headlightViewProjection;
        for(int group=0;group<dx11::MATERIAL_GROUPS;++group)if(counts[group]){
            setTessellation(group);
            shadowConstants.params.x=float(group);
            context->UpdateSubresource(sceneBuffer,0,nullptr,&shadowConstants,0,0);
            context->Draw(UINT(counts[group]),UINT(starts[group]));++drawCalls;
            triangleCount+=counts[group]/3;
        }
        drawSkins(true,shadowConstants);
        drawInstances(true,shadowConstants,false,0,1);
    }
    if(constants.streetShadowInfo.y>0.5f){
        ID3D11ShaderResourceView* empty=nullptr;
        context->PSSetShaderResources(10,1,&empty);
        D3D11_VIEWPORT viewport{};
        viewport.Width=viewport.Height=512.0f;viewport.MaxDepth=1;
        context->RSSetViewports(1,&viewport);
        context->RSSetState(shadowRaster);
        context->OMSetRenderTargets(0,nullptr,streetShadowDepth);
        context->ClearDepthStencilView(streetShadowDepth,D3D11_CLEAR_DEPTH,1,0);
        context->PSSetShader(nullptr,nullptr,0);
        context->PSSetSamplers(0,1,&sampler);
        context->PSSetSamplers(2,1,&modelSampler);
        context->IASetInputLayout(inputLayout);
        context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
        context->VSSetShader(sceneVS,nullptr,0);
        SceneConstants shadowConstants=constants;
        shadowConstants.viewProjection=constants.streetViewProjection;
        for(int group=0;group<dx11::MATERIAL_GROUPS;++group)if(counts[group]){
            setTessellation(group);
            shadowConstants.params.x=float(group);
            context->UpdateSubresource(sceneBuffer,0,nullptr,&shadowConstants,0,0);
            context->Draw(UINT(counts[group]),UINT(starts[group]));++drawCalls;
            triangleCount+=counts[group]/3;
        }
        drawSkins(true,shadowConstants);
        drawInstances(true,shadowConstants,false,0,2);
    }
    if(currentGpu)context->End(currentGpu->shadowEnd);
    float clearColor[]={constants.fogColor.x,constants.fogColor.y,constants.fogColor.z,1};
    ID3D11RenderTargetView* opaqueTargets[5]={sceneTarget,surfaceTarget,indirectTarget,objectMotionTarget,reflectionResponseTarget};
    context->OMSetRenderTargets(5,opaqueTargets,depthView);
    context->ClearRenderTargetView(sceneTarget,clearColor);
    float clearSurface[]={0.5f,1.0f,0.5f,1.0f};
    context->ClearRenderTargetView(surfaceTarget,clearSurface);
    float clearIndirect[]={0,0,0,0};
    context->ClearRenderTargetView(indirectTarget,clearIndirect);
    context->ClearRenderTargetView(objectMotionTarget,clearIndirect);
    context->ClearRenderTargetView(reflectionResponseTarget,clearIndirect);
    context->ClearDepthStencilView(depthView,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
    D3D11_VIEWPORT mainViewport{};mainViewport.Width=float(bufferW);mainViewport.Height=float(bufferH);
    mainViewport.MaxDepth=1;context->RSSetViewports(1,&mainViewport);
    context->RSSetState(rasterState);
    context->VSSetShader(sceneVS,nullptr,0);context->PSSetShader(scenePS,nullptr,0);
    context->VSSetConstantBuffers(0,1,&sceneBuffer);
    context->PSSetConstantBuffers(0,1,&sceneBuffer);
    context->PSSetSamplers(0,1,&sampler);
    context->PSSetSamplers(1,1,&shadowSampler);
    context->PSSetSamplers(2,1,&modelSampler);
    context->PSSetShaderResources(4,1,&shadowView);
    context->PSSetShaderResources(9,1,&headlightShadowView);
    context->PSSetShaderResources(10,1,&streetShadowView);
    context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
    context->IASetInputLayout(inputLayout);
    context->VSSetShader(sceneVS,nullptr,0);
    for(int group=0;group<dx11::MATERIAL_GROUPS;++group){
        if(!counts[group]&&!staticCounts[group])continue;
        constants.params.x=float(group);
        constants.temporalInfo.x=group==3||group==5||group==14?0.0f:1.0f;
        constants.materialPbr=pbrForGroup(group);
        constants.materialSurface={0,0.1f,0,0};
        context->UpdateSubresource(sceneBuffer,0,nullptr,&constants,0,0);
        setTessellation(group);
        ID3D11ShaderResourceView* baseTexture=group==1?textures[1]:nullptr;
        ID3D11ShaderResourceView* resources[4]={baseTexture,detailTextures[group],normalTextures[group],detailTextures[9]};
        context->PSSetShaderResources(0,4,resources);
        if(staticCounts[group]){
            context->IASetVertexBuffers(0,1,&staticBuffer,&stride,&offset);
            context->Draw(UINT(staticCounts[group]),UINT(staticStarts[group]));++drawCalls;
            triangleCount+=staticCounts[group]/3;
        }
        if(counts[group]){
            context->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
            context->Draw(UINT(counts[group]),UINT(starts[group]));++drawCalls;
            triangleCount+=counts[group]/3;
        }
    }
    drawSkins(false,constants);
    drawInstances(false,constants);
    float effectBlend[4]{0,0,0,0};
    context->OMSetRenderTargets(1,&sceneTarget,depthView);
    context->OMSetDepthStencilState(readDepth,0);
    context->OMSetBlendState(alphaBlend,effectBlend,0xffffffffu);
    drawInstances(false,constants,true);
    context->OMSetBlendState(nullptr,effectBlend,0xffffffffu);
    context->OMSetDepthStencilState(nullptr,0);
    setTessellation(-1);
    if(currentGpu)context->End(currentGpu->sceneEnd);
    if(probeBakeActive){
        probeBakeFailed=!captureProbe(constants);
        if(probeBakeFailed)logging::write("HDR probe capture failed");
        endGpuQueries();return;
    }
    if(ui::graphicsQuality>0){
        context->OMSetDepthStencilState(noDepth,0);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->VSSetShader(hudVS,nullptr,0);
        context->PSSetShader(bloomPS,nullptr,0);
        context->PSSetSamplers(0,1,&clampSampler);
        context->PSSetConstantBuffers(0,1,&bloomBuffer);
        for(int level=0;level<3;++level){
            UINT sourceWidth=UINT(std::max(1,bufferW>>level));
            UINT sourceHeight=UINT(std::max(1,bufferH>>level));
            XMFLOAT4 info{1.0f/sourceWidth,1.0f/sourceHeight,
                level==0?1.0f:0.0f,0};
            context->UpdateSubresource(bloomBuffer,0,nullptr,&info,0,0);
            D3D11_VIEWPORT viewport{};
            viewport.Width=float(std::max(1,bufferW>>(level+1)));
            viewport.Height=float(std::max(1,bufferH>>(level+1)));
            viewport.MaxDepth=1;
            context->RSSetViewports(1,&viewport);
            context->OMSetRenderTargets(1,&bloomTarget[level],nullptr);
            ID3D11ShaderResourceView* source=level==0?sceneView:bloomView[level-1];
            context->PSSetShaderResources(0,1,&source);
            context->Draw(4,0);++drawCalls;
            ID3D11ShaderResourceView* empty=nullptr;
            context->PSSetShaderResources(0,1,&empty);
        }
    }
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
        ui::graphicsQuality>0?0.12f:0.0f,
        temporalAA?0.0f:ui::antiAliasingQuality==0?0.75f:
            float(ui::antiAliasingQuality),
        ui::graphicsQuality>0?float(ui::reflectionQuality):0.0f};
    const bool probeDebug=std::strstr(commandLine,"--probe-view")||
        std::strstr(commandLine,"--probe-weight-view");
    if(probeDebug)post.effects={0,0,0,0};
    post.debug.x=std::strstr(commandLine,"--normal-view")?1.0f:
        std::strstr(commandLine,"--roughness-view")?2.0f:
        std::strstr(commandLine,"--indirect-view")?5.0f:
        std::strstr(commandLine,"--bloom-view")?4.0f:
        std::strstr(commandLine,"--shadow-cascade-view")||
        std::strstr(commandLine,"--material-view")||
        std::strstr(commandLine,"--mip-view")||
        std::strstr(commandLine,"--probe-weight-view")?3.0f:0.0f;
    post.previousViewProjection=historyValid?previousViewProjection:
        constants.viewProjection;
    post.temporal={historyValid?1.0f:0.0f,motionDebug?1.0f:0.0f,0,0};
    XMVECTOR solarDirection=XMVector3Normalize(XMVectorSet(
        constants.sun.x,constants.sun.y,constants.sun.z,0));
    XMStoreFloat4(&post.sunDirection,solarDirection);
    post.sunDirection.w=constants.sun.w*(1-weather::current().clouds)*
        (1-weather::current().clouds);
    XMFLOAT4 solarClip;
    XMStoreFloat4(&solarClip,XMVector4Transform(solarDirection,
        XMLoadFloat4x4(&constants.viewProjection)));
    post.sunScreen={solarClip.w>0?solarClip.x/solarClip.w*.5f+.5f:-2,
        solarClip.w>0?.5f-solarClip.y/solarClip.w*.5f:-2,
        .00465f/(2*std::tan(camera::fieldOfView()*PI/360.0f)),
        ui::effectsQuality>0&&solarClip.w>0&&!probeDebug&&!post.debug.x&&
            !std::strstr(commandLine,"--no-lens-flare")?1.0f:0.0f};
    post.debug.w=std::strstr(commandLine,"--flare-view")?1.0f:0.0f;
    post.skyWeather={worldTime*weather::current().wind.x*18,
        worldTime*weather::current().wind.z*18,
        ui::graphicsQuality==0?24.0f:ui::graphicsQuality==1?36.0f:48.0f,daylight};
    context->UpdateSubresource(postBuffer,0,nullptr,&post,0,0);
    if(post.effects.w>0.5f){
        D3D11_VIEWPORT reflectionViewport{};
        reflectionViewport.Width=float(std::max(1,bufferW/2));
        reflectionViewport.Height=float(std::max(1,bufferH/2));
        reflectionViewport.MaxDepth=1;
        context->RSSetViewports(1,&reflectionViewport);
        context->OMSetRenderTargets(1,&reflectionTarget,nullptr);
        context->OMSetDepthStencilState(noDepth,0);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->VSSetShader(hudVS,nullptr,0);
        context->PSSetShader(reflectionPS,nullptr,0);
        context->PSSetSamplers(0,1,&clampSampler);
        context->PSSetConstantBuffers(0,1,&postBuffer);
        ID3D11ShaderResourceView* inputs[4]={sceneView,depthViewSRV,surfaceView,reflectionResponseView};
        context->PSSetShaderResources(0,4,inputs);
        context->Draw(4,0);++drawCalls;
        ID3D11ShaderResourceView* empty[4]{};
        context->PSSetShaderResources(0,4,empty);
    }
    D3D11_VIEWPORT postViewport{};
    postViewport.Width=float(bufferW);postViewport.Height=float(bufferH);
    postViewport.MaxDepth=1;context->RSSetViewports(1,&postViewport);
    context->OMSetRenderTargets(1,&composedTarget,nullptr);
    context->OMSetDepthStencilState(noDepth,0);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(hudVS,nullptr,0);context->PSSetShader(postPS,nullptr,0);
    context->PSSetSamplers(0,1,&clampSampler);
    context->PSSetConstantBuffers(0,1,&postBuffer);
    ID3D11ShaderResourceView* postInputs[8]={sceneView,depthViewSRV,surfaceView,
        bloomView[0],bloomView[1],bloomView[2],reflectionView,indirectView};
    context->PSSetShaderResources(0,8,postInputs);
    context->PSSetShaderResources(8,1,&cloudNoiseView);
    context->PSSetSamplers(1,1,&cloudSampler);
    context->Draw(4,0);++drawCalls;
    ID3D11ShaderResourceView* emptyCloud=nullptr;
    context->PSSetShaderResources(8,1,&emptyCloud);
    ID3D11ShaderResourceView* emptyInputs[8]{};
    context->PSSetShaderResources(0,8,emptyInputs);
    dx11::tone::Constants tone;
    tone.grade[0]=post.grade.x;tone.grade[1]=post.grade.y;
    tone.grade[2]=post.grade.z;tone.grade[3]=post.grade.w;
    const auto now=std::chrono::steady_clock::now();
    tone.adaptation[0]=std::strstr(commandLine,"--smoke")?1.0f/60:
        exposureValid?std::chrono::duration<float>(now-exposureClock).count():0;
    exposureClock=now;
    tone.adaptation[1]=exposureValid?0:1;
    tone.adaptation[2]=std::strstr(commandLine,"--fixed-exposure")?0:1;
    tone.adaptation[3]=post.debug.x||post.debug.w||motionDebug?1:0;
    tone.options[0]=std::strstr(commandLine,"--legacy-tonemap")?1:0;
    context->UpdateSubresource(toneBuffer,0,nullptr,&tone,0,0);
    context->PSSetConstantBuffers(1,1,&toneBuffer);
    ID3D11ShaderResourceView* toneEmpty[2]{};
    if(!tone.adaptation[3]){
        context->PSSetShader(meterPS,nullptr,0);
        for(unsigned level=0;level<dx11::tone::meterLevels;++level){
            D3D11_VIEWPORT viewport{};viewport.Width=viewport.Height=float(dx11::tone::meterSize>>level);viewport.MaxDepth=1;
            context->RSSetViewports(1,&viewport);
            context->OMSetRenderTargets(1,&meterTarget[level],nullptr);
            ID3D11ShaderResourceView* inputs[2]={level?meterView[level-1]:composedView,level?nullptr:depthViewSRV};
            context->PSSetShaderResources(0,2,inputs);
            context->PSSetShader(level?meterReducePS:meterPS,nullptr,0);
            context->Draw(4,0);++drawCalls;
            context->PSSetShaderResources(0,2,toneEmpty);
        }
        const int writeIndex=1-exposureIndex;
        context->OMSetRenderTargets(1,&exposureTarget[writeIndex],nullptr);
        context->PSSetShader(exposurePS,nullptr,0);
        ID3D11ShaderResourceView* inputs[2]={meterView[dx11::tone::meterLevels-1],exposureView[exposureIndex]};
        context->PSSetShaderResources(0,2,inputs);
        context->Draw(4,0);++drawCalls;
        context->PSSetShaderResources(0,2,toneEmpty);
        exposureIndex=writeIndex;exposureValid=true;
    }
    context->RSSetViewports(1,&postViewport);
    context->OMSetRenderTargets(1,&postTarget,nullptr);
    context->PSSetShader(tonePS,nullptr,0);
    ID3D11ShaderResourceView* toneInputs[2]={composedView,exposureView[exposureIndex]};
    context->PSSetShaderResources(0,2,toneInputs);
    context->Draw(4,0);++drawCalls;
    context->PSSetShaderResources(0,2,toneEmpty);
    ID3D11ShaderResourceView* displayImage=postView;
    if(temporalAA||motionDebug){
        context->OMSetRenderTargets(1,&motionTarget,nullptr);
        context->PSSetShader(motionPS,nullptr,0);
        ID3D11ShaderResourceView* motionInputs[3]={depthViewSRV,indirectView,objectMotionView};
        context->PSSetShaderResources(0,3,motionInputs);
        context->Draw(4,0);++drawCalls;
        ID3D11ShaderResourceView* emptyMotion[3]{};
        context->PSSetShaderResources(0,3,emptyMotion);
        ID3D11ShaderResourceView* empty=nullptr;
        int writeIndex=1-historyIndex;
        context->OMSetRenderTargets(1,&historyTarget[writeIndex],nullptr);
        context->PSSetShader(temporalPS,nullptr,0);
        ID3D11ShaderResourceView* temporalInputs[4]={postView,depthViewSRV,
            motionView,historyView[historyIndex]};
        context->PSSetShaderResources(0,4,temporalInputs);
        context->Draw(4,0);++drawCalls;
        ID3D11ShaderResourceView* emptyTemporal[4]{};
        context->PSSetShaderResources(0,4,emptyTemporal);
        displayImage=historyView[writeIndex];
        historyIndex=writeIndex;
        historyValid=temporalAA||motionDebug;
    }
    context->OMSetRenderTargets(1,&target,nullptr);
    context->PSSetShader(temporalCopyPS,nullptr,0);
    context->PSSetShaderResources(0,1,&displayImage);
    context->Draw(4,0);++drawCalls;
    ID3D11ShaderResourceView* displayEmpty=nullptr;
    context->PSSetShaderResources(0,1,&displayEmpty);
    if(std::strstr(commandLine,"--exposure-log")&&exposureFrame%30==0){
        D3D11_TEXTURE2D_DESC description{};exposureTexture[exposureIndex]->GetDesc(&description);
        description.Usage=D3D11_USAGE_STAGING;description.BindFlags=0;description.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* staging=nullptr;
        if(SUCCEEDED(device->CreateTexture2D(&description,nullptr,&staging))){
            context->CopyResource(staging,exposureTexture[exposureIndex]);
            D3D11_MAPPED_SUBRESOURCE read{};
            if(SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&read))){
                const float* values=static_cast<const float*>(read.pData);char info[180]{};
                std::snprintf(info,sizeof(info),"Exposure frame %u: hour %.2f, scale %.6f, target %.6f, EV %.6f",exposureFrame,gameHour,std::exp2(values[0]),std::exp2(values[1]),values[0]);
                logging::write(info);context->Unmap(staging,0);
            }
            release(staging);
        }
    }
    ++exposureFrame;
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
    endGpuQueries();
    captureIfRequested();
    auto beforePresent=std::chrono::steady_clock::now();
    HRESULT presentResult=swapChain->Present(std::strstr(GetCommandLineA(),"--benchmark")?0:1,0);
    if(FAILED(presentResult)){
        char failure[128]{};
        std::snprintf(failure,sizeof(failure),
            "DX11 Present failed: 0x%08lX; device reason: 0x%08lX",
            static_cast<unsigned long>(presentResult),
            static_cast<unsigned long>(device->GetDeviceRemovedReason()));
        logging::write(failure);
        deviceLost=true;
    }
    if(SUCCEEDED(presentResult)){
        previousViewProjection=constants.viewProjection;
        previousCameraEye={pose.eye.x,pose.eye.y,pose.eye.z};
        previousCameraForward=forward;
        ++temporalFrame;
    }
    auto afterPresent=std::chrono::steady_clock::now();
    renderSceneMs=std::chrono::duration<float,std::milli>(sceneBuilt-renderBegin).count();
    renderUploadMs=std::chrono::duration<float,std::milli>(instancesReady-sceneBuilt).count();
    renderDrawMs=std::chrono::duration<float,std::milli>(beforePresent-instancesReady).count();
    renderPresentMs=std::chrono::duration<float,std::milli>(afterPresent-beforePresent).count();
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
bool bakeReflectionProbes(){
    probeBakeActive=true;probeBakeFailed=false;
    screenW=screenH=128;ui::shadowQuality=1;ui::antiAliasingQuality=0;
    ui::graphicsQuality=2;ui::textureQuality=2;ui::filteringQuality=2;
    ui::reflectionQuality=0;ui::effectsQuality=0;ui::drawDistance=21;ui::lodDistance=50;
    ui::vegetationDensity=2;ui::grassDistance=50;
    weather::set("clear");worldTime=0;
    for(probeBakeFrame=0;probeBakeFrame<36&&!probeBakeFailed;++probeBakeFrame){
        // Any early return in render is a failed capture, not a successful bake.
        probeBakeFailed=true;render();
    }
    probeBakeActive=false;
    if(!probeBakeFailed&&probeBakeFrame==36){
        std::ofstream manifest(std::filesystem::path(executableFolder())/"probe-captures"/"capture.json");
        manifest<<R"JSON({"schema":1,"faces":36,"size":128,"field_of_view":90,
"hours":[12,18.5,22],"graphics":2,"shadows":1,"textures":2,"filtering":2,
"vegetation":2,"grass_distance":50,"draw_distance":21,"lod_distance":50,
"weather":"clear","world_time":0,"dynamic_geometry":false,"dynamic_lights":false,
"probe_feedback":false,"tone_mapping":false,"coordinates":"D3D cube face orientation; world Y up"}
)JSON";
        probeBakeFailed=!manifest;
    }
    return !probeBakeFailed&&probeBakeFrame==36;
}
void shutdownRenderer(){
    scenePool.reset();
    dx11::shutdownHud();
    if(context)context->ClearState();
    release(cloudNoiseView);release(cloudSampler);
    releaseGpuQueries();
    releaseTargets();
    release(shadowView);
    for(auto& depth:shadowDepth)release(depth);
    release(shadowTexture);
    release(headlightShadowView);release(headlightShadowDepth);
    release(headlightShadowTexture);
    release(streetShadowView);release(streetShadowDepth);
    release(streetShadowTexture);
    shadowSize=shadowCascadeCount=0;
    for(auto& texture:textures)release(texture);
    for(auto& texture:detailTextures)release(texture);
    for(auto& texture:normalTextures)release(texture);
    release(sampler);release(modelSampler);release(clampSampler);release(shadowSampler);release(hudBlend);release(alphaBlend);release(readDepth);release(noDepth);
    activeFiltering=-1;
    release(rasterState);release(shadowRaster);
    release(toneBuffer);release(bloomBuffer);release(postBuffer);release(sceneBuffer);release(vertexBuffer);release(staticBuffer);release(inputLayout);
    release(probeBuffer);release(probeView);release(brdfView);probeSet={};
    probeBakeActive=probeBakeFailed=false;probeBakeFrame=0;
    release(instanceBuffer);release(instanceLayout);
    release(skinOutputUav);release(skinOutput);release(skinConstants);release(skinCS);
    release(skinPreviousUav);release(skinPreviousOutput);release(skinLayout);release(skinVS);
    previousSkins.clear();skinFrame=0;
    for(auto& item:skinSources)release(item.second);
    skinSources.clear();gpuSkins.clear();skinCapacity=skinVertexCount=0;
    skinValidationDone=false;
    for(auto& item:meshBuffers)release(item.second);
    meshBuffers.clear();
    meshRevisions.clear();
    for(auto& item:meshIndexBuffers)release(item.second);
    meshIndexBuffers.clear();
    for(auto& item:modelTextures)release(item.second);
    modelTextures.clear();
    for(auto& item:sharedModelTextures)release(item.second);
    sharedModelTextures.clear();
    for(auto& item:pbrTextures)release(item.second);
    pbrTextures.clear();
    loadedTextureBytes=loadedDdsBytes=0;loadedTextureCount=loadedDdsCount=0;
    release(sceneVS);release(instanceVS);release(scenePS);release(alphaShadowPS);release(sceneHS);release(sceneDS);
    release(hudVS);release(hudPS);release(postPS);release(bloomPS);release(reflectionPS);
    release(motionPS);release(temporalPS);release(temporalCopyPS);
    release(meterPS);release(meterReducePS);release(exposurePS);release(tonePS);
    if(swapChain)swapChain->SetFullscreenState(FALSE,nullptr);
    release(swapChain);release(context);release(device);
    deviceLost=false;
    if(gdiplusToken){Gdiplus::GdiplusShutdown(gdiplusToken);gdiplusToken=0;}
}
}
