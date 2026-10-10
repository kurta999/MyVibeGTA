#include "renderer.h"

namespace game {
Dx11Renderer::SceneConstants Dx11Renderer::ConstantsForFrame(const camera::Pose& pose, float solar,
                                                             float daylight, bool temporalAA) {
    SceneConstants constants{};
    const auto& conditions = weather::current();
    float light = daylight * (1.0f - conditions.clouds * 0.42f);
    XMVECTOR eye = XMVectorSet(pose.eye.x, pose.eye.y, pose.eye.z, 1);
    XMVECTOR targetPoint = XMVectorSet(pose.target.x, pose.target.y, pose.target.z, 1);
    // Movement, steering, and mouse yaw use the same right-handed view as the OpenGL build.
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    if (m_probeBakeActive && m_probeBakeFrame % 6 == 2)
        up = XMVectorSet(0, 0, -1, 0);
    if (m_probeBakeActive && m_probeBakeFrame % 6 == 3)
        up = XMVectorSet(0, 0, 1, 0);
    XMMATRIX view = m_probeBakeActive ? XMMatrixLookAtLH(eye, targetPoint, up)
                                      : XMMatrixLookAtRH(eye, targetPoint, up);
    float drawScale = ui::drawDistanceScale();
    XMMATRIX projection = m_probeBakeActive
                              ? XMMatrixPerspectiveFovLH(XM_PIDIV2, 1, 2, 1250 * drawScale)
                              : XMMatrixPerspectiveFovRH(XMConvertToRadians(camera::fieldOfView()),
                                                         float(m_bufferW) / m_bufferH, 2.0f,
                                                         std::max(5000.0f, 1250.0f * drawScale));
    if (temporalAA) {
        // Eight subpixel sample positions. Projection jitter is included in
        // both the current and previous matrices used for reprojection.
        constexpr float jitter[8][2] = {
            {0.0f, -0.166667f},  {-0.25f, 0.166667f},   {0.25f, -0.388889f}, {-0.375f, -0.055556f},
            {0.125f, 0.277778f}, {-0.125f, -0.277778f}, {0.375f, 0.055556f}, {-0.4375f, 0.388889f}};
        const auto& sample = jitter[m_temporalFrame % 8];
        projection =
            XMMatrixMultiply(projection, XMMatrixTranslation(2.0f * sample[0] / m_bufferW,
                                                             -2.0f * sample[1] / m_bufferH, 0));
    }
    XMStoreFloat4x4(&constants.m_viewProjection, XMMatrixMultiply(view, projection));
    constants.m_sun = {std::cos((gameHour - 6) * PI / 12), solar, 0.3f,
                       std::clamp(solar * 6.0f, 0.0f, 1.0f) * light};
    if (std::strstr(GetCommandLineA(), "--no-direct-sun"))
        constants.m_sun.w = 0;
    XMVECTOR focus = XMVectorSet(player.x, playerY, player.z, 1);
    XMVECTOR sunDirection =
        XMVector3Normalize(XMVectorSet(constants.m_sun.x, constants.m_sun.y, constants.m_sun.z, 0));
    XMVECTOR cameraForward = XMVector3Normalize(XMVectorSubtract(targetPoint, eye));
    const float splitNear[3] = {2.0f, 250.0f * drawScale, 650.0f * drawScale};
    const float splitFar[3] = {250.0f * drawScale, 650.0f * drawScale, 1250.0f * drawScale};
    for (int cascade = 0; cascade < 3; ++cascade) {
        const bool high = m_shadowCascadeCount > 1;
        float extent = high ? ShadowCascadeExtent(cascade) : 1800.0f * drawScale;
        XMVECTOR center =
            high ? XMVectorAdd(eye, XMVectorScale(cameraForward,
                                                  (splitNear[cascade] + splitFar[cascade]) * 0.5f))
                 : focus;
        XMVECTOR lightEye = XMVectorAdd(center, XMVectorScale(sunDirection, 1400));
        XMMATRIX lightView = XMMatrixLookAtRH(lightEye, center, XMVectorSet(0, 1, 0, 0));
        XMVECTOR origin = XMVector3TransformCoord(XMVectorZero(), lightView);
        float texel = extent / float(std::max(1, m_shadowSize));
        float snapX = std::round(XMVectorGetX(origin) / texel) * texel - XMVectorGetX(origin);
        float snapY = std::round(XMVectorGetY(origin) / texel) * texel - XMVectorGetY(origin);
        lightView = XMMatrixMultiply(lightView, XMMatrixTranslation(snapX, snapY, 0));
        XMMATRIX lightProjection = XMMatrixOrthographicRH(extent, extent, 1, 3300);
        XMStoreFloat4x4(&constants.m_shadowViewProjection[cascade],
                        XMMatrixMultiply(lightView, lightProjection));
    }
    const float night = 1.0f - daylight;
    const float twilight = std::max(0.0f, 1.0f - std::abs(solar) * 3.5f);
    constants.m_ambient = {0.18f + 0.36f * light, 0.21f + 0.35f * light, 0.27f + 0.34f * light, 0};
    constants.m_fogColor = {0.045f + 0.52f * light, 0.065f + 0.68f * light, 0.14f + 0.74f * light,
                            1};
    constants.m_fogColor.x =
        constants.m_fogColor.x * (1 - conditions.clouds * 0.4f) + 0.40f * conditions.clouds * 0.4f;
    constants.m_fogColor.y =
        constants.m_fogColor.y * (1 - conditions.clouds * 0.4f) + 0.43f * conditions.clouds * 0.4f;
    constants.m_fogColor.z =
        constants.m_fogColor.z * (1 - conditions.clouds * 0.4f) + 0.47f * conditions.clouds * 0.4f;
    constants.m_fogColor.x += twilight * 0.20f;
    constants.m_fogColor.y += twilight * 0.055f;
    constants.m_fogColor.z -= twilight * 0.045f;
    const auto biome = regions::biomeAt(player);
    if (biome == regions::Biome::Snow) {
        constants.m_fogColor.x += 0.045f;
        constants.m_fogColor.y += 0.075f;
        constants.m_fogColor.z += 0.10f;
    } else if (biome == regions::Biome::Desert || biome == regions::Biome::Savanna) {
        constants.m_fogColor.x += 0.09f;
        constants.m_fogColor.y += 0.045f;
        constants.m_fogColor.z -= 0.025f;
    }
    constants.m_weatherAndTime = {conditions.snow ? 0.0f : conditions.precipitation,
                                  conditions.snow ? std::max(0.5f, conditions.precipitation)
                                  : biome == regions::Biome::Snow ? 0.75f
                                                                  : 0.0f,
                                  night, worldTime};
    constants.m_ambient.w = float(ui::reflectionQuality);
    struct LocalLight {
        XMFLOAT4 position, color;
        float distance;
        bool playerHeadlight, streetLight;
    };
    std::vector<LocalLight> lights;
    auto addLight = [&](float x, float y, float z, float radius, float r, float g, float b,
                        float brightness, bool playerHeadlight = false, bool streetLight = false) {
        float distance = std::hypot(x - pose.eye.x, z - pose.eye.z);
        if (distance > radius + 330)
            return;
        lights.push_back(
            {{x, y, z, radius}, {r, g, b, brightness}, distance, playerHeadlight, streetLight});
    };
    if (night > 0.1f) {
        const bool modernLamps = dx11::mesh("modern/street-lamp") != nullptr;
        for (int column = 0; column < 5; ++column)
            for (int row = 0; row < 7; ++row)
                addLight(368.0f + column * 450 - (modernLamps ? 13.1f : 0),
                         modernLamps ? 44.65f : 43, 115.0f + row * 215, 115, 1.0f, 0.74f, 0.41f,
                         1.7f * night, false, true);
        for (int z = 8860; z < 10010; z += 116)
            addLight(8066.0f, 36, float(z), 95, 1.0f, 0.78f, 0.50f, 1.4f * night, false, true);
        for (const auto& shop : commerce::shops)
            addLight(shop.p.x, 20, shop.p.z, 65, 0.35f, 0.90f, 1.0f, 0.95f * night);
    }
    if (!m_probeBakeActive) {
        for (const auto& vehicle : vehicles) {
            if (!vehicleLightsOn(vehicle) ||
                std::hypot(vehicle.p.x - player.x, vehicle.p.z - player.z) > 390)
                continue;
            Vec2 facing = forward(vehicle.angle);
            Vec2 side{-facing.z, facing.x};
            float scale = physics::vehicleScale(vehicle.kind);
            float front = (vehicle.kind == Kind::Bike ? 13.0f : 24.05f) * scale;
            float width = (vehicle.kind == Kind::Bike ? 0.0f : 8.0f) * scale;
            for (float sign : {-1.0f, 1.0f}) {
                if (width == 0 && sign > 0)
                    continue;
                addLight(vehicle.p.x + facing.x * front + side.x * width * sign,
                         vehicle.rideHeight + (width == 0 ? 17.0f : 9.5f) * scale,
                         vehicle.p.z + facing.z * front + side.z * width * sign, 125, 1.0f, 0.94f,
                         0.72f, 1.7f,
                         occupied >= 0 && occupied < int(vehicles.size()) &&
                             &vehicle == &vehicles[occupied]);
                if (width > 0)
                    addLight(vehicle.p.x - facing.x * front + side.x * width * sign,
                             vehicle.rideHeight + 9.7f * scale,
                             vehicle.p.z - facing.z * front + side.z * width * sign, 45, 1.0f,
                             0.09f, 0.04f, 0.75f);
            }
        }
        if (builder::active())
            for (const auto& block : builder::blocks())
                if (builder::items()[block.second.item].id == "torch") {
                    auto p = builder::cellLow(block.first);
                    addLight(p.x + 20, p.y + 38, p.z + 20, 180, 1.0f, .65f, .22f, 1.2f);
                }
        for (const auto& flame : fire::active())
            addLight(flame.p.x, 12, flame.p.z, 70 + flame.intensity * 25, 1.0f, 0.29f, 0.08f,
                     0.85f + flame.intensity * 0.65f);
        for (const auto& ped : peds)
            if (ped.alive && ped.burnTime > 0)
                addLight(ped.p.x, 18, ped.p.z, 75, 1.0f, 0.30f, 0.08f, 1.2f);
        for (const auto& vehicle : vehicles)
            if (!vehicle.exploded && vehicle.burnTime > 0)
                addLight(vehicle.p.x, vehicle.rideHeight + 20, vehicle.p.z, 105, 1.0f, 0.30f, 0.08f,
                         1.5f);
    }
    std::sort(lights.begin(), lights.end(),
              [](const LocalLight& a, const LocalLight& b) { return a.distance < b.distance; });
    constants.m_headlightShadowInfo = {-1, -1, 0, 1.0f / 1024.0f};
    XMStoreFloat4x4(&constants.m_headlightViewProjection, XMMatrixIdentity());
    constants.m_streetShadowInfo = {-1, 0, 1.0f / 512.0f, 0};
    XMStoreFloat4x4(&constants.m_streetViewProjection, XMMatrixIdentity());
    for (size_t i = 0; i < std::min<size_t>(lights.size(), 12); ++i) {
        constants.m_localLightPosition[i] = lights[i].position;
        constants.m_localLightColor[i] = lights[i].color;
        if (lights[i].playerHeadlight) {
            if (constants.m_headlightShadowInfo.x < 0)
                constants.m_headlightShadowInfo.x = float(i);
            else
                constants.m_headlightShadowInfo.y = float(i);
        }
        if (lights[i].streetLight && constants.m_streetShadowInfo.x < 0)
            constants.m_streetShadowInfo.x = float(i);
    }
    if (m_headlightShadowDepth && ui::shadowQuality > 1 && constants.m_headlightShadowInfo.x >= 0 &&
        occupied >= 0 && occupied < int(vehicles.size())) {
        const Vehicle& vehicle = vehicles[occupied];
        Vec2 facing = forward(vehicle.angle);
        float scale = physics::vehicleScale(vehicle.kind);
        float front = (vehicle.kind == Kind::Bike ? 17.0f : 28.0f) * scale;
        XMVECTOR lamp =
            XMVectorSet(vehicle.p.x + facing.x * front,
                        vehicle.rideHeight + (vehicle.kind == Kind::Bike ? 17.0f : 10.0f) * scale,
                        vehicle.p.z + facing.z * front, 1);
        XMVECTOR target = XMVectorAdd(lamp, XMVectorSet(facing.x * 100, -14, facing.z * 100, 0));
        XMMATRIX view = XMMatrixLookAtRH(lamp, target, XMVectorSet(0, 1, 0, 0));
        XMMATRIX projection =
            XMMatrixPerspectiveFovRH(XMConvertToRadians(105.0f), 1.0f, 2.0f, 145.0f);
        XMStoreFloat4x4(&constants.m_headlightViewProjection, XMMatrixMultiply(view, projection));
        constants.m_headlightShadowInfo.z = 1;
    }
    if (m_streetShadowDepth && ui::shadowQuality > 1 && constants.m_streetShadowInfo.x >= 0) {
        const auto& lamp = lights[size_t(constants.m_streetShadowInfo.x)].position;
        XMVECTOR position = XMVectorSet(lamp.x, lamp.y, lamp.z, 1);
        XMVECTOR target = XMVectorSet(lamp.x, 0, lamp.z, 1);
        XMMATRIX view = XMMatrixLookAtRH(position, target, XMVectorSet(0, 0, 1, 0));
        XMMATRIX projection =
            XMMatrixPerspectiveFovRH(XMConvertToRadians(135.0f), 1.0f, 2.0f, 130.0f);
        XMStoreFloat4x4(&constants.m_streetViewProjection, XMMatrixMultiply(view, projection));
        constants.m_streetShadowInfo.y = 1;
    }
    constants.m_eye = {pose.eye.x, pose.eye.y, pose.eye.z, float(ui::graphicsQuality)};
    float fogEnd = std::max(regions::biomeAt(player) == regions::Biome::City ? 0.0f : 4200.0f,
                            1250.0f * drawScale * 0.94f) *
                   conditions.visibility;
    constants.m_params = {0, fogEnd * 0.51f, fogEnd,
                          m_shadowDepth[0] && ui::shadowQuality > 0 && daylight > 0.05f ? 1.0f
                                                                                        : 0.0f};
    constants.m_shadowInfo = {1.0f / float(std::max(1, m_shadowSize)), 250.0f * drawScale,
                              650.0f * drawScale, float(m_shadowCascadeCount)};
    constants.m_materialOptions.y =
        std::strstr(GetCommandLineA(), "--shadow-cascade-view") ? 1.0f : 0.0f;
    constants.m_materialOptions.z = std::strstr(GetCommandLineA(), "--material-view") ? 1.0f : 0.0f;
    constants.m_materialOptions.w = std::strstr(GetCommandLineA(), "--mip-view") ? 1.0f : 0.0f;
    return constants;
}
} // namespace game
