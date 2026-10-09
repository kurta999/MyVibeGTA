#include "dx12_backend.h"
#include "logging.h"
#include "cpu_jobs.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <cstdio>
#include <chrono>

namespace dx12 {
namespace {
void check(HRESULT hr){if(FAILED(hr)){char s[80];std::snprintf(s,sizeof(s),"DX12 failure 0x%08lX",hr);logging::write(s);throw std::runtime_error(s);}}
D3D12_RESOURCE_DESC bufferDesc(UINT64 bytes){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return d;}
D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type){D3D12_HEAP_PROPERTIES p{};p.Type=type;p.CreationNodeMask=p.VisibleNodeMask=1;return p;}
D3D12_SHADER_BYTECODE bytecode(Shader* s){return s?D3D12_SHADER_BYTECODE{s->code.data(),s->code.size()}:D3D12_SHADER_BYTECODE{};}
View* view(Device* owner,Resource* r,D3D12_DESCRIPTOR_HEAP_TYPE type){auto h=owner->allocate(type);auto* v=new View;v->owner=owner;v->heapType=type;v->resource=r;r->AddRef();v->handle=h;v->format=r->desc.Format;return v;}
}
View::~View(){if(resource)resource->Release();if(owner)owner->freeDescriptors[heapType].push_back(handle);}
SamplerState::~SamplerState(){if(owner)owner->freeDescriptors[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].push_back(handle);}
Device::~Device(){if(queue&&fence&&eventHandle){try{submit();}catch(...){}}if(eventHandle)CloseHandle(eventHandle);}
HRESULT Device::initialize(HWND window,UINT width,UINT height){
    if(std::strstr(GetCommandLineA(),"--dx12-debug")){
        ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    }
    check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    for(UINT i=0;factory->EnumAdapters1(i,&adapter)!=DXGI_ERROR_NOT_FOUND;++i){
        DXGI_ADAPTER_DESC1 d{};adapter->GetDesc1(&d);
        if(!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)&&SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&native))))break;
        adapter.Reset();
    }
    if(!native){check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&native)));}
    native.As(&infoQueue);
    DXGI_ADAPTER_DESC1 ad{};adapter->GetDesc1(&ad);char name[160]{};WideCharToMultiByte(CP_UTF8,0,ad.Description,-1,name,sizeof(name),nullptr,nullptr);
    logging::write((std::string("Native Direct3D 12 adapter: ")+name).c_str());
    D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;check(native->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
    check(queue->GetTimestampFrequency(&timestampFrequency));
    DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=width;sd.Height=height;sd.Format=DXGI_FORMAT_B8G8R8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferCount=2;
    sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;sd.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    ComPtr<IDXGISwapChain1> initial;check(factory->CreateSwapChainForHwnd(queue.Get(),window,&sd,nullptr,nullptr,&initial));check(initial.As(&swap));
    factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER);
    for(auto& frame:frames)check(native->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&frame.allocator)));
    allocator=frames[0].allocator;
    check(native->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)));
    frames[0].primaryLists.push_back(commands);frames[0].primaryCursor=1;
    check(native->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));eventHandle=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!eventHandle)return HRESULT_FROM_WIN32(GetLastError());
    auto makeHeap=[&](D3D12_DESCRIPTOR_HEAP_TYPE type,UINT count,bool visible,ComPtr<ID3D12DescriptorHeap>& out){D3D12_DESCRIPTOR_HEAP_DESC d{};d.Type=type;d.NumDescriptors=count;d.Flags=visible?D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE:D3D12_DESCRIPTOR_HEAP_FLAG_NONE;check(native->CreateDescriptorHeap(&d,IID_PPV_ARGS(&out)));};
    makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,65536,false,views);makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,16384,false,rtvs);makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,4096,false,dsvs);makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,2048,false,samplers);
    for(auto& frame:frames){makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,262144,true,frame.views);makeHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,2048,true,frame.samplers);}
    gpuViews=frames[0].views;gpuSamplers=frames[0].samplers;
    viewStep=native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);rtvStep=native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);dsvStep=native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);samplerStep=native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    nullTexture=allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc{};nullDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;nullDesc.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;nullDesc.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;nullDesc.Texture2D.MipLevels=1;
    native->CreateShaderResourceView(nullptr,&nullDesc,nullTexture);
    D3D12_ROOT_PARAMETER parameters[7]{};for(UINT i=0;i<4;++i){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[i].Descriptor.ShaderRegister=i;}
    D3D12_DESCRIPTOR_RANGE ranges[3]{};ranges[0]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,16,0,0,0};ranges[1]={D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,4,0,0,0};ranges[2]={D3D12_DESCRIPTOR_RANGE_TYPE_UAV,2,0,0,0};
    for(UINT i=0;i<3;++i){parameters[4+i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[4+i].DescriptorTable={1,&ranges[i]};}
    D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=7;rd.pParameters=parameters;rd.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> blob,error;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error));check(native->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
    auto hp=heap(D3D12_HEAP_TYPE_UPLOAD);auto bd=bufferDesc(65536);check(native->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&zeroConstants)));
    void* zero=nullptr;check(zeroConstants->Map(0,nullptr,&zero));std::memset(zero,0,65536);zeroConstants->Unmap(0,nullptr);
    D3D12_QUERY_HEAP_DESC qh{};qh.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;qh.Count=128;check(native->CreateQueryHeap(&qh,IID_PPV_ARGS(&queryHeap)));
    hp=heap(D3D12_HEAP_TYPE_READBACK);bd=bufferDesc(128*8);check(native->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&queryReadback)));
    return S_OK;
}
void Device::waitFor(UINT64 value){
    if(fence->GetCompletedValue()<value){check(fence->SetEventOnCompletion(value,eventHandle));if(WaitForSingleObject(eventHandle,30000)!=WAIT_OBJECT_0)throw std::runtime_error("DX12 GPU fence timeout");}
}
void Device::submit(bool wait){
    if(context)context->endParallel();
    joinRecordings();
    check(commands->Close());pendingLists.push_back(commands.Get());
    // Submit ordered primary/worker segments together: implicit buffer decay
    // happens between ExecuteCommandLists calls, not between these segments.
    queue->ExecuteCommandLists(UINT(pendingLists.size()),pendingLists.data());pendingLists.clear();
    check(queue->Signal(fence.Get(),++serial));
    auto& submitted=frames[frameIndex];submitted.fence=serial;submitted.resources.swap(retained);submitted.uploads.swap(activeUploads);
    frameIndex=(frameIndex+1)%frameCount;
    auto& next=frames[frameIndex];
    waitFor(wait?serial:next.fence);
    // A full drain must release every retained swap-chain reference before resize.
    if(wait)for(auto& frame:frames){frame.resources.clear();frame.uploads.clear();}
    else {next.resources.clear();next.uploads.clear();}
    allocator=next.allocator;gpuViews=next.views;gpuSamplers=next.samplers;
    if(infoQueue){
        for(UINT64 i=0;i<infoQueue->GetNumStoredMessages();++i){SIZE_T size=0;infoQueue->GetMessage(i,nullptr,&size);std::vector<unsigned char> bytes(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());infoQueue->GetMessage(i,message,&size);
            if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){logging::write(message->pDescription);++validationErrors;}
            else if(message->Severity==D3D12_MESSAGE_SEVERITY_WARNING&&reportedWarnings.insert(unsigned(message->ID)).second)logging::write(message->pDescription);}
        infoQueue->ClearStoredMessages();
    }
    retained.clear();retainedSet.clear();activeUploads.clear();uploadBlock.reset();uploadOffset=uploadBytes=0;gpuViewCount=gpuSamplerCount=0;samplerTables.clear();textureTables.clear();
    // Keep a bounded reserve after large startup/streaming uploads. Pages still
    // referenced by a buffer or an in-flight command list cannot be recycled.
    UINT64 reserve=0;
    uploadPool.erase(std::remove_if(uploadPool.begin(),uploadPool.end(),[&](const auto& page){
        if(page.use_count()!=1)return false;
        reserve+=page->size;return reserve>128ull*1024*1024;
    }),uploadPool.end());
    check(allocator->Reset());next.primaryCursor=next.workerCursor=0;nextPrimary();
    if(context)context->invalidateBindings();
}
void Device::transition(Resource* r,D3D12_RESOURCE_STATES state){
    if(!r||!r->native)return;keep(r);
    if(r->dynamic&&r->desc.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER)return;
    if(r->state==state){if(state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;b.UAV.pResource=r->native.Get();commands->ResourceBarrier(1,&b);}return;}
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r->native.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,r->state,state};commands->ResourceBarrier(1,&b);r->state=state;
}
void Device::nextPrimary(){
    // Primary segments are recorded serially on the owner thread and share its
    // allocator. The frame fence protects all of them before allocator reset.
    auto& frame=frames[frameIndex];size_t index=frame.primaryCursor++;
    if(index==frame.primaryLists.size()){
        ComPtr<ID3D12GraphicsCommandList> list;
        check(native->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
        frame.primaryLists.push_back(list);
    }else check(frame.primaryLists[index]->Reset(allocator.Get(),nullptr));
    commands=frame.primaryLists[index];if(context)context->invalidateBindings();
}
void Device::leaseRecordingView(View* view){
    if(view&&recordingViewSet.insert(view).second){view->AddRef();recordingViews.push_back(view);}
}
void Device::recordDraws(cpu::Pool& pool,std::vector<Context::DrawPacket>&& draws){
    if(draws.empty())return;
    check(commands->Close());pendingLists.push_back(commands.Get());
    auto packets=std::make_shared<std::vector<Context::DrawPacket>>(std::move(draws));
    auto& frame=frames[frameIndex];
    const unsigned chunks=std::min(pool.concurrency(),unsigned((packets->size()+63)/64));
    ++recordingStats.batches;recordingStats.lists+=chunks;recordingStats.draws+=packets->size();
    auto* rootSignature=root.Get();auto* viewHeap=gpuViews.Get();auto* samplerHeap=gpuSamplers.Get();
    for(unsigned chunk=0;chunk<chunks;++chunk){
        size_t index=frame.workerCursor++;
        if(index==frame.workerLists.size()){
            Frame::Recording recording;
            check(native->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&recording.allocator)));
            check(native->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,recording.allocator.Get(),nullptr,IID_PPV_ARGS(&recording.commands)));
            frame.workerLists.push_back(std::move(recording));
        }else{
            auto& recording=frame.workerLists[index];check(recording.allocator->Reset());check(recording.commands->Reset(recording.allocator.Get(),nullptr));
        }
        auto* list=frame.workerLists[index].commands.Get();pendingLists.push_back(list);
        size_t first=packets->size()*chunk/chunks,last=packets->size()*(chunk+1)/chunks;
        recordingJobs.push_back(pool.submit([this,packets,list,rootSignature,viewHeap,samplerHeap,first,last]{
            auto started=std::chrono::steady_clock::now();unsigned active=++activeRecorders,peak=peakRecorders.load();
            while(active>peak&&!peakRecorders.compare_exchange_weak(peak,active)){}
            struct Done {std::atomic<unsigned>& active;~Done(){--active;}} done{activeRecorders};
            RecordingResult result;Context::Applied applied;
            for(size_t i=first;i<last;++i){const auto& draw=(*packets)[i];
                Context::emitGraphics(list,rootSignature,viewHeap,samplerHeap,draw.state,applied,result.binds);
                if(draw.indexed)list->DrawIndexedInstanced(draw.count,draw.instances,draw.start,draw.base,draw.first);
                else list->DrawInstanced(draw.count,draw.instances,draw.start,draw.first);
            }
            check(list->Close());result.cpuMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return result;
        }));
    }
    nextPrimary();
}
void Device::joinRecordings(){
    const auto started=std::chrono::steady_clock::now();std::exception_ptr failure;
    for(auto& job:recordingJobs)try{
        auto result=job.get();recordingStats.cpuMs+=result.cpuMs;
        if(context){auto& stats=context->bindStats;stats.draws+=result.binds.draws;stats.pipelineBinds+=result.binds.pipelineBinds;
            stats.heapBinds+=result.binds.heapBinds;stats.constantBinds+=result.binds.constantBinds;}
    }catch(...){if(!failure)failure=std::current_exception();}
    if(!recordingJobs.empty())recordingStats.waitMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    recordingJobs.clear();recordingStats.peakActive=peakRecorders.load();
    for(auto* view:recordingViews)view->Release();recordingViews.clear();recordingViewSet.clear();
    if(failure)std::rethrow_exception(failure);
}
Context::~Context(){
    if(owner&&owner->context==this){try{if(owner->commands&&owner->fence)owner->submit();else owner->joinRecordings();}catch(...){}owner->context=nullptr;}
}
void Context::beginParallel(cpu::Pool* pool){
    endParallel();if(pool&&pool->concurrency()>1)recordPool=pool;
}
void Context::endParallel(){
    if(!recordPool)return;
    auto* pool=recordPool;recordPool=nullptr;
    owner->recordDraws(*pool,std::move(drawPackets));drawPackets.clear();
}
void Context::emitGraphics(ID3D12GraphicsCommandList* cmd,ID3D12RootSignature* root,ID3D12DescriptorHeap* views,ID3D12DescriptorHeap* samplers,
    const Applied& state,Applied& applied,BindStats& stats){
    if(!applied.heaps){ID3D12DescriptorHeap* heaps[]={views,samplers};cmd->SetDescriptorHeaps(2,heaps);applied.heaps=true;++stats.heapBinds;}
    if(!applied.roots[0]){cmd->SetGraphicsRootSignature(root);applied.roots[0]=true;}
    for(unsigned i=0;i<4;++i)if(applied.cb[0][i]!=state.cb[0][i]){cmd->SetGraphicsRootConstantBufferView(i,state.cb[0][i]);applied.cb[0][i]=state.cb[0][i];++stats.constantBinds;}
    if(applied.textures[0]!=state.textures[0]){cmd->SetGraphicsRootDescriptorTable(4,{state.textures[0]});applied.textures[0]=state.textures[0];}
    if(applied.samplers!=state.samplers){cmd->SetGraphicsRootDescriptorTable(5,{state.samplers});applied.samplers=state.samplers;}
    if(applied.pipeline!=state.pipeline){cmd->SetPipelineState(state.pipeline);applied.pipeline=state.pipeline;++stats.pipelineBinds;}
    if(!applied.graphics||applied.targetCount!=state.targetCount||applied.depthIdentity!=state.depthIdentity||applied.targetIdentities!=state.targetIdentities){
        cmd->OMSetRenderTargets(state.targetCount,state.targets.data(),FALSE,state.depth.ptr?&state.depth:nullptr);
        applied.targetCount=state.targetCount;applied.depth=state.depth;applied.depthIdentity=state.depthIdentity;applied.targetIdentities=state.targetIdentities;applied.targets=state.targets;
    }
    if(!applied.graphics||std::memcmp(&applied.viewport,&state.viewport,sizeof(state.viewport))){
        cmd->RSSetViewports(1,&state.viewport);D3D12_RECT rect{0,0,LONG(state.viewport.TopLeftX+state.viewport.Width),LONG(state.viewport.TopLeftY+state.viewport.Height)};
        cmd->RSSetScissorRects(1,&rect);applied.viewport=state.viewport;
    }
    if(applied.topology!=state.topology){cmd->IASetPrimitiveTopology(state.topology);applied.topology=state.topology;}
    if(!applied.graphics||std::memcmp(applied.vertices.data(),state.vertices.data(),sizeof(state.vertices))){cmd->IASetVertexBuffers(0,8,state.vertices.data());applied.vertices=state.vertices;}
    if(!applied.graphics||std::memcmp(&applied.indices,&state.indices,sizeof(state.indices))){cmd->IASetIndexBuffer(state.indices.BufferLocation?&state.indices:nullptr);applied.indices=state.indices;}
    applied.graphics=true;++stats.draws;
}
std::shared_ptr<UploadPage> Device::upload(UINT64 bytes,ComPtr<ID3D12Resource>& out,UINT64& offset,void*& cpu){
    bytes=(bytes+511)&~UINT64(511);
    if(!uploadBlock||uploadOffset+bytes>uploadBlock->size){
        uploadBlock.reset();
        for(const auto& page:uploadPool)if(page.use_count()==1&&page->size>=bytes&&(!uploadBlock||page->size<uploadBlock->size))uploadBlock=page;
        if(!uploadBlock){
            uploadBlock=std::make_shared<UploadPage>();uploadBlock->size=std::max(UINT64(8*1024*1024),bytes);
            auto hp=heap(D3D12_HEAP_TYPE_UPLOAD);auto bd=bufferDesc(uploadBlock->size);
            check(native->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&uploadBlock->native)));
            D3D12_RANGE noRead{0,0};check(uploadBlock->native->Map(0,&noRead,reinterpret_cast<void**>(&uploadBlock->cpu)));uploadPool.push_back(uploadBlock);
        }
        activeUploads.push_back(uploadBlock);uploadOffset=0;
    }
    out=uploadBlock->native;offset=uploadOffset;cpu=uploadBlock->cpu+uploadOffset;uploadOffset+=bytes;uploadBytes+=bytes;
    return uploadBlock;
}
D3D12_CPU_DESCRIPTOR_HANDLE Device::allocate(D3D12_DESCRIPTOR_HEAP_TYPE type){
    if(!freeDescriptors[type].empty()){auto h=freeDescriptors[type].back();freeDescriptors[type].pop_back();return h;}
    UINT *count=nullptr,step=0,limit=0;ID3D12DescriptorHeap* h=nullptr;
    if(type==D3D12_DESCRIPTOR_HEAP_TYPE_RTV){count=&rtvCount;step=rtvStep;h=rtvs.Get();limit=16384;}
    else if(type==D3D12_DESCRIPTOR_HEAP_TYPE_DSV){count=&dsvCount;step=dsvStep;h=dsvs.Get();limit=4096;}
    else if(type==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER){count=&samplerCount;step=samplerStep;h=samplers.Get();limit=2048;}
    else{count=&viewCount;step=viewStep;h=views.Get();limit=65536;}
    if(*count>=limit)throw std::runtime_error("DX12 CPU descriptor heap exhausted");auto handle=h->GetCPUDescriptorHandleForHeapStart();handle.ptr+=SIZE_T((*count)++)*step;return handle;
}
HRESULT Device::resource(Resource* r,D3D12_HEAP_TYPE type){auto hp=heap(type);auto d=r->desc;
    if(r->readback&&d.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER){UINT n=d.MipLevels*d.DepthOrArraySize;UINT64 size=0;r->footprints.resize(n);r->rows.resize(n);r->rowBytes.resize(n);native->GetCopyableFootprints(&d,0,n,0,r->footprints.data(),r->rows.data(),r->rowBytes.data(),&size);d=bufferDesc(size);}
    if(type==D3D12_HEAP_TYPE_READBACK)r->state=D3D12_RESOURCE_STATE_COPY_DEST;
    D3D12_CLEAR_VALUE clear{};const D3D12_CLEAR_VALUE* optimized=nullptr;
    if(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL){clear.Format=d.Format==DXGI_FORMAT_R32_TYPELESS?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_D24_UNORM_S8_UINT;clear.DepthStencil.Depth=1;optimized=&clear;}
    return native->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,r->state,optimized,IID_PPV_ARGS(&r->native));
}
HRESULT Device::CreateBuffer(const dx12::BufferDesc* d,const dx12::InitialData* initial,Buffer** out){
    auto r=std::make_unique<Resource>();r->buffer=*d;r->desc=bufferDesc(d->ByteWidth);r->readback=d->Usage==dx12::Readback;
    r->dynamic=d->Usage==dx12::Dynamic||(d->BindFlags&dx12::Constant);
    if(d->BindFlags&dx12::Unordered)r->desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if(!r->dynamic){HRESULT hr=resource(r.get(),r->readback?D3D12_HEAP_TYPE_READBACK:D3D12_HEAP_TYPE_DEFAULT);if(FAILED(hr))return hr;}
    else{void* p=nullptr;r->allocation=upload(d->ByteWidth,r->native,r->offset,p);std::memset(p,0,d->ByteWidth);r->state=D3D12_RESOURCE_STATE_GENERIC_READ;}
    if(initial)context->UpdateSubresource(r.get(),0,nullptr,initial->pSysMem,0,0);*out=r.release();return S_OK;
}
HRESULT Device::CreateTexture2D(const dx12::TextureDesc* d,const dx12::InitialData* initial,Texture2D** out){
    auto r=std::make_unique<Resource>();r->texture=*d;r->readback=d->Usage==dx12::Readback;r->dynamic=d->Usage==dx12::Dynamic;
    auto& t=r->desc;t.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;t.Width=d->Width;t.Height=d->Height;t.DepthOrArraySize=UINT16(d->ArraySize);t.MipLevels=UINT16(d->MipLevels);t.Format=d->Format;t.SampleDesc=d->SampleDesc;
    if(d->BindFlags&dx12::RenderTarget)t.Flags|=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;if(d->BindFlags&dx12::DepthTarget)t.Flags|=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;if(d->BindFlags&dx12::Unordered)t.Flags|=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    HRESULT hr=resource(r.get(),r->readback?D3D12_HEAP_TYPE_READBACK:D3D12_HEAP_TYPE_DEFAULT);if(FAILED(hr))return hr;
    if(initial)for(UINT i=0;i<d->MipLevels*d->ArraySize;++i)context->UpdateSubresource(r.get(),i,nullptr,initial[i].pSysMem,initial[i].SysMemPitch,initial[i].SysMemSlicePitch);
    *out=r.release();if(uploadBytes>64*1024*1024)submit();return S_OK;
}
HRESULT Device::CreateTexture3D(const dx12::VolumeDesc* d,const dx12::InitialData* initial,Texture3D** out){
    auto r=std::make_unique<Resource>();auto& t=r->desc;t.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;t.Width=d->Width;t.Height=d->Height;t.DepthOrArraySize=UINT16(d->Depth);t.MipLevels=UINT16(d->MipLevels);t.Format=d->Format;t.SampleDesc.Count=1;
    HRESULT hr=resource(r.get(),D3D12_HEAP_TYPE_DEFAULT);if(FAILED(hr))return hr;
    if(initial)for(UINT i=0;i<d->MipLevels;++i)context->UpdateSubresource(r.get(),i,nullptr,initial[i].pSysMem,initial[i].SysMemPitch,initial[i].SysMemSlicePitch);*out=r.release();return S_OK;
}
HRESULT Device::CreateShaderResourceView(Resource* r,const D3D12_SHADER_RESOURCE_VIEW_DESC* given,View** out){
    D3D12_SHADER_RESOURCE_VIEW_DESC d{};if(given)d=*given;else{
        d.Format=r->desc.Format;
        if(r->desc.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER){d.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;d.Buffer.NumElements=r->buffer.ByteWidth/r->buffer.StructureByteStride;d.Buffer.StructureByteStride=r->buffer.StructureByteStride;}
        else if(r->desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE3D){d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;d.Texture3D.MipLevels=r->desc.MipLevels;}
        else{d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Texture2D.MipLevels=r->desc.MipLevels;}}
    textureTables.clear(); // CPU descriptor addresses can be recycled when views are replaced.
    auto s=d;s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    auto* v=view(this,r,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);v->srv=d;v->format=d.Format;native->CreateShaderResourceView(r->native.Get(),&s,v->handle);*out=v;return S_OK;
}
HRESULT Device::CreateRenderTargetView(Resource* r,const D3D12_RENDER_TARGET_VIEW_DESC*,View** out){auto* v=view(this,r,D3D12_DESCRIPTOR_HEAP_TYPE_RTV);native->CreateRenderTargetView(r->native.Get(),nullptr,v->handle);*out=v;return S_OK;}
HRESULT Device::CreateDepthStencilView(Resource* r,const D3D12_DEPTH_STENCIL_VIEW_DESC* d,View** out){
    D3D12_DEPTH_STENCIL_VIEW_DESC s{};s.Format=d->Format;
    if(d->ViewDimension==D3D12_DSV_DIMENSION_TEXTURE2DARRAY){s.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DARRAY;s.Texture2DArray={d->Texture2DArray.MipSlice,d->Texture2DArray.FirstArraySlice,d->Texture2DArray.ArraySize};}else{s.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;s.Texture2D.MipSlice=d->Texture2D.MipSlice;}
    auto* v=view(this,r,D3D12_DESCRIPTOR_HEAP_TYPE_DSV);v->format=s.Format;native->CreateDepthStencilView(r->native.Get(),&s,v->handle);*out=v;return S_OK;
}
HRESULT Device::CreateUnorderedAccessView(Resource* r,const D3D12_UNORDERED_ACCESS_VIEW_DESC* d,View** out){D3D12_UNORDERED_ACCESS_VIEW_DESC s{};s.Format=d->Format;s.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;s.Buffer.NumElements=d->Buffer.NumElements;s.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;auto* v=view(this,r,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);native->CreateUnorderedAccessView(r->native.Get(),nullptr,&s,v->handle);*out=v;return S_OK;}
HRESULT Device::CreateVertexShader(const void* data,SIZE_T size,void*,Shader** out){auto* s=new Shader;s->code.assign(static_cast<const unsigned char*>(data),static_cast<const unsigned char*>(data)+size);*out=s;return S_OK;}
HRESULT Device::CreateInputLayout(const D3D12_INPUT_ELEMENT_DESC* d,UINT count,const void*,SIZE_T,InputLayout** out){auto* l=new InputLayout;l->elements.resize(count);l->names.reserve(count);for(UINT i=0;i<count;++i)l->names.emplace_back(d[i].SemanticName);for(UINT i=0;i<count;++i)l->elements[i]={l->names[i].c_str(),d[i].SemanticIndex,d[i].Format,d[i].InputSlot,d[i].AlignedByteOffset,D3D12_INPUT_CLASSIFICATION(d[i].InputSlotClass),d[i].InstanceDataStepRate};*out=l;return S_OK;}
HRESULT Device::CreateRasterizerState(const D3D12_RASTERIZER_DESC* d,RasterizerState** out){auto* s=new RasterizerState;s->desc=*d;*out=s;return S_OK;}
HRESULT Device::CreateDepthStencilState(const D3D12_DEPTH_STENCIL_DESC* d,DepthStencilState** out){auto* s=new DepthStencilState;s->desc=*d;s->desc.FrontFace=s->desc.BackFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};*out=s;return S_OK;}
HRESULT Device::CreateBlendState(const D3D12_BLEND_DESC* d,BlendState** out){auto* s=new BlendState;s->desc=*d;for(auto& b:s->desc.RenderTarget){if(!b.SrcBlend)b.SrcBlend=D3D12_BLEND_ONE;if(!b.DestBlend)b.DestBlend=D3D12_BLEND_ZERO;if(!b.BlendOp)b.BlendOp=D3D12_BLEND_OP_ADD;if(!b.SrcBlendAlpha)b.SrcBlendAlpha=D3D12_BLEND_ONE;if(!b.DestBlendAlpha)b.DestBlendAlpha=D3D12_BLEND_ZERO;if(!b.BlendOpAlpha)b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_NOOP;}*out=s;return S_OK;}
HRESULT Device::CreateSamplerState(const D3D12_SAMPLER_DESC* d,SamplerState** out){samplerTables.clear();auto* s=new SamplerState;s->owner=this;s->handle=allocate(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);auto x=*d;x.MaxAnisotropy=std::max(1u,x.MaxAnisotropy);if(!x.ComparisonFunc)x.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;native->CreateSampler(&x,s->handle);*out=s;return S_OK;}
HRESULT Device::CreateQuery(const dx12::QueryDesc* d,Query** out){auto* q=new Query;q->disjoint=d->Query==dx12::TimestampFrequency;if(!q->disjoint)q->index=queryCount++;*out=q;return S_OK;}
HRESULT Device::backBuffer(Texture2D** out){auto r=std::make_unique<Resource>();HRESULT hr=swap->GetBuffer(swap->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&r->native));if(FAILED(hr))return hr;r->desc=r->native->GetDesc();r->texture.Width=UINT(r->desc.Width);r->texture.Height=r->desc.Height;r->texture.Format=r->desc.Format;r->texture.ArraySize=r->texture.MipLevels=r->texture.SampleDesc.Count=1;r->state=D3D12_RESOURCE_STATE_PRESENT;*out=r.release();return S_OK;}
HRESULT Device::present(UINT interval){submit(std::strstr(GetCommandLineA(),"--dx12-sync")!=nullptr);return swap->Present(interval,0);}
HRESULT Device::resize(UINT width,UINT height){submit();return swap->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);}
void Context::RSSetViewports(UINT,const D3D12_VIEWPORT* v){viewport={v->TopLeftX,v->TopLeftY,v->Width,v->Height,v->MinDepth,v->MaxDepth};}
void Context::OMSetRenderTargets(UINT n,View* const* v,View* d){
    bool changed=n!=targetCount||d!=depthTarget;for(UINT i=0;i<n&&!changed;++i)changed=v[i]!=targets[i];
    if(changed)endParallel();targetCount=n;targets.fill(nullptr);for(UINT i=0;i<n;++i)targets[i]=v[i];depthTarget=d;}
