#pragma once
#include "../gpu_profiler.h"
#include <d3d11.h>

namespace rendering {
struct Dx11TimingApi {
    using Device = ID3D11Device;
    using Context = ID3D11DeviceContext;
    using Query = ID3D11Query;
    using Frequency = D3D11_QUERY_DATA_TIMESTAMP_DISJOINT;
    static constexpr bool Extended = false;
    static constexpr std::size_t TimestampCount = 4;
    static bool CreateFrequency(Device& device, Query** output) {
        const D3D11_QUERY_DESC description{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        return SUCCEEDED(device.CreateQuery(&description, output));
    }
    static bool CreateTimestamp(Device& device, Query** output) {
        const D3D11_QUERY_DESC description{D3D11_QUERY_TIMESTAMP, 0};
        return SUCCEEDED(device.CreateQuery(&description, output));
    }
    static bool Read(Context& context, Query* query, void* output, unsigned size) {
        return context.GetData(query, output, size, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
    }
};
} // namespace rendering
