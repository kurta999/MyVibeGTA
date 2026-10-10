#pragma once
#include "../mesh_cache.h"
#include "../../dx12_backend.h"
#include <limits>

namespace rendering {
struct Dx12ResourceApi {
    using Device = dx12::Device;
    using Context = dx12::Context;
    using Buffer = dx12::Buffer;
    using Texture = dx12::Texture2D;
    using View = dx12::ShaderResourceView;
    using InitialData = dx12::InitialData;
    using MappedData = dx12::MappedData;

    static OwnedGpuResource<Buffer> CreateDynamicVertexBuffer(Device& device, size_t size) {
        if (!size || size > std::numeric_limits<UINT>::max())
            return {};
        dx12::BufferDesc description{};
        description.ByteWidth = UINT(size);
        description.Usage = dx12::Dynamic;
        description.BindFlags = dx12::Vertex;
        description.CPUAccessFlags = dx12::Write;
        return CreateGpuResource<Buffer>([&](Buffer** output) {
            return SUCCEEDED(device.CreateBuffer(&description, nullptr, output));
        });
    }

    static OwnedGpuResource<Buffer> CreateMeshBuffer(Device& device, const void* bytes, size_t size,
                                                     MeshBufferKind kind) {
        if (!size || size > std::numeric_limits<UINT>::max())
            return {};
        dx12::BufferDesc description{};
        description.ByteWidth = UINT(size);
        description.Usage = dx12::Immutable;
        description.BindFlags = kind == MeshBufferKind::Vertices ? dx12::Vertex : dx12::Index;
        InitialData initial{};
        initial.pSysMem = bytes;
        return CreateGpuResource<Buffer>([&](Buffer** output) {
            return SUCCEEDED(device.CreateBuffer(&description, &initial, output));
        });
    }
    static OwnedGpuResource<Texture> CreateTexture(Device& device, unsigned width, unsigned height,
                                                   DXGI_FORMAT format,
                                                   const std::vector<InitialData>& initial) {
        dx12::TextureDesc description{};
        description.Width = width;
        description.Height = height;
        description.MipLevels = UINT(initial.size());
        description.ArraySize = 1;
        description.Format = format;
        description.SampleDesc.Count = 1;
        description.Usage = dx12::Immutable;
        description.BindFlags = dx12::ShaderInput;
        return CreateGpuResource<Texture>([&](Texture** output) {
            return SUCCEEDED(device.CreateTexture2D(&description, initial.data(), output));
        });
    }
    static OwnedGpuResource<Texture> CreateReadback(Device& device, Texture& texture) {
        dx12::TextureDesc description{};
        texture.GetDesc(&description);
        description.Usage = dx12::Readback;
        description.BindFlags = 0;
        description.CPUAccessFlags = dx12::Read;
        return CreateGpuResource<Texture>([&](Texture** output) {
            return SUCCEEDED(device.CreateTexture2D(&description, nullptr, output));
        });
    }
    static OwnedGpuResource<View> CreateView(Device& device, Texture& texture) {
        return CreateGpuResource<View>([&](View** output) {
            return SUCCEEDED(device.CreateShaderResourceView(&texture, nullptr, output));
        });
    }
    static bool MapRead(Context& context, Texture& texture, unsigned index, MappedData& output) {
        return SUCCEEDED(context.Map(&texture, index, dx12::MapRead, 0, &output));
    }
    static DXGI_FORMAT ViewFormat(View& view) {
        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        view.GetDesc(&description);
        return description.Format;
    }
};
} // namespace rendering