void Context::IASetVertexBuffers(UINT start,UINT n,Buffer* const* b,const UINT* s,const UINT* o){for(UINT i=0;i<n;++i){vertices[start+i]=b[i];strides[start+i]=s[i];offsets[start+i]=o[i];}}
void Context::VSSetConstantBuffers(UINT start,UINT n,Buffer* const* b){for(UINT i=0;i<n;++i)constants[start+i]=b[i];}
void Context::CSSetConstantBuffers(UINT start,UINT n,Buffer* const* b){for(UINT i=0;i<n;++i)computeConstants[start+i]=b[i];}
void Context::PSSetShaderResources(UINT start,UINT n,View* const* v){for(UINT i=0;i<n;++i)textures[start+i]=v[i];}
void Context::CSSetShaderResources(UINT start,UINT n,View* const* v){for(UINT i=0;i<n;++i)computeTextures[start+i]=v[i];}
void Context::CSSetUnorderedAccessViews(UINT start,UINT n,View* const* v,const UINT*){for(UINT i=0;i<n;++i)uavs[start+i]=v[i];}
void Context::PSSetSamplers(UINT start,UINT n,SamplerState* const* s){for(UINT i=0;i<n;++i)samplers[start+i]=s[i];}
void Context::ClearRenderTargetView(View* v,const float* c){endParallel();owner->transition(v->resource,D3D12_RESOURCE_STATE_RENDER_TARGET);owner->commands->ClearRenderTargetView(v->handle,c,0,nullptr);}
void Context::ClearDepthStencilView(View* v,UINT flags,float value,UINT8 stencil){endParallel();owner->transition(v->resource,D3D12_RESOURCE_STATE_DEPTH_WRITE);owner->commands->ClearDepthStencilView(v->handle,D3D12_CLEAR_FLAGS(flags),value,stencil,0,nullptr);}
HRESULT Context::Map(Resource* r,UINT sub,dx12::MapMode mode,UINT,dx12::MappedData* out){
    if(mode==dx12::MapRead)endParallel();
    *out={};if(mode==dx12::MapRead){owner->submit();void* ptr=nullptr;HRESULT hr=r->native->Map(0,nullptr,&ptr);if(FAILED(hr))return hr;out->pData=ptr;if(!r->footprints.empty()){auto& f=r->footprints[sub];out->pData=static_cast<unsigned char*>(ptr)+f.Offset;out->RowPitch=f.Footprint.RowPitch;out->DepthPitch=out->RowPitch*r->rows[sub];}return S_OK;}
    if(r->desc.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER){void* ptr=nullptr;r->allocation=owner->upload(r->buffer.ByteWidth,r->native,r->offset,ptr);r->dynamic=true;r->state=D3D12_RESOURCE_STATE_GENERIC_READ;out->pData=ptr;}
    else{r->pending.resize(size_t(r->desc.Width)*r->desc.Height*4);out->pData=r->pending.data();out->RowPitch=UINT(r->desc.Width)*4;out->DepthPitch=out->RowPitch*r->desc.Height;}return S_OK;
}
void Context::Unmap(Resource* r,UINT sub){if(r->readback)r->native->Unmap(0,nullptr);else if(!r->pending.empty()){UpdateSubresource(r,sub,nullptr,r->pending.data(),UINT(r->desc.Width)*4,0);r->pending.clear();}}
void Context::UpdateSubresource(Resource* r,UINT sub,const D3D12_BOX*,const void* data,UINT pitch,UINT slice){
    if(!r->dynamic||r->desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER)endParallel();
    if(r->desc.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER){
        if(r->dynamic){void* ptr=nullptr;r->allocation=owner->upload(r->buffer.ByteWidth,r->native,r->offset,ptr);std::memcpy(ptr,data,r->buffer.ByteWidth);return;}
        ComPtr<ID3D12Resource> upload;UINT64 off;void* ptr;owner->upload(r->buffer.ByteWidth,upload,off,ptr);std::memcpy(ptr,data,r->buffer.ByteWidth);owner->transition(r,D3D12_RESOURCE_STATE_COPY_DEST);owner->commands->CopyBufferRegion(r->native.Get(),0,upload.Get(),off,r->buffer.ByteWidth);return;
    }
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT f{};UINT rows;UINT64 rowBytes,total;owner->native->GetCopyableFootprints(&r->desc,sub,1,0,&f,&rows,&rowBytes,&total);
    ComPtr<ID3D12Resource> upload;UINT64 off;void* ptr;owner->upload(total,upload,off,ptr);f.Offset=off;
    if(!slice)slice=pitch*rows;
    for(UINT z=0;z<f.Footprint.Depth;++z)for(UINT y=0;y<rows;++y)std::memcpy(static_cast<unsigned char*>(ptr)+size_t(z)*rows*f.Footprint.RowPitch+size_t(y)*f.Footprint.RowPitch,static_cast<const unsigned char*>(data)+size_t(z)*slice+size_t(y)*pitch,size_t(rowBytes));
    owner->transition(r,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=f;dst.pResource=r->native.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=sub;owner->commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
}
void Context::CopyResource(Resource* dst,Resource* src){endParallel();owner->transition(src,D3D12_RESOURCE_STATE_COPY_SOURCE);owner->transition(dst,D3D12_RESOURCE_STATE_COPY_DEST);
    if(dst->readback&&!dst->footprints.empty()){for(UINT i=0;i<dst->footprints.size();++i){D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=dst->native.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=dst->footprints[i];b.pResource=src->native.Get();b.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;b.SubresourceIndex=i;owner->commands->CopyTextureRegion(&a,0,0,0,&b,nullptr);}}
    else owner->commands->CopyResource(dst->native.Get(),src->native.Get());
}
void Context::bind(bool compute){
    auto& d=*owner;
    if(d.gpuViewCount+18>262144)d.submit();
    auto* cmd=d.commands.Get();prepared={};
    if(compute&&!applied.heaps){ID3D12DescriptorHeap* heaps[]={d.gpuViews.Get(),d.gpuSamplers.Get()};cmd->SetDescriptorHeaps(2,heaps);applied.heaps=true;++bindStats.heapBinds;}
    if(compute&&!applied.roots[compute]){if(compute)cmd->SetComputeRootSignature(d.root.Get());else cmd->SetGraphicsRootSignature(d.root.Get());applied.roots[compute]=true;}
    auto& cb=compute?computeConstants:constants;
    for(UINT i=0;i<4;++i){auto* b=cb[i];if(b)d.keep(b);auto address=b?b->native->GetGPUVirtualAddress()+b->offset:d.zeroConstants->GetGPUVirtualAddress();prepared.cb[0][i]=address;if(compute&&applied.cb[compute][i]!=address){if(compute)cmd->SetComputeRootConstantBufferView(i,address);else cmd->SetGraphicsRootConstantBufferView(i,address);applied.cb[compute][i]=address;++bindStats.constantBinds;}}
    auto& tex=compute?computeTextures:textures;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE,16> sources{};
    for(UINT i=0;i<16;++i){
        sources[i]=tex[i]?tex[i]->handle:d.nullTexture;
        if(tex[i])d.transition(tex[i]->resource,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
    std::string textureKey(reinterpret_cast<const char*>(sources.data()),sizeof(sources));
    auto table=d.textureTables.find(textureKey);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    if(table!=d.textureTables.end())gpu=table->second;
    else{
        auto destination=d.gpuViews->GetCPUDescriptorHandleForHeapStart();destination.ptr+=SIZE_T(d.gpuViewCount)*d.viewStep;
        gpu=d.gpuViews->GetGPUDescriptorHandleForHeapStart();gpu.ptr+=UINT64(d.gpuViewCount)*d.viewStep;
        UINT count=16;
        d.native->CopyDescriptors(1,&destination,&count,16,sources.data(),nullptr,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        d.gpuViewCount+=16;d.textureTables.emplace(std::move(textureKey),gpu);
    }
    prepared.textures[0]=gpu.ptr;if(compute&&applied.textures[compute]!=gpu.ptr){if(compute)cmd->SetComputeRootDescriptorTable(4,gpu);else cmd->SetGraphicsRootDescriptorTable(4,gpu);applied.textures[compute]=gpu.ptr;}
    auto cpu=d.gpuViews->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=SIZE_T(d.gpuViewCount)*d.viewStep;
    gpu=d.gpuViews->GetGPUDescriptorHandleForHeapStart();gpu.ptr+=UINT64(d.gpuViewCount)*d.viewStep;
    if(compute){for(UINT i=0;i<2;++i){if(uavs[i]){d.transition(uavs[i]->resource,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);d.native->CopyDescriptorsSimple(1,cpu,uavs[i]->handle,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);}else{D3D12_UNORDERED_ACCESS_VIEW_DESC n{};n.Format=DXGI_FORMAT_R32_TYPELESS;n.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;n.Buffer.NumElements=1;n.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;d.native->CreateUnorderedAccessView(nullptr,nullptr,&n,cpu);}cpu.ptr+=d.viewStep;}d.gpuViewCount+=2;cmd->SetComputeRootDescriptorTable(6,gpu);
        D3D12_COMPUTE_PIPELINE_STATE_DESC p{};p.pRootSignature=d.root.Get();p.CS=bytecode(cs);std::string key(reinterpret_cast<const char*>(&p),sizeof(p));auto& pipeline=d.pipelines[key];if(!pipeline)check(d.native->CreateComputePipelineState(&p,IID_PPV_ARGS(&pipeline)));if(applied.pipeline!=pipeline.Get()){cmd->SetPipelineState(pipeline.Get());applied.pipeline=pipeline.Get();++bindStats.pipelineBinds;}return;
    }
    std::string samplerKey(reinterpret_cast<const char*>(samplers.data()),sizeof(samplers));auto found=d.samplerTables.find(samplerKey);D3D12_GPU_DESCRIPTOR_HANDLE sg{};
    if(found!=d.samplerTables.end())sg=found->second;else{
        if(d.gpuSamplerCount+4>2048)throw std::runtime_error("DX12 sampler table exhausted");auto sc=d.gpuSamplers->GetCPUDescriptorHandleForHeapStart();sc.ptr+=SIZE_T(d.gpuSamplerCount)*d.samplerStep;sg=d.gpuSamplers->GetGPUDescriptorHandleForHeapStart();sg.ptr+=UINT64(d.gpuSamplerCount)*d.samplerStep;
        for(auto* s:samplers){if(s)d.native->CopyDescriptorsSimple(1,sc,s->handle,D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);else{D3D12_SAMPLER_DESC n{};n.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;n.AddressU=n.AddressV=n.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;n.MaxLOD=D3D12_FLOAT32_MAX;n.MaxAnisotropy=1;n.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;d.native->CreateSampler(&n,sc);}sc.ptr+=d.samplerStep;}d.gpuSamplerCount+=4;d.samplerTables.emplace(samplerKey,sg);
    }prepared.samplers=sg.ptr;
    D3D12_CPU_DESCRIPTOR_HANDLE handles[8]{};
    std::array<UINT64,8> targetIdentities{};
    std::array<UINT64,18> state{};
    Ref* objects[]={vs,ps,hs,ds,layout,raster,depth,blend};
    for(unsigned i=0;i<8;++i)state[i]=objects[i]?objects[i]->identity:0;
    state[8]=targetCount;state[9]=depthTarget?depthTarget->format:DXGI_FORMAT_UNKNOWN;
    for(UINT i=0;i<targetCount;++i)if(targets[i]){d.transition(targets[i]->resource,D3D12_RESOURCE_STATE_RENDER_TARGET);handles[i]=targets[i]->handle;targetIdentities[i]=targets[i]->identity;state[10+i]=targets[i]->format;}
    if(depthTarget)d.transition(depthTarget->resource,D3D12_RESOURCE_STATE_DEPTH_WRITE);
    if(state!=lastPipelineKey||!lastPipeline){
        ++bindStats.pipelineLookups;
        std::string key(reinterpret_cast<const char*>(state.data()),sizeof(state));auto& pipeline=pipelineCache[key];
        if(!pipeline){
            D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=d.root.Get();p.VS=bytecode(vs);p.PS=bytecode(ps);p.HS=bytecode(hs);p.DS=bytecode(ds);p.SampleMask=UINT_MAX;p.SampleDesc.Count=1;
            p.RasterizerState={D3D12_FILL_MODE_SOLID,D3D12_CULL_MODE_BACK,FALSE,0,0,0,TRUE,FALSE,FALSE,0,D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF};if(raster)p.RasterizerState=raster->desc;
            p.DepthStencilState.DepthEnable=TRUE;p.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;p.DepthStencilState.FrontFace=p.DepthStencilState.BackFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};if(depth)p.DepthStencilState=depth->desc;
            for(auto& b:p.BlendState.RenderTarget){b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_NOOP;b.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;}if(blend)p.BlendState=blend->desc;
            if(layout)p.InputLayout={layout->elements.data(),UINT(layout->elements.size())};p.PrimitiveTopologyType=hs?D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH:D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=targetCount;

            for(UINT i=0;i<targetCount;++i)p.RTVFormats[i]=DXGI_FORMAT(state[10+i]);
            p.DSVFormat=DXGI_FORMAT(state[9]);if(!depthTarget){p.DepthStencilState.DepthEnable=FALSE;p.DepthStencilState.StencilEnable=FALSE;}
            check(d.native->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&pipeline)));
        }
        lastPipelineKey=state;lastPipeline=pipeline.Get();
    }
    prepared.pipeline=lastPipeline;
    prepared.targetCount=targetCount;prepared.targetIdentities=targetIdentities;
    std::copy(handles,handles+8,prepared.targets.begin());
    prepared.depth=depthTarget?depthTarget->handle:D3D12_CPU_DESCRIPTOR_HANDLE{};
    prepared.depthIdentity=depthTarget?depthTarget->identity:0;
    prepared.viewport=viewport;prepared.topology=topology;
    for(UINT i=0;i<8;++i)if(auto* buffer=vertices[i]){d.transition(buffer,D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        prepared.vertices[i]={buffer->native->GetGPUVirtualAddress()+buffer->offset+offsets[i],buffer->buffer.ByteWidth-offsets[i],strides[i]};}
    if(indices){d.transition(indices,D3D12_RESOURCE_STATE_INDEX_BUFFER);
        prepared.indices={indices->native->GetGPUVirtualAddress()+indices->offset+indexOffset,indices->buffer.ByteWidth-indexOffset,indexFormat};}
    if(recordPool){for(UINT i=0;i<targetCount;++i)d.leaseRecordingView(targets[i]);d.leaseRecordingView(depthTarget);}
    else emitGraphics(cmd,d.root.Get(),d.gpuViews.Get(),d.gpuSamplers.Get(),prepared,applied,bindStats);
}
void Context::DrawInstanced(UINT n,UINT instances,UINT start,UINT first){bind(false);
    if(recordPool)drawPackets.push_back({prepared,n,instances,start,first,0,false});else owner->commands->DrawInstanced(n,instances,start,first);}
void Context::DrawIndexedInstanced(UINT n,UINT instances,UINT start,INT base,UINT first){bind(false);
    if(recordPool)drawPackets.push_back({prepared,n,instances,start,first,base,true});else owner->commands->DrawIndexedInstanced(n,instances,start,base,first);}
void Context::Dispatch(UINT x,UINT y,UINT z){endParallel();bind(true);owner->commands->Dispatch(x,y,z);}
void Context::End(Query* q){endParallel();q->fence=owner->serial+1;if(!q->disjoint){owner->commands->EndQuery(owner->queryHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,q->index);owner->commands->ResolveQueryData(owner->queryHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,q->index,1,owner->queryReadback.Get(),UINT64(q->index)*8);}}
HRESULT Context::GetData(Query* q,void* out,UINT,UINT){if(owner->fence->GetCompletedValue()<q->fence)return S_FALSE;if(q->disjoint){auto* d=static_cast<dx12::TimestampInfo*>(out);d->Disjoint=FALSE;d->Frequency=owner->timestampFrequency;}else{void* p=nullptr;check(owner->queryReadback->Map(0,nullptr,&p));std::memcpy(out,static_cast<UINT64*>(p)+q->index,8);owner->queryReadback->Unmap(0,nullptr);}return S_OK;}
void Context::ClearState(){endParallel();constants.fill(nullptr);computeConstants.fill(nullptr);textures.fill(nullptr);computeTextures.fill(nullptr);uavs.fill(nullptr);samplers.fill(nullptr);vertices.fill(nullptr);targets.fill(nullptr);indices=nullptr;depthTarget=nullptr;targetCount=0;}
}
