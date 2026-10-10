#include "../src/rendering/mesh_cache.h"
#include "../src/rendering/growable_buffer.h"
#include "../src/rendering/texture_library.h"
#include <cstdio>
#include <stdexcept>
#include <type_traits>

namespace {
void Require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

enum class Failure { None, Vertex, Index, Dynamic, Texture, View, Readback, Map };

// Exercise both COM's counted Release and the native DX12 wrapper's void Release.
template <bool Counted> struct FixtureApi {
    struct Resource {
        inline static int m_live = 0;
        unsigned m_references = 1;
        Resource() {
            ++m_live;
        }
        virtual ~Resource() {
            --m_live;
        }
        void AddRef() {
            ++m_references;
        }
        auto Release() {
            const unsigned remaining = --m_references;
            if (!remaining)
                delete this;
            if constexpr (Counted)
                return remaining;
        }
    };
    struct Buffer : Resource {};
    struct Texture : Resource {
        DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
        unsigned m_width = 0, m_height = 0;
        std::vector<std::vector<unsigned char>> m_levels;
        std::vector<unsigned> m_pitches;
    };
    struct View : Resource {
        Texture* m_texture;
        explicit View(Texture& texture) : m_texture(&texture) {
            texture.AddRef();
        }
        ~View() override {
            m_texture->Release();
        }
    };
    struct InitialData {
        const void* pSysMem = nullptr;
        unsigned SysMemPitch = 0;
    };
    struct MappedData {
        void* pData = nullptr;
        unsigned RowPitch = 0;
    };
    struct Device {
        Failure m_failure = Failure::None;
        unsigned m_allocations = 0;
        size_t m_lastBytes = 0;
    };
    struct Context {
        Failure m_failure = Failure::None;
        unsigned m_maps = 0, m_unmaps = 0;
        int m_failMip = -1;
        bool m_corrupt = false;
        void CopyResource(Texture* destination, Texture* source) {
            unsigned height = source->m_height;
            for (size_t mip = 0; mip < source->m_levels.size(); ++mip) {
                const unsigned rows = Rows(height, source->m_format);
                const unsigned pitch = source->m_pitches[mip], paddedPitch = pitch + 16;
                destination->m_levels.emplace_back(rows * paddedPitch, 0xEE);
                destination->m_pitches.push_back(paddedPitch);
                for (unsigned row = 0; row < rows; ++row)
                    std::memcpy(destination->m_levels.back().data() + row * paddedPitch,
                                source->m_levels[mip].data() + row * pitch, pitch);
                height = std::max(1u, height / 2);
            }
            if (m_corrupt)
                destination->m_levels.back()[0] ^= 1;
        }
        void Unmap(Texture*, unsigned) {
            ++m_unmaps;
        }
    };
    static unsigned Rows(unsigned height, DXGI_FORMAT format) {
        return format == DXGI_FORMAT_BC5_UNORM ? (height + 3) / 4 : height;
    }
    static rendering::OwnedGpuResource<Buffer> CreateDynamicVertexBuffer(Device& device,
                                                                         size_t size) {
        ++device.m_allocations;
        device.m_lastBytes = size;
        if (device.m_failure == Failure::Dynamic)
            return {};
        return rendering::OwnedGpuResource<Buffer>(new Buffer);
    }
    static rendering::OwnedGpuResource<Buffer> CreateMeshBuffer(Device& device, const void*, size_t,
                                                                rendering::MeshBufferKind kind) {
        ++device.m_allocations;
        if (device.m_failure ==
            (kind == rendering::MeshBufferKind::Vertices ? Failure::Vertex : Failure::Index))
            return {};
        return rendering::OwnedGpuResource<Buffer>(new Buffer);
    }
    static rendering::OwnedGpuResource<Texture>
    CreateTexture(Device& device, unsigned width, unsigned height, DXGI_FORMAT format,
                  const std::vector<InitialData>& initial) {
        ++device.m_allocations;
        if (device.m_failure == Failure::Texture)
            return {};
        rendering::OwnedGpuResource<Texture> texture(new Texture);
        texture->m_format = format;
        texture->m_width = width;
        texture->m_height = height;
        for (const auto& mip : initial) {
            const auto* bytes = static_cast<const unsigned char*>(mip.pSysMem);
            texture->m_levels.emplace_back(bytes, bytes + mip.SysMemPitch * Rows(height, format));
            texture->m_pitches.push_back(mip.SysMemPitch);
            height = std::max(1u, height / 2);
        }
        return texture;
    }
    static rendering::OwnedGpuResource<View> CreateView(Device& device, Texture& texture) {
        ++device.m_allocations;
        if (device.m_failure == Failure::View)
            return {};
        return rendering::OwnedGpuResource<View>(new View(texture));
    }
    static rendering::OwnedGpuResource<Texture> CreateReadback(Device& device, Texture& texture) {
        ++device.m_allocations;
        if (device.m_failure == Failure::Readback)
            return {};
        rendering::OwnedGpuResource<Texture> result(new Texture);
        result->m_height = texture.m_height;
        result->m_format = texture.m_format;
        return result;
    }
    static bool MapRead(Context& context, Texture& texture, unsigned mip, MappedData& output) {
        if (context.m_failure == Failure::Map && int(mip) == context.m_failMip)
            return false;
        ++context.m_maps;
        output.pData = texture.m_levels[mip].data();
        output.RowPitch = texture.m_pitches[mip];
        return true;
    }
    static DXGI_FORMAT ViewFormat(View& view) {
        return view.m_texture->m_format;
    }
};

dx11::texture::Prepared Image(dx11::texture::Format format = dx11::texture::Format::Rgba8) {
    dx11::texture::Prepared image;
    image.format = format;
    for (unsigned size : {8u, 4u, 2u, 1u}) {
        dx11::texture::Level level;
        level.width = level.height = size;
        level.pixels.assign(
            dx11::texture::rowPitch(size, format) * dx11::texture::rowCount(size, format), 0x57);
        image.levels.push_back(std::move(level));
    }
    return image;
}

template <bool Counted> void MeshScenarios() {
    using Api = FixtureApi<Counted>;
    typename Api::Device device;
    dx11::Mesh mesh;
    mesh.vertices.resize(3);
    mesh.indices = {0, 1, 2};
    rendering::MeshCache<Api> cache(device);
    Require(cache.Ensure(mesh), "initial mesh upload");
    auto* original = cache.VertexBuffer(mesh);
    Require(cache.IndexBuffer(mesh) && Api::Resource::m_live == 2, "cache owns both buffers");
    Require(cache.Ensure(mesh) && device.m_allocations == 2, "same revision reuses buffers");
    for (Failure failure : {Failure::Vertex, Failure::Index}) {
        ++mesh.revision;
        device.m_failure = failure;
        Require(!cache.Ensure(mesh), "failed revision upload must fail");
        Require(!cache.VertexBuffer(mesh) && !cache.IndexBuffer(mesh),
                "stale buffers cannot be drawn");
        Require(Api::Resource::m_live == 2, "temporary replacement must be released");
    }
    device.m_failure = Failure::None;
    Require(cache.Ensure(mesh) && cache.VertexBuffer(mesh) != original,
            "retry publishes replacement");
    Require(Api::Resource::m_live == 2, "superseded buffers released");
    mesh.indices.clear();
    ++mesh.revision;
    Require(cache.Ensure(mesh) && !cache.IndexBuffer(mesh), "indexed to nonindexed replacement");
    mesh.vertices.clear();
    ++mesh.revision;
    Require(!cache.Ensure(mesh) && !cache.VertexBuffer(mesh),
            "empty revision cannot draw stale geometry");
    cache.Clear();
    cache.Clear();
    Require(Api::Resource::m_live == 0, "idempotent mesh clear");
    mesh.vertices.resize(3);
    Require(cache.Ensure(mesh), "cache can be reused after clear");
}

template <bool Counted> void TextureScenarios() {
    using Api = FixtureApi<Counted>;
    using Kind = dx11::texture::Kind;
    typename Api::Device device;
    typename Api::Context context;
    auto color = Image();
    auto compressed = Image(dx11::texture::Format::Bc5);
    rendering::TextureLibrary<Api> library(device, context, true);
    Require(library.Ensure(L""), "optional empty channels succeed");
    Require(!library.EnsurePrepared(L"", Kind::Color, color), "unnamed prepared uploads rejected");
    Require(library.EnsurePrepared(L"surface", Kind::Color, color), "color upload");
    auto* shared = library.Find(L"surface");
    dx11::Mesh mesh;
    mesh.textureFile = L"surface";
    Require(library.EnsureMesh(mesh) && library.ForMesh(mesh) == shared,
            "mesh borrows surface image");
    const auto allocations = device.m_allocations;
    Require(library.EnsurePrepared(L"surface", Kind::Color, {}),
            "cache hit avoids invalid duplicate data");
    Require(device.m_allocations == allocations && library.Statistics().m_textureCount == 1,
            "upload and stats deduplicated");
    Require(Api::ViewFormat(*shared) == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, "color is sRGB");
    Require(library.EnsurePrepared(L"surface", Kind::Linear, color),
            "distinct interpretation upload");
    Require(Api::ViewFormat(*library.Find(L"surface", Kind::Linear)) == DXGI_FORMAT_R8G8B8A8_UNORM,
            "linear channel stays linear");
    Require(library.Find(L"surface", Kind::Linear) != shared, "interpretations do not alias");
    mesh.alphaTest = true;
    Require(library.EnsurePrepared(L"surface", Kind::MaskedColor, color) &&
                library.ForMesh(mesh) != shared,
            "masked interpretation distinct");
    Require(library.EnsurePrepared(L"normal", Kind::Normal, compressed),
            "compressed normal validated");
    Require(library.IsBc5Normal(L"normal") && !library.IsBc5Normal(L"missing"),
            "BC5 normal lookup");
    Require(context.m_maps == 4 && context.m_unmaps == 4, "all GPU mips validated and unmapped");
    Require(library.Statistics().m_compressedCount == 1,
            "successful compressed upload counted once");
    library.Clear();
    library.Clear();
    Require(Api::Resource::m_live == 0 && !library.Statistics().m_textureCount &&
                !library.Statistics().m_payloadBytes,
            "clear destroys views and retained textures");

    for (Failure failure : {Failure::Texture, Failure::View, Failure::Readback, Failure::Map}) {
        device.m_failure = failure;
        context.m_failure = failure;
        context.m_failMip = 2;
        Require(!library.EnsurePrepared(L"failed", Kind::Normal, compressed),
                "partial upload fails");
        Require(!library.Find(L"failed", Kind::Normal) && Api::Resource::m_live == 0,
                "failed upload publishes nothing and leaks nothing");
        Require(context.m_maps == context.m_unmaps, "only successful maps must be unmapped");
        Require(!library.Statistics().m_textureCount, "failed uploads excluded from stats");
    }
    device.m_failure = context.m_failure = Failure::None;
    context.m_corrupt = true;
    Require(!library.EnsurePrepared(L"corrupt", Kind::Normal, compressed),
            "GPU byte mismatch rejected");
    Require(context.m_maps == context.m_unmaps && Api::Resource::m_live == 0, "mismatch cleanup");
    context.m_corrupt = false;
    Require(library.EnsurePrepared(L"failed", Kind::Normal, compressed),
            "retry after failure works");
    library.Clear();
    const auto beforeInvalid = device.m_allocations;
    auto invalid = color;
    invalid.error = "decode failure";
    Require(!library.EnsurePrepared(L"decode", Kind::Color, invalid), "decode errors rejected");
    invalid = color;
    invalid.levels[0].pixels.pop_back();
    Require(!library.EnsurePrepared(L"size", Kind::Color, invalid),
            "malformed payload rejected before native upload");
    invalid = color;
    invalid.levels[0].width = 0;
    Require(!library.EnsurePrepared(L"empty", Kind::Color, invalid), "zero dimensions rejected");
    Require(device.m_allocations == beforeInvalid, "invalid CPU data never reaches device");
}

template <bool Counted> void OwnerScenario() {
    using Resource = typename FixtureApi<Counted>::Buffer;
    auto failed = rendering::CreateGpuResource<Resource>([](Resource** output) {
        *output = new Resource;
        return false;
    });
    Require(!failed && FixtureApi<Counted>::Resource::m_live == 0,
            "failed factory output still adopted and released");
    bool threw = false;
    try {
        rendering::CreateGpuResource<Resource>([](Resource** output) -> bool {
            *output = new Resource;
            throw std::runtime_error("factory exception");
        });
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && FixtureApi<Counted>::Resource::m_live == 0,
            "factory exception releases assigned output");
}

template <bool Counted> void GrowthScenarios() {
    using Api = FixtureApi<Counted>;
    typename Api::Device device;
    rendering::GrowableBuffer<Api> buffer(device, 16, 4, 20);
    Require(buffer.Ensure(0) && !buffer.Get() && !device.m_allocations,
            "empty stream needs no allocation");
    Require(buffer.Ensure(1) && buffer.Capacity() == 4 && device.m_lastBytes == 64,
            "initial spare capacity");
    auto* original = buffer.Get();
    device.m_failure = Failure::Dynamic;
    Require(!buffer.Ensure(5) && buffer.Get() == original && buffer.Capacity() == 4,
            "failed growth preserves owner and capacity");
    Require(buffer.Ensure(4), "existing allocation still usable after failure");
    device.m_failure = Failure::None;
    Require(buffer.Ensure(5) && buffer.Capacity() == 12 && buffer.Get() != original,
            "growth retry succeeds");
    Require(Api::Resource::m_live == 1, "superseded dynamic buffer released");
    Require(buffer.Ensure(20) && buffer.Capacity() == 20, "growth clamps to maximum capacity");
    original = buffer.Get();
    const auto allocations = device.m_allocations;
    Require(!buffer.Ensure(21) && buffer.Get() == original && device.m_allocations == allocations,
            "over limit rejected without allocation");
    buffer.Clear();
    buffer.Clear();
    Require(!buffer.Get() && !buffer.Capacity() && Api::Resource::m_live == 0,
            "dynamic clear resets capacity with owner");
    Require(buffer.Ensure(3), "dynamic owner reusable after clear");
    rendering::GrowableBuffer<Api> oversized(device, size_t(UINT32_MAX) / 2 + 1, 100);
    Require(oversized.Ensure(1) && !oversized.Ensure(2), "native byte width cannot overflow");
    rendering::GrowableBuffer<Api> invalid(device, 0, 4);
    Require(!invalid.Ensure(1), "zero stride cannot allocate");
}
} // namespace

int main() {
    try {
        OwnerScenario<true>();
        OwnerScenario<false>();
        GrowthScenarios<true>();
        GrowthScenarios<false>();
        Require(FixtureApi<true>::Resource::m_live == 0 && FixtureApi<false>::Resource::m_live == 0,
                "dynamic destructor cleanup");
        MeshScenarios<true>();
        MeshScenarios<false>();
        Require(FixtureApi<true>::Resource::m_live == 0 && FixtureApi<false>::Resource::m_live == 0,
                "mesh destructor cleanup");
        TextureScenarios<true>();
        TextureScenarios<false>();
        Require(FixtureApi<true>::Resource::m_live == 0 && FixtureApi<false>::Resource::m_live == 0,
                "texture destructor cleanup");
        std::puts("PASS resource ownership, mesh revision transactions, texture sharing, "
                  "interpretation, mip validation, failures and retry for both Release contracts");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s\n", error.what());
        return 1;
    }
}
