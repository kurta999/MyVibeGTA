#pragma once
// Only the Direct3D game targets link Tracy. Tests and OpenGL compile these out.
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#include <optional>
namespace profiling {
class Phase {
public:
    void Stage(const tracy::SourceLocationData* source) { m_zone.reset();m_zone.emplace(source, true); }
    void End() { m_zone.reset(); }
private:
    std::optional<tracy::ScopedZone> m_zone;
};
}
#define CITY_CPU_STAGE(phase, name) do { \
    static constexpr tracy::SourceLocationData source{name, __FUNCTION__, __FILE__, __LINE__, 0}; \
    phase.Stage(&source); } while(false)
#define CITY_ZONE(name) ZoneScopedN(name)
#define CITY_THREAD(name) tracy::SetThreadName(name)
#define CITY_FRAME() FrameMark
#define CITY_PLOT(name, value) TracyPlot(name, value)
#else
namespace profiling { class Phase { public: void End() {} }; }
#define CITY_CPU_STAGE(phase, name) ((void)0)
#define CITY_ZONE(name) ((void)0)
#define CITY_THREAD(name) ((void)0)
#define CITY_FRAME() ((void)0)
#define CITY_PLOT(name, value) ((void)0)
#endif
