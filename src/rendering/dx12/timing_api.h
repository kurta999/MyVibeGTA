#pragma once
#include "../gpu_profiler.h"
#include "../../dx12_backend.h"

namespace rendering {
struct Dx12TimingApi {
    using Device = dx12::Device;
    using Context = dx12::Context;
    using Query = dx12::Query;
    using Frequency = dx12::TimestampInfo;
    static constexpr bool Extended = true;
    static constexpr std::size_t TimestampCount = 12;
    static bool CreateFrequency(Device& device, Query** output) {
        const dx12::QueryDesc description{dx12::TimestampFrequency, 0};
        return SUCCEEDED(device.CreateQuery(&description, output));
    }
    static bool CreateTimestamp(Device& device, Query** output) {
        const dx12::QueryDesc description{dx12::Timestamp, 0};
        return SUCCEEDED(device.CreateQuery(&description, output));
    }
    static bool Read(Context& context, Query* query, void* output, unsigned size) {
        return context.GetData(query, output, size, 0) == S_OK;
    }
};
} // namespace rendering
