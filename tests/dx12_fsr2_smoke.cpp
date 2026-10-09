#include "../src/fsr2.h"
#include "../src/logging.h"
#include <DirectXPackedVector.h>
#include <cstdio>
#include <cmath>
#include <stdexcept>

void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void dx12SkinScenarios(dx12::Device& device,dx12::Context& commands);
void dx12StateScenarios(dx12::Device& device,dx12::Context& commands);
void dx12RecordingScenarios(dx12::Device& device,dx12::Context& commands);
void dx12VisibilityScenarios();
void dx12CloudScenarios(dx12::Device& device,dx12::Context& commands);
int main(){
    logging::initialize();
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DX12FSR2Test";RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"DX12 test",WS_OVERLAPPEDWINDOW,0,0,640,360,nullptr,nullptr,wc.hInstance,nullptr);
    try{
        dx12VisibilityScenarios();
        dx12::Device device;dx12::Context commands(&device);device.context=&commands;
        require(SUCCEEDED(device.initialize(window,640,360)),"native device creation");
        dx12SkinScenarios(device,commands);
        dx12StateScenarios(device,commands);
        dx12RecordingScenarios(device,commands);
        dx12CloudScenarios(device,commands);
        dx12::BufferDesc persistentDesc{};persistentDesc.ByteWidth=256;persistentDesc.Usage=dx12::Dynamic;persistentDesc.BindFlags=dx12::Constant;
        std::array<unsigned,64> persistentData{};persistentData.fill(0xABCDEF01);
        dx12::InitialData persistentInitial{persistentData.data()};dx12::Resource* persistent=nullptr;
        require(SUCCEEDED(device.CreateBuffer(&persistentDesc,&persistentInitial,&persistent)),"persistent upload allocation");
        // Submit transient uploads without draining the queue. Their owners may
        // die immediately, but each frame's GPU data must remain intact.
        std::array<dx12::Resource*,12> copies{};
        for(unsigned frame=0;frame<copies.size();++frame){
            dx12::BufferDesc desc{};desc.ByteWidth=256;desc.BindFlags=dx12::Vertex;
            dx12::Resource* source=nullptr;
            std::array<unsigned,64> data{};data.fill(0x12340000+frame);
            dx12::InitialData initial{data.data()};
            require(SUCCEEDED(device.CreateBuffer(&desc,&initial,&source)),"transient upload allocation");
            desc.Usage=dx12::Readback;desc.BindFlags=0;
            require(SUCCEEDED(device.CreateBuffer(&desc,nullptr,&copies[frame])),"frame readback allocation");
            commands.CopyResource(copies[frame],source);source->Release();device.submit(false);
        }
        for(unsigned frame=0;frame<copies.size();++frame){
            dx12::MappedData mapped{};require(SUCCEEDED(commands.Map(copies[frame],0,dx12::MapRead,0,&mapped)),"frame readback");
            for(unsigned i=0;i<64;++i)require(static_cast<unsigned*>(mapped.pData)[i]==0x12340000+frame,"in-flight upload/resource lifetime");
            commands.Unmap(copies[frame],0);copies[frame]->Release();
        }
        // A long-lived constant buffer must pin its upload page even when it
        // has not been used by any recent command list.
        persistentDesc.Usage=dx12::Readback;persistentDesc.BindFlags=0;dx12::Resource* persistentRead=nullptr;
        require(SUCCEEDED(device.CreateBuffer(&persistentDesc,nullptr,&persistentRead)),"persistent readback allocation");
        device.keep(persistent);device.keep(persistentRead);
        device.commands->CopyBufferRegion(persistentRead->native.Get(),0,persistent->native.Get(),persistent->offset,256);
        persistent->Release();device.submit(false);
        dx12::MappedData persistentMap{};require(SUCCEEDED(commands.Map(persistentRead,0,dx12::MapRead,0,&persistentMap)),"persistent upload readback");
        for(unsigned i=0;i<64;++i)require(static_cast<unsigned*>(persistentMap.pData)[i]==persistentData[i],"live constant buffer must prevent upload recycling");
        commands.Unmap(persistentRead,0);persistentRead->Release();
        std::printf("PASS 12 asynchronous submissions preserve transient uploads and resources\n");
        std::printf("PASS long-lived constant buffer survives upload-page recycling\n");
        dx12::Fsr2 fsr;
        for(int step=0;step<10;++step){
            const int mode=step%5,w=step<5?640:800,h=step<5?360:450;
            commands.ClearState();fsr.configure(device,w,h,mode);require(SUCCEEDED(device.resize(w,h)),"swapchain resize");
            if(!mode){require(!fsr.active&&fsr.renderWidth==w&&fsr.renderHeight==h,"Off must use native resolution");continue;}
            const float scales[]={1,1.5f,1.7f,2,3};
            require(std::abs(fsr.renderWidth-int(w/scales[mode]))<=1&&std::abs(fsr.renderHeight-int(h/scales[mode]))<=1,"quality render dimensions");
            dx12::Resource *color=nullptr,*depth=nullptr,*motion=nullptr,*reactive=nullptr;dx12::View *cv=nullptr,*dv=nullptr,*mv=nullptr,*rv=nullptr;
            dx12::TextureDesc desc{};desc.Width=fsr.renderWidth;desc.Height=fsr.renderHeight;desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.BindFlags=dx12::RenderTarget|dx12::ShaderInput;
            auto target=[&](dx12::Resource** r,dx12::View** v){require(SUCCEEDED(device.CreateTexture2D(&desc,nullptr,r)),"texture allocation");require(SUCCEEDED(device.CreateRenderTargetView(*r,nullptr,v)),"RTV allocation");};
            target(&color,&cv);target(&motion,&mv);desc.Format=DXGI_FORMAT_R8_UNORM;target(&reactive,&rv);
            desc.Format=DXGI_FORMAT_R32_TYPELESS;desc.BindFlags=dx12::DepthTarget|dx12::ShaderInput;require(SUCCEEDED(device.CreateTexture2D(&desc,nullptr,&depth)),"depth allocation");
            D3D12_DEPTH_STENCIL_VIEW_DESC dd{};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;require(SUCCEEDED(device.CreateDepthStencilView(depth,&dd,&dv)),"depth view");
            const float signal[]={0.25f,0.5f,0.75f,1},zero[4]{};
            for(unsigned frame=0;frame<16;++frame){
                commands.ClearRenderTargetView(cv,signal);commands.ClearRenderTargetView(mv,zero);commands.ClearRenderTargetView(rv,zero);commands.ClearDepthStencilView(dv,D3D12_CLEAR_FLAG_DEPTH,0.5f,0);
                fsr.jitter(frame);require(std::abs(fsr.jitterX)<=0.5f&&std::abs(fsr.jitterY)<=0.5f,"jitter bounds");
                fsr.dispatch(device,color,depth,motion,reactive,frame==0||frame==8,16.667f,2,5000,1.1f,0.2f);device.submit(false);
            }
            auto readDesc=fsr.output->texture;readDesc.Usage=dx12::Readback;readDesc.BindFlags=0;dx12::Resource* readback=nullptr;
            require(SUCCEEDED(device.CreateTexture2D(&readDesc,nullptr,&readback)),"readback allocation");commands.CopyResource(readback,fsr.output);dx12::MappedData mapped{};require(SUCCEEDED(commands.Map(readback,0,dx12::MapRead,0,&mapped)),"readback mapping");
            for(int y=h/4;y<3*h/4;y+=31)for(int x=w/4;x<3*w/4;x+=31){const auto* pixel=reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(mapped.pData)+size_t(y)*mapped.RowPitch)+4*x;
                for(int c=0;c<3;++c){float value=DirectX::PackedVector::XMConvertHalfToFloat(pixel[c]);require(std::isfinite(value)&&std::abs(value-signal[c])<0.035f,"FSR2 must reconstruct the known color signal");}}
            commands.Unmap(readback,0);readback->Release();cv->Release();mv->Release();rv->Release();dv->Release();color->Release();motion->Release();reactive->Release();depth->Release();
            std::printf("PASS %s %dx%d -> %dx%d, temporal reset and GPU readback\n",dx12::fsr2Modes[mode],fsr.renderWidth,fsr.renderHeight,w,h);
        }
        fsr.destroy(device);require(device.validationErrors==0,"DX12 validation errors");
        std::printf("PASS native DX12 resource lifetime, resizing, all FSR2 modes; validation errors: %u\n",device.validationErrors);
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());DestroyWindow(window);logging::shutdown();return 1;}
    DestroyWindow(window);logging::shutdown();return 0;
}
