#include "renderer.h"
#include "../../launch_options.h"
#include "world_placements.h"

namespace game {
Dx11Renderer::SceneConstants Dx11Renderer::ConstantsForFrame(const camera::Pose& pose, float solar,
                                                             float daylight, bool temporalAA) {
    // DX11 has no FSR2 upscaler; only the TAA jitter applies.
    constexpr bool fsrJitterActive = false;
    constexpr float fsrJitterX = 0, fsrJitterY = 0;
#include "../scene_constants_body.inl"
}
} // namespace game
