#pragma once
#include "resource_owner.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace rendering {
enum class GpuStage : unsigned {
    SceneBegin,
    ShadowEnd,
    SceneEnd,
    FrameEnd,
    SkinBegin,
    SkinEnd,
    BloomEnd,
    ReflectionsEnd,
    CloudsEnd,
    CompositionEnd,
    ToneEnd,
    TemporalEnd
};

struct GpuTimingStatistics {
    double m_skinMilliseconds = 0;
    unsigned m_skinSamples = 0;
    std::array<double, 7> m_postMilliseconds{};
    unsigned m_postSamples = 0;
};

// A nonblocking ring of timestamp queries. Api supplies only the differences
// between D3D11's disjoint queries and D3D12's fence-backed timestamp queries.
// Pending slots are never reused; when all slots are busy, profiling skips a
// frame instead of making the renderer wait for the GPU.
template <class Api> class GpuProfiler final {
  public:
    using Device = typename Api::Device;
    using Context = typename Api::Context;
    GpuProfiler() = default;
    GpuProfiler(const GpuProfiler&) = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;

    bool Initialize(Device& device) {
        Reset();
        m_statistics = {};
        for (auto& frame : m_frames) {
            typename Api::Query* frequency = nullptr;
            const bool frequencyCreated = Api::CreateFrequency(device, &frequency);
            frame.m_frequency.reset(frequency);
            if (!frequencyCreated) {
                Reset();
                return false;
            }
            for (auto& timestamp : frame.m_timestamps) {
                typename Api::Query* query = nullptr;
                const bool timestampCreated = Api::CreateTimestamp(device, &query);
                timestamp.reset(query);
                if (!timestampCreated) {
                    Reset();
                    return false;
                }
            }
        }
        return true;
    }

    void Reset() {
        m_active = nullptr;
        m_cursor = 0;
        for (auto& frame : m_frames) {
            frame.m_frequency.reset();
            for (auto& timestamp : frame.m_timestamps)
                timestamp.reset();
            frame.m_pending = false;
        }
    }

    void Poll(Context& context, float& shadowMs, float& sceneMs, float& postMs) {
        for (auto& frame : m_frames) {
            if (!frame.m_pending)
                continue;
            typename Api::Frequency frequency{};
            if (!Api::Read(context, frame.m_frequency.get(), &frequency, sizeof(frequency)))
                continue;
            std::array<std::uint64_t, Api::TimestampCount> times{};
            bool ready = true;
            for (std::size_t index = 0; index < times.size(); ++index) {
                if (!Api::Read(context, frame.m_timestamps[index].get(), &times[index],
                               sizeof(times[index]))) {
                    ready = false;
                    break;
                }
            }
            if (!ready)
                continue;
            frame.m_pending = false;
            if (frequency.Disjoint || !frequency.Frequency)
                continue;
            const double scale = 1000.0 / double(frequency.Frequency);
            shadowMs = float((times[1] - times[0]) * scale);
            sceneMs = float((times[2] - times[1]) * scale);
            postMs = float((times[3] - times[2]) * scale);
            if constexpr (Api::Extended) {
                m_statistics.m_skinMilliseconds += (times[5] - times[4]) * scale;
                ++m_statistics.m_skinSamples;
                auto previous = times[2];
                for (std::size_t stage = 0; stage < 7; ++stage) {
                    const auto next = stage < 6 ? times[stage + 6] : times[3];
                    m_statistics.m_postMilliseconds[stage] += (next - previous) * scale;
                    previous = next;
                }
                ++m_statistics.m_postSamples;
            }
        }
    }

    void Begin(Context& context) {
        m_active = nullptr;
        auto& frame = m_frames[m_cursor++ % m_frames.size()];
        if (!frame.m_frequency || frame.m_pending)
            return;
        context.Begin(frame.m_frequency.get());
        m_active = &frame;
        Mark(context, Api::Extended ? GpuStage::SkinBegin : GpuStage::SceneBegin);
    }

    void Mark(Context& context, GpuStage stage) {
        const auto index = static_cast<std::size_t>(stage);
        if (m_active && index < Api::TimestampCount)
            context.End(m_active->m_timestamps[index].get());
    }

    void End(Context& context) {
        if (!m_active)
            return;
        Mark(context, GpuStage::FrameEnd);
        context.End(m_active->m_frequency.get());
        m_active->m_pending = true;
        m_active = nullptr;
    }

    const GpuTimingStatistics& Statistics() const {
        return m_statistics;
    }

  private:
    using OwnedQuery = OwnedGpuResource<typename Api::Query>;
    struct FrameQueries {
        OwnedQuery m_frequency;
        std::array<OwnedQuery, Api::TimestampCount> m_timestamps;
        bool m_pending = false;
    };
    std::array<FrameQueries, 8> m_frames{};
    FrameQueries* m_active = nullptr;
    unsigned m_cursor = 0;
    GpuTimingStatistics m_statistics;
};
} // namespace rendering
