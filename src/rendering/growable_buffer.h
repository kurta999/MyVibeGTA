#pragma once
#include "resource_owner.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace rendering {
// Owns a dynamic vertex-stream buffer and its committed capacity. Failed growth
// leaves both unchanged, so callers can draw the old data or retry allocation.
template <class Api> class GrowableBuffer final {
  public:
    GrowableBuffer(typename Api::Device& device, size_t stride, size_t spareElements,
                   size_t maximumElements = std::numeric_limits<std::uint32_t>::max())
        : m_device(device), m_stride(stride), m_spareElements(spareElements),
          m_maximumElements(
              stride ? std::min(maximumElements,
                                size_t(std::numeric_limits<std::uint32_t>::max()) / stride)
                     : 0) {}
    GrowableBuffer(const GrowableBuffer&) = delete;
    GrowableBuffer& operator=(const GrowableBuffer&) = delete;

    bool Ensure(size_t count) {
        if (count <= m_capacity)
            return true;
        if (count > m_maximumElements)
            return false;
        const size_t spare = std::min(m_spareElements, m_maximumElements);
        const size_t doubled = m_capacity <= (m_maximumElements - spare) / 2
                                   ? m_capacity * 2 + spare
                                   : m_maximumElements;
        const size_t capacity = std::max(count, doubled);
        auto replacement = Api::CreateDynamicVertexBuffer(m_device, capacity * m_stride);
        if (!replacement)
            return false;
        m_buffer = std::move(replacement);
        m_capacity = capacity;
        return true;
    }
    typename Api::Buffer* Get() const {
        return m_buffer.get();
    }
    size_t Capacity() const {
        return m_capacity;
    }
    void Clear() {
        m_buffer.reset();
        m_capacity = 0;
    }

  private:
    typename Api::Device& m_device;
    size_t m_stride;
    size_t m_spareElements;
    size_t m_maximumElements;
    size_t m_capacity = 0;
    OwnedGpuResource<typename Api::Buffer> m_buffer;
};
} // namespace rendering
