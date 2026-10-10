#pragma once
#include <memory>
#include <utility>

namespace rendering {
// D3D11 COM objects and the native DX12 wrappers both release one retained
// reference through Release(). They differ in its return type, so ComPtr is
// not a common owner for both APIs.
template <class Resource> struct ReleaseGpuResource {
    void operator()(Resource* resource) const {
        if (resource)
            resource->Release();
    }
};

template <class Resource>
using OwnedGpuResource = std::unique_ptr<Resource, ReleaseGpuResource<Resource>>;

// Adopt the factory's output immediately, including a non-null output returned
// with failure or an exception after assigning an output.
template <class Resource, class Factory>
OwnedGpuResource<Resource> CreateGpuResource(Factory&& factory) {
    Resource* raw = nullptr;
    bool succeeded = false;
    try {
        succeeded = factory(&raw);
    } catch (...) {
        ReleaseGpuResource<Resource>{}(raw);
        throw;
    }
    OwnedGpuResource<Resource> result(raw);
    if (!succeeded)
        return {};
    return result;
}
} // namespace rendering
