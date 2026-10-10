#pragma once
#include "resource_owner.h"
#include "texture_key.h"
#include "../dx11_texture_loading.h"
#include "../dx11_assets.h"
#include "../logging.h"
#include <dxgiformat.h>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace rendering {
struct TextureStatistics {
    std::uint64_t m_payloadBytes = 0, m_compressedBytes = 0;
    unsigned m_textureCount = 0, m_compressedCount = 0;
};

// Owns one image per (file, interpretation) key. Surfaces, mesh materials, and
// PBR channels borrow the same view; no renderer map owns an extra reference.
template <class Api> class TextureLibrary final {
  public:
    using View = typename Api::View;
    using Kind = dx11::texture::Kind;
    TextureLibrary(typename Api::Device& device, typename Api::Context& context,
                   bool validateUploads)
        : m_device(device), m_context(context), m_validateUploads(validateUploads) {}
    TextureLibrary(const TextureLibrary&) = delete;
    TextureLibrary& operator=(const TextureLibrary&) = delete;

    View* Find(const std::wstring& file, Kind kind = Kind::Color) const {
        const auto found = m_images.find(TextureKey(file, kind));
        return found == m_images.end() ? nullptr : found->second.get();
    }
    View* ForMesh(const dx11::Mesh& mesh) const {
        return Find(mesh.textureFile, mesh.alphaTest ? Kind::MaskedColor : Kind::Color);
    }
    bool IsBc5Normal(const std::wstring& file) const {
        auto* view = Find(file, Kind::Normal);
        return view && Api::ViewFormat(*view) == DXGI_FORMAT_BC5_UNORM;
    }

    bool Ensure(const std::wstring& file, Kind kind = Kind::Color) {
        if (file.empty() || Find(file, kind))
            return true;
        return EnsurePrepared(file, kind, dx11::texture::prepare({file, kind}));
    }
    bool EnsurePrepared(const std::wstring& file, Kind kind,
                        const dx11::texture::Prepared& prepared) {
        if (file.empty())
            return false;
        const auto key = TextureKey(file, kind);
        if (m_images.count(key))
            return true;
        auto view = Upload(prepared, kind);
        if (!view)
            return false;
        m_images.emplace(key, std::move(view));
        std::uint64_t bytes = 0;
        for (const auto& level : prepared.levels)
            bytes += level.pixels.size();
        m_statistics.m_payloadBytes += bytes;
        ++m_statistics.m_textureCount;
        if (prepared.format >= dx11::texture::Format::Bc7) {
            m_statistics.m_compressedBytes += bytes;
            ++m_statistics.m_compressedCount;
        }
        return true;
    }
    bool EnsureMesh(const dx11::Mesh& mesh) {
        if (mesh.materialRanges.empty())
            return Ensure(mesh.textureFile, mesh.alphaTest ? Kind::MaskedColor : Kind::Color);
        for (const auto& range : mesh.materialRanges) {
            if (!Ensure(range.baseFile, range.alphaTest ? Kind::MaskedColor : Kind::Color) ||
                !Ensure(range.normalFile, Kind::Normal) || !Ensure(range.ormFile, Kind::Linear) ||
                !Ensure(range.occlusionFile, Kind::Linear) || !Ensure(range.emissiveFile))
                return false;
        }
        return true;
    }
    const TextureStatistics& Statistics() const {
        return m_statistics;
    }
    void Clear() {
        m_images.clear();
        m_statistics = {};
    }

  private:
    static DXGI_FORMAT ImageFormat(dx11::texture::Format format, Kind kind) {
        const bool color = kind == Kind::Color || kind == Kind::MaskedColor;
        switch (format) {
        case dx11::texture::Format::Bgra8:
            return color ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
        case dx11::texture::Format::Rgba8:
            return color ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        case dx11::texture::Format::Bc7:
            return color ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
        case dx11::texture::Format::Bc5:
            return DXGI_FORMAT_BC5_UNORM;
        case dx11::texture::Format::Bc4:
            return DXGI_FORMAT_BC4_UNORM;
        }
        return DXGI_FORMAT_UNKNOWN;
    }

    OwnedGpuResource<View> Upload(const dx11::texture::Prepared& prepared, Kind kind) {
        const auto& levels = prepared.levels;
        if (!prepared.error.empty() || levels.empty())
            return {};
        std::vector<typename Api::InitialData> initial(levels.size());
        for (size_t index = 0; index < levels.size(); ++index) {
            const auto& level = levels[index];
            if (!level.width || !level.height)
                return {};
            initial[index].pSysMem = level.pixels.data();
            initial[index].SysMemPitch = dx11::texture::rowPitch(level.width, prepared.format);
            if (level.pixels.size() != size_t(initial[index].SysMemPitch) *
                                           dx11::texture::rowCount(level.height, prepared.format))
                return {};
        }
        const auto format = ImageFormat(prepared.format, kind);
        auto texture =
            Api::CreateTexture(m_device, levels[0].width, levels[0].height, format, initial);
        if (!texture)
            return {};
        auto view = Api::CreateView(m_device, *texture);
        if (!view)
            return {};
        if (m_validateUploads && prepared.format >= dx11::texture::Format::Bc7) {
            const bool valid = Validate(*texture, prepared, initial);
            char line[180]{};
            std::snprintf(line, sizeof(line),
                          "DDS GPU mip verification: %ux%u, format %u, %u mips: %s",
                          levels[0].width, levels[0].height, unsigned(format),
                          unsigned(levels.size()), valid ? "passed" : "FAILED");
            logging::write(line);
            if (!valid)
                return {};
        }
        return view;
    }

    bool Validate(typename Api::Texture& texture, const dx11::texture::Prepared& prepared,
                  const std::vector<typename Api::InitialData>& initial) {
        auto readback = Api::CreateReadback(m_device, texture);
        if (!readback)
            return false;
        m_context.CopyResource(readback.get(), &texture);
        for (unsigned index = 0; index < prepared.levels.size(); ++index) {
            typename Api::MappedData mapped{};
            if (!Api::MapRead(m_context, *readback, index, mapped))
                return false;
            const auto& level = prepared.levels[index];
            const auto pitch = initial[index].SysMemPitch;
            bool valid = true;
            for (unsigned row = 0; row < dx11::texture::rowCount(level.height, prepared.format);
                 ++row) {
                if (std::memcmp(static_cast<const unsigned char*>(mapped.pData) +
                                    size_t(row) * mapped.RowPitch,
                                level.pixels.data() + size_t(row) * pitch, pitch) != 0) {
                    valid = false;
                    break;
                }
            }
            m_context.Unmap(readback.get(), index);
            if (!valid)
                return false;
        }
        return true;
    }

    typename Api::Device& m_device;
    typename Api::Context& m_context;
    bool m_validateUploads;
    std::unordered_map<std::wstring, OwnedGpuResource<View>> m_images;
    TextureStatistics m_statistics;
};
} // namespace rendering
