#pragma once
#include "resource_owner.h"
#include "../dx11_assets.h"
#include <unordered_map>

namespace rendering {
enum class MeshBufferKind { Vertices, Indices };

// One cache entry owns the complete GPU representation of one mesh revision.
// Callers can only borrow buffers for a fully uploaded, current revision.
template <class Api> class MeshCache final {
  public:
    using Buffer = typename Api::Buffer;
    explicit MeshCache(typename Api::Device& device) : m_device(device) {}
    MeshCache(const MeshCache&) = delete;
    MeshCache& operator=(const MeshCache&) = delete;

    bool Ensure(const dx11::Mesh& mesh) {
        if (Find(mesh))
            return true;
        if (mesh.vertices.empty())
            return false;
        Entry replacement;
        replacement.m_revision = mesh.revision;
        replacement.m_vertices = Api::CreateMeshBuffer(m_device, mesh.vertices.data(),
                                                       mesh.vertices.size() * sizeof(dx11::Vertex),
                                                       MeshBufferKind::Vertices);
        if (!replacement.m_vertices)
            return false;
        if (!mesh.indices.empty()) {
            replacement.m_indices = Api::CreateMeshBuffer(
                m_device, mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t),
                MeshBufferKind::Indices);
            if (!replacement.m_indices)
                return false;
        }
        m_entries.insert_or_assign(&mesh, std::move(replacement));
        return true;
    }

    Buffer* VertexBuffer(const dx11::Mesh& mesh) const {
        const auto* entry = Find(mesh);
        return entry ? entry->m_vertices.get() : nullptr;
    }
    Buffer* IndexBuffer(const dx11::Mesh& mesh) const {
        const auto* entry = Find(mesh);
        return entry ? entry->m_indices.get() : nullptr;
    }
    void Clear() {
        m_entries.clear();
    }

  private:
    struct Entry {
        std::uint64_t m_revision = 0;
        OwnedGpuResource<Buffer> m_vertices;
        OwnedGpuResource<Buffer> m_indices;
    };
    const Entry* Find(const dx11::Mesh& mesh) const {
        const auto found = m_entries.find(&mesh);
        return found != m_entries.end() && found->second.m_revision == mesh.revision
                   ? &found->second
                   : nullptr;
    }
    typename Api::Device& m_device;
    std::unordered_map<const dx11::Mesh*, Entry> m_entries;
};
} // namespace rendering
