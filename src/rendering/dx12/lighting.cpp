#include "renderer.h"
#include "../../launch_options.h"
#include "world_placements.h"

namespace game {
Dx12Renderer::SceneConstants Dx12Renderer::ConstantsForFrame(const camera::Pose& pose, float solar,
                                                             float daylight, bool temporalAA) {
    const bool fsrJitterActive = m_fsr.active;
    const float fsrJitterX = m_fsr.jitterX, fsrJitterY = m_fsr.jitterY;
#include "../scene_constants_body.inl"
}
} // namespace game
