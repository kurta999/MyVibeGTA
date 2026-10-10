#include "../src/rendering/gpu_profiler.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Query {
    inline static int m_liveCount = 0;
    unsigned m_references = 1;
    int m_id = 0;
    bool m_frequency = false;
    std::uint64_t m_timestamp = 0;
    Query() { ++m_liveCount; }
    ~Query() { --m_liveCount; }
    unsigned AddRef() { return ++m_references; }
    unsigned Release() {
        const auto remaining = --m_references;
        if (!remaining) delete this;
        return remaining;
    }
};

struct Device {
    int m_created = 0;
    int m_failAt = -1;
    bool Create(bool frequency, Query** output) {
        if (m_created == m_failAt) return false;
        auto* query = new Query;
        query->m_id = m_created++;
        query->m_frequency = frequency;
        *output = query;
        return true;
    }
};

// Mirrors the external Direct3D frequency-result layout used by the adapters.
struct FrequencyData {
    std::uint64_t Frequency = 1000;
    bool Disjoint = false;
};

struct Context {
    std::uint64_t m_clock = 0;
    unsigned m_begins = 0, m_ends = 0;
    bool m_ready = true;
    int m_unreadyQuery = -1;
    FrequencyData m_frequency;
    void Begin(Query*) { ++m_begins; }
    void End(Query* query) {
        ++m_ends;
        if (!query->m_frequency) query->m_timestamp = ++m_clock;
    }
};

template <bool ExtendedTiming> struct TimingApi {
    using Device = ::Device;
    using Context = ::Context;
    using Query = ::Query;
    using Frequency = FrequencyData;
    static constexpr bool Extended = ExtendedTiming;
    static constexpr std::size_t TimestampCount = Extended ? 12 : 4;
    static bool CreateFrequency(Device& device, Query** output) { return device.Create(true, output); }
    static bool CreateTimestamp(Device& device, Query** output) { return device.Create(false, output); }
    static bool Read(Context& context, Query* query, void* output, unsigned size) {
        if (!context.m_ready || query->m_id == context.m_unreadyQuery) return false;
        if (query->m_frequency) {
            Require(size == sizeof(context.m_frequency), "Wrong frequency read size");
            std::memcpy(output, &context.m_frequency, size);
        } else {
            Require(size == sizeof(query->m_timestamp), "Wrong timestamp read size");
            std::memcpy(output, &query->m_timestamp, size);
        }
        return true;
    }
};

template <bool Extended>
void Record(rendering::GpuProfiler<TimingApi<Extended>>& profiler, Context& context) {
    using rendering::GpuStage;
    profiler.Begin(context);
    if constexpr (Extended) {
        profiler.Mark(context, GpuStage::SkinEnd);
        profiler.Mark(context, GpuStage::SceneBegin);
    }
    profiler.Mark(context, GpuStage::ShadowEnd);
    profiler.Mark(context, GpuStage::SceneEnd);
    if constexpr (Extended) {
        for (auto stage : {GpuStage::BloomEnd, GpuStage::ReflectionsEnd, GpuStage::CloudsEnd,
                           GpuStage::CompositionEnd, GpuStage::ToneEnd, GpuStage::TemporalEnd})
            profiler.Mark(context, stage);
    }
    profiler.End(context);
}

template <bool Extended> void RunScenarios() {
    using Profiler = rendering::GpuProfiler<TimingApi<Extended>>;
    constexpr int queryCount = 8 * (TimingApi<Extended>::TimestampCount + 1);
    for (int failure = 0; failure < queryCount; ++failure) {
        Device device;
        device.m_failAt = failure;
        Profiler profiler;
        Require(!profiler.Initialize(device), "Injected creation failure was ignored");
        Require(Query::m_liveCount == 0, "Partial initialization leaked queries");
    }
    {
        Device device;
        Context context;
        Profiler profiler;
        Require(profiler.Initialize(device), "Initialization failed");
        Require(Query::m_liveCount == queryCount, "Wrong query count");
        float shadow = -1, scene = -2, post = -3;
        for (int i = 0; i < 8; ++i) Record(profiler, context);
        const auto ends = context.m_ends;
        Record(profiler, context);
        Require(context.m_begins == 8 && context.m_ends == ends, "Busy query slot was overwritten");
        context.m_ready = false;
        profiler.Poll(context, shadow, scene, post);
        Require(shadow == -1 && scene == -2 && post == -3, "Unavailable results changed timings");
        context.m_ready = true;
        profiler.Poll(context, shadow, scene, post);
        Require(shadow == 1 && scene == 1 && post == (Extended ? 7 : 1), "Stage durations changed");
        if constexpr (Extended) {
            const auto& stats = profiler.Statistics();
            Require(stats.m_skinSamples == 8 && stats.m_skinMilliseconds == 8, "Skin timing changed");
            for (double value : stats.m_postMilliseconds) Require(value == 8, "Post timing changed");
            profiler.Poll(context, shadow, scene, post);
            Require(stats.m_postSamples == 8, "Consumed results were counted twice");
        }
        Record(profiler, context);
        Require(context.m_begins == 9, "Completed query slots were not reused");
        profiler.Reset();
        profiler.Reset();
        Require(Query::m_liveCount == 0, "Reset leaked queries");
        device = {};
        Require(profiler.Initialize(device), "Reinitialization failed");
        Require(profiler.Statistics().m_skinSamples == 0, "Statistics survived reinitialization");
        Record(profiler, context);
        shadow = -1;
        context.m_unreadyQuery = 2;
        profiler.Poll(context, shadow, scene, post);
        Require(shadow == -1, "Partial results were consumed");
        context.m_unreadyQuery = -1;
        context.m_frequency.Disjoint = true;
        profiler.Poll(context, shadow, scene, post);
        Require(shadow == -1, "Disjoint results were accepted");
        context.m_frequency.Disjoint = false;
        context.m_frequency.Frequency = 0;
        Record(profiler, context);
        profiler.Poll(context, shadow, scene, post);
        Require(shadow == -1, "Zero frequency was accepted");
    }
    Require(Query::m_liveCount == 0, "Destruction leaked queries");
}
} // namespace

int main() {
    try {
        RunScenarios<false>();
        RunScenarios<true>();
        std::puts("GPU profiler: both timing layouts, failures, saturation, pending/disjoint results and lifetime passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
