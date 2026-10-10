#pragma once
#include "../mesh_cache.h"
#include <d3d11.h>
#include <limits>

namespace rendering {
struct Dx11ResourceApi {
    using Device = ID3D11Device;
    using Context = ID3D11DeviceContext;
    using Buffer = ID3D11Buffer;
    using Texture = ID3D11Texture2D;
    using View = ID3D11ShaderResourceView;
    using InitialData = D3D11_SUBRESOURCE_DATA;
    using MappedData = D3D11_MAPPED_SUBRESOURCE;

    static OwnedGpuResource<Buffer> CreateDynamicVertexBuffer(Device& device, size_t size) {
        if (!size || size > std::numeric_limits<UINT>::max())
            return {};
        D3D11_BUFFER_DESC description{};
        description.ByteWidth = UINT(size);
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return CreateGpuResource<Buffer>([&](Buffer** output) {
            return SUCCEEDED(device.CreateBuffer(&description, nullptr, output));
        });
    }

    static OwnedGpuResource<Buffer> CreateMeshBuffer(Device& device, const void* bytes, size_t size,
                                                     MeshBufferKind kind) {
        if (!size || size > std::numeric_limits<UINT>::max())
            return {};
        D3D11_BUFFER_DESC description{};
        description.ByteWidth = UINT(size);
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags =
            kind == MeshBufferKind::Vertices ? D3D11_BIND_VERTEX_BUFFER : D3D11_BIND_INDEX_BUFFER;
        InitialData initial{};
        initial.pSysMem = bytes;
        return CreateGpuResource<Buffer>([&](Buffer** output) {
            return SUCCEEDED(device.CreateBuffer(&description, &initial, output));
        });
    }
    static OwnedGpuResource<Texture> CreateTexture(Device& device, unsigned width, unsigned height,
                                                   DXGI_FORMAT format,
                                                   const std::vector<InitialData>& initial) {
        D3D11_TEXTURE2D_DESC description{};
        description.Width = width;
        description.Height = height;
        description.MipLevels = UINT(initial.size());
        description.ArraySize = 1;
        description.Format = format;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        return CreateGpuResource<Texture>([&](Texture** output) {
            return SUCCEEDED(device.CreateTexture2D(&description, initial.data(), output));
        });
    }
    static OwnedGpuResource<Texture> CreateReadback(Device& device, Texture& texture) {
        D3D11_TEXTURE2D_DESC description{};
        texture.GetDesc(&description);
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
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
        return SUCCEEDED(context.Map(&texture, index, D3D11_MAP_READ, 0, &output));
    }
    static DXGI_FORMAT ViewFormat(View& view) {
        D3D11_SHADER_RESOURCE_VIEW_DESC description{};
        view.GetDesc(&description);
        return description.Format;
    }
};
} // namespace rendering
