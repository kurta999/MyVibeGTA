#include "rendering/dx11/renderer.h"
#include "launch_options.h"
#include "logging.h"
#include "savegame.h"

namespace game {
namespace {
std::unique_ptr<Dx11Renderer> renderer;

// A removed/reset GPU device used to leave a frozen window while the game
// kept running. Try once to rebuild the renderer on a fresh device; if that
// fails, save, explain and quit cleanly.
void recoverFromDeviceLoss() {
    logging::write("Graphics device lost; recreating the renderer");
    renderer.reset();
    auto candidate = std::make_unique<Dx11Renderer>();
    if (candidate->InitRenderer()) {
        renderer = std::move(candidate);
        logging::write("Renderer recreated after device loss");
        return;
    }
    candidate.reset();
    logging::write("Renderer could not be recreated after device loss; exiting");
    const bool saved = savegame::save();
    if (!launch::smoke())
        MessageBoxA(win,
                    saved ? "The graphics device stopped responding (driver reset or GPU removed).\n"
                            "Your progress has been saved. Please restart MyVibeGTA."
                          : "The graphics device stopped responding (driver reset or GPU removed).\n"
                            "Please restart MyVibeGTA.",
                    "MyVibeGTA", MB_ICONERROR);
    PostQuitMessage(0);
}
}

bool initRenderer() {
    if (renderer) return true;
    auto candidate = std::make_unique<Dx11Renderer>();
    if (!candidate->InitRenderer()) return false;
    renderer = std::move(candidate);
    return true;
}

void render() {
    if (!renderer)
        return;
    renderer->Render();
    if (renderer->DeviceLost())
        recoverFromDeviceLoss();
}
void shutdownRenderer() { renderer.reset(); }
bool bakeReflectionProbes() { return renderer && renderer->BakeReflectionProbes(); }
bool exclusiveFullscreenEnabled() { return renderer && renderer->ExclusiveFullscreenEnabled(); }
bool setExclusiveFullscreen(bool enabled, int width, int height) {
    return renderer ? renderer->SetExclusiveFullscreen(enabled, width, height) : !enabled;
}
} // namespace game
