#pragma once
#include "../profiling.h"
#ifdef TRACY_ENABLE
#include <tracy/TracyD3D11.hpp>
#include <optional>
namespace rendering {
class TracyDx11 {
public:
    ~TracyDx11() { Reset(); }
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        Reset();m_context = TracyD3D11Context(device, context);
        TracyD3D11ContextName(m_context, "DX11 immediate", 14);
    }
    void Stage(const tracy::SourceLocationData* source) {
        End();if(m_context)m_zone.emplace(m_context, source, true);
    }
    void End() { m_zone.reset(); }
    void Collect() { End();if(m_context) { TracyD3D11Collect(m_context); } }
    void Reset() { End();if(m_context) { TracyD3D11Destroy(m_context);m_context=nullptr; } }
private:
    TracyD3D11Ctx m_context = nullptr;
    std::optional<tracy::D3D11ZoneScope> m_zone;
};
}
#define CITY_DX11_STAGE(profiler, name) do { \
    static constexpr tracy::SourceLocationData source{name, __FUNCTION__, __FILE__, __LINE__, 0}; \
    profiler.Stage(&source); } while(false)
#else
namespace rendering {
class TracyDx11 {
public:
    template<class D, class C> void Initialize(D*, C*) {}
    void End() {} void Collect() {} void Reset() {}
};
}
#define CITY_DX11_STAGE(profiler, name) ((void)0)
#endif
