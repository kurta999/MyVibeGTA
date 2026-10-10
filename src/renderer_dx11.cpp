#include "rendering/dx11/renderer.h"

namespace game {
namespace {
std::unique_ptr<Dx11Renderer> renderer;
}

bool initRenderer() {
    if (renderer) return true;
    auto candidate = std::make_unique<Dx11Renderer>();
    if (!candidate->InitRenderer()) return false;
    renderer = std::move(candidate);
    return true;
}

void render() { if (renderer) renderer->Render(); }
void shutdownRenderer() { renderer.reset(); }
bool bakeReflectionProbes() { return renderer && renderer->BakeReflectionProbes(); }
bool exclusiveFullscreenEnabled() { return renderer && renderer->ExclusiveFullscreenEnabled(); }
bool setExclusiveFullscreen(bool enabled, int width, int height) {
    return renderer ? renderer->SetExclusiveFullscreen(enabled, width, height) : !enabled;
}
} // namespace game
