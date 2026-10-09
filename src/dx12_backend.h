#pragma once
// Native DX12 renderer: owns resources, descriptor heaps, pipeline cache,
// command recording and queue/fence lifetime. No D3D11 runtime dependency.
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <unordered_set>

namespace dx12 {
using Microsoft::WRL::ComPtr;
enum Memory { Default, Immutable, Dynamic, Readback };
enum Binding : UINT { Vertex=1, Index=2, Constant=4, ShaderInput=8, RenderTarget=16, DepthTarget=32, Unordered=64 };
enum ResourceOption : UINT { Structured=1, Raw=2, Cube=4 };
enum CpuAccess : UINT { Read=1, Write=2 };
enum MapMode { MapRead, MapWrite };
enum QueryType { Timestamp, TimestampFrequency };
struct BufferDesc {UINT ByteWidth=0;Memory Usage=Default;UINT BindFlags=0,CPUAccessFlags=0,MiscFlags=0,StructureByteStride=0;};
struct TextureDesc {UINT Width=0,Height=0,MipLevels=1,ArraySize=1;DXGI_FORMAT Format=DXGI_FORMAT_UNKNOWN;DXGI_SAMPLE_DESC SampleDesc{1,0};Memory Usage=Default;UINT BindFlags=0,CPUAccessFlags=0,MiscFlags=0;};
struct VolumeDesc {UINT Width=0,Height=0,Depth=0,MipLevels=1;DXGI_FORMAT Format=DXGI_FORMAT_UNKNOWN;Memory Usage=Default;UINT BindFlags=0,CPUAccessFlags=0,MiscFlags=0;};
struct InitialData {const void* pSysMem=nullptr;UINT SysMemPitch=0,SysMemSlicePitch=0;};
struct MappedData {void* pData=nullptr;UINT RowPitch=0,DepthPitch=0;};
struct QueryDesc {QueryType Query=Timestamp;UINT MiscFlags=0;};
struct TimestampInfo {UINT64 Frequency=0;BOOL Disjoint=FALSE;};
struct Ref {
    unsigned refs=1;
    virtual ~Ref()=default;
    void AddRef(){++refs;}
    void Release(){if(!--refs)delete this;}
};
struct UploadPage {
    ComPtr<ID3D12Resource> native;
    unsigned char* cpu=nullptr;
    UINT64 size=0;
};
struct Resource:Ref {
    ComPtr<ID3D12Resource> native;
    std::shared_ptr<UploadPage> allocation;
    D3D12_RESOURCE_STATES state=D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_DESC desc{};
    dx12::BufferDesc buffer{};
    dx12::TextureDesc texture{};
    bool readback=false,dynamic=false;
    UINT64 offset=0;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
    std::vector<UINT> rows;
    std::vector<UINT64> rowBytes;
    std::vector<unsigned char> pending;
    void GetDesc(dx12::BufferDesc* d){*d=buffer;}
    void GetDesc(dx12::TextureDesc* d){*d=texture;}
};
using Buffer=Resource;using Texture2D=Resource;using Texture3D=Resource;
struct Device;
struct View:Ref {
    Device* owner=nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE heapType=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    Resource* resource=nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE handle{};
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    ~View();
    void GetDesc(D3D12_SHADER_RESOURCE_VIEW_DESC* d){*d=srv;}
};
using ShaderResourceView=View;using RenderTargetView=View;using DepthStencilView=View;using UnorderedAccessView=View;
struct Shader:Ref {std::vector<unsigned char> code;};
using VertexShader=Shader;using PixelShader=Shader;using ComputeShader=Shader;using HullShader=Shader;using DomainShader=Shader;
struct InputLayout:Ref {std::vector<D3D12_INPUT_ELEMENT_DESC> elements;std::vector<std::string> names;};
struct RasterizerState:Ref {D3D12_RASTERIZER_DESC desc{};};
struct DepthStencilState:Ref {D3D12_DEPTH_STENCIL_DESC desc{};};
struct BlendState:Ref {D3D12_BLEND_DESC desc{};};
struct SamplerState:Ref {Device* owner=nullptr;D3D12_CPU_DESCRIPTOR_HANDLE handle{};~SamplerState();};
struct Query:Ref {bool disjoint=false;UINT index=0;UINT64 fence=0;};
struct Device;
struct Context:Ref {
    Device* owner;
    explicit Context(Device* d):owner(d){}
    Shader *vs=nullptr,*ps=nullptr,*hs=nullptr,*ds=nullptr,*cs=nullptr;
    InputLayout* layout=nullptr;
    RasterizerState* raster=nullptr;DepthStencilState* depth=nullptr;BlendState* blend=nullptr;
    std::array<Buffer*,4> constants{},computeConstants{};
    std::array<View*,16> textures{},computeTextures{};
    std::array<View*,2> uavs{};
    std::array<SamplerState*,4> samplers{};
    std::array<Buffer*,8> vertices{};
    std::array<UINT,8> strides{},offsets{};
    Buffer* indices=nullptr;DXGI_FORMAT indexFormat=DXGI_FORMAT_R32_UINT;UINT indexOffset=0;
    std::array<View*,8> targets{};UINT targetCount=0;View* depthTarget=nullptr;
    D3D_PRIMITIVE_TOPOLOGY topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    D3D12_VIEWPORT viewport{};
    void VSSetShader(Shader* s,void*,UINT){vs=s;}void PSSetShader(Shader* s,void*,UINT){ps=s;}
    void HSSetShader(Shader* s,void*,UINT){hs=s;}void DSSetShader(Shader* s,void*,UINT){ds=s;}
    void CSSetShader(Shader* s,void*,UINT){cs=s;}
    void IASetInputLayout(InputLayout* s){layout=s;}
    void IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY t){topology=t;}
    void RSSetState(RasterizerState* s){raster=s;}
    void OMSetDepthStencilState(DepthStencilState* s,UINT){depth=s;}
    void OMSetBlendState(BlendState* s,const float*,UINT){blend=s;}
    void RSSetViewports(UINT,const D3D12_VIEWPORT* v);
    void OMSetRenderTargets(UINT n,View* const* v,View* d);
    void IASetVertexBuffers(UINT start,UINT n,Buffer* const* b,const UINT* stride,const UINT* offset);
    void IASetIndexBuffer(Buffer* b,DXGI_FORMAT f,UINT off){indices=b;indexFormat=f;indexOffset=off;}
    void VSSetConstantBuffers(UINT start,UINT n,Buffer* const* b);
    void PSSetConstantBuffers(UINT start,UINT n,Buffer* const* b){VSSetConstantBuffers(start,n,b);}
    void HSSetConstantBuffers(UINT start,UINT n,Buffer* const* b){VSSetConstantBuffers(start,n,b);}
    void DSSetConstantBuffers(UINT start,UINT n,Buffer* const* b){VSSetConstantBuffers(start,n,b);}
    void CSSetConstantBuffers(UINT start,UINT n,Buffer* const* b);
    void PSSetShaderResources(UINT start,UINT n,View* const* v);
    void CSSetShaderResources(UINT start,UINT n,View* const* v);
    void CSSetUnorderedAccessViews(UINT start,UINT n,View* const* v,const UINT*);
    void PSSetSamplers(UINT start,UINT n,SamplerState* const* s);
    void ClearRenderTargetView(View*,const float*);
    void ClearDepthStencilView(View*,UINT,float,UINT8);
    HRESULT Map(Resource*,UINT,dx12::MapMode,UINT,dx12::MappedData*);
    void Unmap(Resource*,UINT);
    void UpdateSubresource(Resource*,UINT,const D3D12_BOX*,const void*,UINT,UINT);
    void CopyResource(Resource*,Resource*);
    void Draw(UINT n,UINT start){DrawInstanced(n,1,start,0);}
    void DrawInstanced(UINT,UINT,UINT,UINT);
    void DrawIndexedInstanced(UINT,UINT,UINT,INT,UINT);
    void Dispatch(UINT,UINT,UINT);
    void Begin(Query*){}void End(Query*);
    HRESULT GetData(Query*,void*,UINT,UINT);
    void ClearState();
    void bind(bool compute);
};
struct Device:Ref {
    ComPtr<ID3D12Device> native;
    ComPtr<ID3D12InfoQueue> infoQueue;
    unsigned validationErrors=0;
    std::unordered_set<unsigned> reportedWarnings;
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain3> swap;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12Fence> fence;
    HANDLE eventHandle=nullptr;UINT64 serial=0;
    // Each recording slot owns everything the GPU may still read after Present.
    static constexpr UINT frameCount=2;
    struct Frame {
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12DescriptorHeap> views,samplers;
        std::vector<ComPtr<ID3D12Resource>> resources;
        std::vector<std::shared_ptr<UploadPage>> uploads;
        UINT64 fence=0;
    };
    std::array<Frame,frameCount> frames;
    UINT frameIndex=0;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12DescriptorHeap> views,rtvs,dsvs,samplers,gpuViews,gpuSamplers;
    UINT viewCount=0,rtvCount=0,dsvCount=0,samplerCount=0,gpuViewCount=0,gpuSamplerCount=0;
    UINT viewStep=0,rtvStep=0,dsvStep=0,samplerStep=0;
    std::array<std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>,4> freeDescriptors;
    ComPtr<ID3D12QueryHeap> queryHeap;ComPtr<ID3D12Resource> queryReadback;UINT queryCount=0;
    UINT64 timestampFrequency=0;
    std::vector<ComPtr<ID3D12Resource>> retained;
    std::unordered_set<ID3D12Resource*> retainedSet;
    std::vector<std::shared_ptr<UploadPage>> uploadPool,activeUploads;
    std::shared_ptr<UploadPage> uploadBlock;
    UINT64 uploadOffset=0,uploadBytes=0;
    ComPtr<ID3D12Resource> zeroConstants;
    std::unordered_map<std::string,ComPtr<ID3D12PipelineState>> pipelines;
    std::unordered_map<std::string,D3D12_GPU_DESCRIPTOR_HANDLE> samplerTables;
    std::unordered_map<std::string,D3D12_GPU_DESCRIPTOR_HANDLE> textureTables;
    D3D12_CPU_DESCRIPTOR_HANDLE nullTexture{};
    Context* context=nullptr;
    ~Device();
    HRESULT initialize(HWND,UINT,UINT);
    // Explicit readback, resize and teardown drain the queue; Present does not.
    void submit(bool wait=true);
    void waitFor(UINT64 value);
    void transition(Resource*,D3D12_RESOURCE_STATES);
    void keep(Resource* r){if(r&&r->native&&retainedSet.insert(r->native.Get()).second){retained.push_back(r->native);if(r->allocation)activeUploads.push_back(r->allocation);}}
    std::shared_ptr<UploadPage> upload(UINT64,ComPtr<ID3D12Resource>&,UINT64&,void*&);
    D3D12_CPU_DESCRIPTOR_HANDLE allocate(D3D12_DESCRIPTOR_HEAP_TYPE);
    HRESULT resource(Resource*,D3D12_HEAP_TYPE);
    HRESULT CreateBuffer(const dx12::BufferDesc*,const dx12::InitialData*,Buffer**);
    HRESULT CreateTexture2D(const dx12::TextureDesc*,const dx12::InitialData*,Texture2D**);
    HRESULT CreateTexture3D(const dx12::VolumeDesc*,const dx12::InitialData*,Texture3D**);
    HRESULT CreateShaderResourceView(Resource*,const D3D12_SHADER_RESOURCE_VIEW_DESC*,View**);
    HRESULT CreateRenderTargetView(Resource*,const D3D12_RENDER_TARGET_VIEW_DESC*,View**);
    HRESULT CreateDepthStencilView(Resource*,const D3D12_DEPTH_STENCIL_VIEW_DESC*,View**);
    HRESULT CreateUnorderedAccessView(Resource*,const D3D12_UNORDERED_ACCESS_VIEW_DESC*,View**);
    HRESULT CreateVertexShader(const void*,SIZE_T,void*,Shader**);
    HRESULT CreatePixelShader(const void* p,SIZE_T n,void* x,Shader** s){return CreateVertexShader(p,n,x,s);}
    HRESULT CreateComputeShader(const void* p,SIZE_T n,void* x,Shader** s){return CreateVertexShader(p,n,x,s);}
    HRESULT CreateHullShader(const void* p,SIZE_T n,void* x,Shader** s){return CreateVertexShader(p,n,x,s);}
    HRESULT CreateDomainShader(const void* p,SIZE_T n,void* x,Shader** s){return CreateVertexShader(p,n,x,s);}
    HRESULT CreateInputLayout(const D3D12_INPUT_ELEMENT_DESC*,UINT,const void*,SIZE_T,InputLayout**);
    HRESULT CreateRasterizerState(const D3D12_RASTERIZER_DESC*,RasterizerState**);
    HRESULT CreateDepthStencilState(const D3D12_DEPTH_STENCIL_DESC*,DepthStencilState**);
    HRESULT CreateBlendState(const D3D12_BLEND_DESC*,BlendState**);
    HRESULT CreateSamplerState(const D3D12_SAMPLER_DESC*,SamplerState**);
    HRESULT CreateQuery(const dx12::QueryDesc*,Query**);
    HRESULT GetDeviceRemovedReason(){return native->GetDeviceRemovedReason();}
    HRESULT backBuffer(Texture2D**);
    HRESULT present(UINT);
    HRESULT resize(UINT,UINT);
};
}
