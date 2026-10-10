#include "renderer.h"

namespace game {
using ::rendering::GpuStage;
void Dx11Renderer::Render() {
    if (m_deviceLost || !m_device || !m_context)
        return;
    if (m_groundRevision != excavation::revision() && !CreateStaticGeometry())
        return;
    if (std::strstr(GetCommandLineA(), "--smoke") &&
        std::strstr(GetCommandLineA(), "--high-shadows"))
        ui::shadowQuality = 2;
    if (std::strstr(GetCommandLineA(), "--smoke") && std::strstr(GetCommandLineA(), "--high-taa"))
        ui::antiAliasingQuality = 2;
    if (std::strstr(GetCommandLineA(), "--smoke") && std::strstr(GetCommandLineA(), "--low-aa"))
        ui::antiAliasingQuality = 0;
    auto renderBegin = std::chrono::steady_clock::now();
    drawCalls = 0;
    triangleCount = 0;
    m_profiler.Poll(*m_context, gpuShadowMs, gpuSceneMs, gpuPostMs);
    if (!UpdateTextureSampler())
        return;
    int width = std::max(1, screenW), height = std::max(1, screenH);
    if (width != m_bufferW || height != m_bufferH) {
        ReleaseTargets();
        if (!CreateTargets(width, height))
            return;
    }
    // Scene LOD and draw culling share the same interpolated camera pose.
    Vec2 focus = (occupied >= 0 || rightMouse)
                     ? player
                     : previousPlayer * (1.0f - renderAlpha) + player * renderAlpha;
    camera::Pose pose =
        camera::compute(focus, playerY, rightMouse && occupied < 0 && !ui::paused(), occupied);
    if (m_probeBakeActive) {
        unsigned probe = m_probeBakeFrame / 18, state = (m_probeBakeFrame / 6) % 3,
                 face = m_probeBakeFrame % 6;
        constexpr Vec3 directions[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                        {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
        const auto& origin = m_probePositions[probe];
        player = previousPlayer = {origin.x, origin.z};
        gameHour = m_probeHours[state];
        pose.eye = {origin.x, origin.y, origin.z};
        pose.target = {origin.x + directions[face].x, origin.y + directions[face].y,
                       origin.z + directions[face].z};
    }
    dx11::buildScene(m_groups, m_models, pose.eye.x, pose.eye.y, pose.eye.z,
                     m_skinCS ? &m_gpuSkins : nullptr, m_probeBakeActive, m_scenePool.get());
    if (m_skinCS)
        PrepareSkins();
    else {
        m_gpuSkins.clear();
        m_skinVertexCount = 0;
    }
    auto sceneBuilt = std::chrono::steady_clock::now();
    m_vertices.clear();
    size_t starts[dx11::MATERIAL_GROUPS]{}, counts[dx11::MATERIAL_GROUPS]{};
    for (int group = 0; group < dx11::MATERIAL_GROUPS; ++group) {
        starts[group] = m_vertices.size();
        counts[group] = m_groups[group].size();
        m_vertices.insert(m_vertices.end(), m_groups[group].begin(), m_groups[group].end());
    }
    if (!m_dynamicVertices->Ensure(std::max(size_t(1), m_vertices.size())))
        return;
    auto* vertexBuffer = m_dynamicVertices->Get();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    if (!m_vertices.empty())
        std::memcpy(mapped.pData, m_vertices.data(), m_vertices.size() * sizeof(dx11::Vertex));
    m_context->Unmap(vertexBuffer, 0);
    float solar = std::sin((gameHour - 6) * PI / 12.0f);
    float daylight = std::clamp(solar * 2.3f + 0.42f, 0.0f, 1.0f);
    int requestedShadowSize = ui::shadowQuality == 0 ? 0 : ui::shadowQuality == 1 ? 1024 : 2048;
    int requestedCascades = ui::shadowQuality == 0 ? 0 : ui::shadowQuality == 1 ? 1 : 3;
    if ((requestedShadowSize != m_shadowSize || requestedCascades != m_shadowCascadeCount) &&
        !CreateShadowTargets(requestedShadowSize, requestedCascades))
        CreateShadowTargets(0, 0);
    dx11::sortInstancesForRendering(m_models, pose.eye.x, pose.eye.y, pose.eye.z);
    const char* commandLine = GetCommandLineA();
    const bool temporalAA = ui::graphicsQuality > 0 && ui::antiAliasingQuality == 2 &&
                            !std::strstr(commandLine, "--no-taa") && !m_probeBakeActive;
    const bool motionDebug = std::strstr(commandLine, "--motion-view") != nullptr;
    SceneConstants constants = ConstantsForFrame(pose, solar, daylight, temporalAA);
    UpdateProbes();
    constants.m_previousViewProjection = m_previousViewProjection;
    constants.m_temporalInfo = {0, 0, 1.0f / width, 1.0f / height};
    XMFLOAT3 forward{pose.target.x - pose.eye.x, pose.target.y - pose.eye.y,
                     pose.target.z - pose.eye.z};
    float forwardLength =
        std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (forwardLength > 0.0001f) {
        forward.x /= forwardLength;
        forward.y /= forwardLength;
        forward.z /= forwardLength;
    }
    const float eyeDistance =
        std::sqrt((pose.eye.x - m_previousCameraEye.x) * (pose.eye.x - m_previousCameraEye.x) +
                  (pose.eye.y - m_previousCameraEye.y) * (pose.eye.y - m_previousCameraEye.y) +
                  (pose.eye.z - m_previousCameraEye.z) * (pose.eye.z - m_previousCameraEye.z));
    const float forwardDot = forward.x * m_previousCameraForward.x +
                             forward.y * m_previousCameraForward.y +
                             forward.z * m_previousCameraForward.z;
    if ((!temporalAA && !motionDebug) || eyeDistance > 65.0f || forwardDot < 0.93f)
        m_historyValid = false;
    if (!PrepareInstances(pose, constants))
        return;
    auto instancesReady = std::chrono::steady_clock::now();
    UINT stride = sizeof(dx11::Vertex), offset = 0;
    m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_sceneVS, nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, &m_sceneBuffer);
    m_context->HSSetConstantBuffers(0, 1, &m_sceneBuffer);
    m_context->DSSetConstantBuffers(0, 1, &m_sceneBuffer);
    m_profiler.Begin(*m_context);
    if (constants.m_params.w > 0) {
        ID3D11ShaderResourceView* empty = nullptr;
        m_context->PSSetShaderResources(4, 1, &empty);
        D3D11_VIEWPORT shadowViewport{};
        shadowViewport.Width = shadowViewport.Height = float(m_shadowSize);
        shadowViewport.MaxDepth = 1;
        m_context->RSSetViewports(1, &shadowViewport);
        m_context->RSSetState(m_shadowRaster);
        m_context->PSSetShader(nullptr, nullptr, 0);
        m_context->PSSetSamplers(0, 1, &m_sampler);
        m_context->PSSetSamplers(2, 1, &m_modelSampler);
        SceneConstants shadowConstants = constants;
        for (int cascade = 0; cascade < m_shadowCascadeCount; ++cascade) {
            m_context->OMSetRenderTargets(0, nullptr, m_shadowDepth[cascade]);
            m_context->ClearDepthStencilView(m_shadowDepth[cascade], D3D11_CLEAR_DEPTH, 1, 0);
            shadowConstants.m_viewProjection = constants.m_shadowViewProjection[cascade];
            m_context->IASetInputLayout(m_inputLayout);
            m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            m_context->VSSetShader(m_sceneVS, nullptr, 0);
            m_context->PSSetShader(nullptr, nullptr, 0);
            for (int group = 0; group < dx11::MATERIAL_GROUPS; ++group)
                if (counts[group]) {
                    SetTessellation(group);
                    shadowConstants.m_params.x = float(group);
                    m_context->UpdateSubresource(m_sceneBuffer, 0, nullptr, &shadowConstants, 0, 0);
                    m_context->Draw(UINT(counts[group]), UINT(starts[group]));
                    ++drawCalls;
                    triangleCount += counts[group] / 3;
                }
            DrawSkins(true, shadowConstants);
            DrawInstances(true, shadowConstants, false, cascade);
        }
    }
    if (constants.m_headlightShadowInfo.z > 0.5f) {
        ID3D11ShaderResourceView* empty = nullptr;
        m_context->PSSetShaderResources(9, 1, &empty);
        D3D11_VIEWPORT viewport{};
        viewport.Width = viewport.Height = 1024.0f;
        viewport.MaxDepth = 1;
        m_context->RSSetViewports(1, &viewport);
        m_context->RSSetState(m_shadowRaster);
        m_context->OMSetRenderTargets(0, nullptr, m_headlightShadowDepth);
        m_context->ClearDepthStencilView(m_headlightShadowDepth, D3D11_CLEAR_DEPTH, 1, 0);
        m_context->PSSetShader(nullptr, nullptr, 0);
        m_context->PSSetSamplers(0, 1, &m_sampler);
        m_context->PSSetSamplers(2, 1, &m_modelSampler);
        m_context->IASetInputLayout(m_inputLayout);
        m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        m_context->VSSetShader(m_sceneVS, nullptr, 0);
        SceneConstants shadowConstants = constants;
        shadowConstants.m_viewProjection = constants.m_headlightViewProjection;
        for (int group = 0; group < dx11::MATERIAL_GROUPS; ++group)
            if (counts[group]) {
                SetTessellation(group);
                shadowConstants.m_params.x = float(group);
                m_context->UpdateSubresource(m_sceneBuffer, 0, nullptr, &shadowConstants, 0, 0);
                m_context->Draw(UINT(counts[group]), UINT(starts[group]));
                ++drawCalls;
                triangleCount += counts[group] / 3;
            }
        DrawSkins(true, shadowConstants);
        DrawInstances(true, shadowConstants, false, 0, 1);
    }
    if (constants.m_streetShadowInfo.y > 0.5f) {
        ID3D11ShaderResourceView* empty = nullptr;
        m_context->PSSetShaderResources(10, 1, &empty);
        D3D11_VIEWPORT viewport{};
        viewport.Width = viewport.Height = 512.0f;
        viewport.MaxDepth = 1;
        m_context->RSSetViewports(1, &viewport);
        m_context->RSSetState(m_shadowRaster);
        m_context->OMSetRenderTargets(0, nullptr, m_streetShadowDepth);
        m_context->ClearDepthStencilView(m_streetShadowDepth, D3D11_CLEAR_DEPTH, 1, 0);
        m_context->PSSetShader(nullptr, nullptr, 0);
        m_context->PSSetSamplers(0, 1, &m_sampler);
        m_context->PSSetSamplers(2, 1, &m_modelSampler);
        m_context->IASetInputLayout(m_inputLayout);
        m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        m_context->VSSetShader(m_sceneVS, nullptr, 0);
        SceneConstants shadowConstants = constants;
        shadowConstants.m_viewProjection = constants.m_streetViewProjection;
        for (int group = 0; group < dx11::MATERIAL_GROUPS; ++group)
            if (counts[group]) {
                SetTessellation(group);
                shadowConstants.m_params.x = float(group);
                m_context->UpdateSubresource(m_sceneBuffer, 0, nullptr, &shadowConstants, 0, 0);
                m_context->Draw(UINT(counts[group]), UINT(starts[group]));
                ++drawCalls;
                triangleCount += counts[group] / 3;
            }
        DrawSkins(true, shadowConstants);
        DrawInstances(true, shadowConstants, false, 0, 2);
    }
    m_profiler.Mark(*m_context, GpuStage::ShadowEnd);
    float clearColor[] = {constants.m_fogColor.x, constants.m_fogColor.y, constants.m_fogColor.z,
                          1};
    ID3D11RenderTargetView* opaqueTargets[5] = {m_sceneTarget, m_surfaceTarget, m_indirectTarget,
                                                m_objectMotionTarget, m_reflectionResponseTarget};
    m_context->OMSetRenderTargets(5, opaqueTargets, m_depthView);
    m_context->ClearRenderTargetView(m_sceneTarget, clearColor);
    float clearSurface[] = {0.5f, 1.0f, 0.5f, 1.0f};
    m_context->ClearRenderTargetView(m_surfaceTarget, clearSurface);
    float clearIndirect[] = {0, 0, 0, 0};
    m_context->ClearRenderTargetView(m_indirectTarget, clearIndirect);
    m_context->ClearRenderTargetView(m_objectMotionTarget, clearIndirect);
    m_context->ClearRenderTargetView(m_reflectionResponseTarget, clearIndirect);
    m_context->ClearDepthStencilView(m_depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
    D3D11_VIEWPORT mainViewport{};
    mainViewport.Width = float(m_bufferW);
    mainViewport.Height = float(m_bufferH);
    mainViewport.MaxDepth = 1;
    m_context->RSSetViewports(1, &mainViewport);
    m_context->RSSetState(m_rasterState);
    m_context->VSSetShader(m_sceneVS, nullptr, 0);
    m_context->PSSetShader(m_scenePS, nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, &m_sceneBuffer);
    m_context->PSSetConstantBuffers(0, 1, &m_sceneBuffer);
    m_context->PSSetSamplers(0, 1, &m_sampler);
    m_context->PSSetSamplers(1, 1, &m_shadowSampler);
    m_context->PSSetSamplers(2, 1, &m_modelSampler);
    m_context->PSSetShaderResources(4, 1, &m_shadowView);
    m_context->PSSetShaderResources(9, 1, &m_headlightShadowView);
    m_context->PSSetShaderResources(10, 1, &m_streetShadowView);
    m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout);
    m_context->VSSetShader(m_sceneVS, nullptr, 0);
    for (int group = 0; group < dx11::MATERIAL_GROUPS; ++group) {
        if (!counts[group] && !m_staticCounts[group])
            continue;
        constants.m_params.x = float(group);
        constants.m_temporalInfo.x = group == 3 || group == 5 || group == 14 ? 0.0f : 1.0f;
        constants.m_materialPbr = PbrForGroup(group);
        constants.m_materialSurface = {0, 0.1f, 0, 0};
        m_context->UpdateSubresource(m_sceneBuffer, 0, nullptr, &constants, 0, 0);
        SetTessellation(group);
        ID3D11ShaderResourceView* baseTexture = group == 1 ? m_textures[1] : nullptr;
        ID3D11ShaderResourceView* resources[4] = {baseTexture, m_detailTextures[group],
                                                  m_normalTextures[group], m_detailTextures[9]};
        m_context->PSSetShaderResources(0, 4, resources);
        if (m_staticCounts[group]) {
            m_context->IASetVertexBuffers(0, 1, &m_staticBuffer, &stride, &offset);
            m_context->Draw(UINT(m_staticCounts[group]), UINT(m_staticStarts[group]));
            ++drawCalls;
            triangleCount += m_staticCounts[group] / 3;
        }
        if (counts[group]) {
            m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            m_context->Draw(UINT(counts[group]), UINT(starts[group]));
            ++drawCalls;
            triangleCount += counts[group] / 3;
        }
    }
    DrawSkins(false, constants);
    DrawInstances(false, constants);
    float effectBlend[4]{0, 0, 0, 0};
    m_context->OMSetRenderTargets(1, &m_sceneTarget, m_depthView);
    m_context->OMSetDepthStencilState(m_readDepth, 0);
    m_context->OMSetBlendState(m_alphaBlend, effectBlend, 0xffffffffu);
    DrawInstances(false, constants, true);
    m_context->OMSetBlendState(nullptr, effectBlend, 0xffffffffu);
    m_context->OMSetDepthStencilState(nullptr, 0);
    SetTessellation(-1);
    m_profiler.Mark(*m_context, GpuStage::SceneEnd);
    if (m_probeBakeActive) {
        m_probeBakeFailed = !CaptureProbe(constants);
        if (m_probeBakeFailed)
            logging::write("HDR probe capture failed");
        m_profiler.End(*m_context);
        return;
    }
    if (ui::graphicsQuality > 0) {
        m_context->OMSetDepthStencilState(m_noDepth, 0);
        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_context->VSSetShader(m_hudVS, nullptr, 0);
        m_context->PSSetShader(m_bloomPS, nullptr, 0);
        m_context->PSSetSamplers(0, 1, &m_clampSampler);
        m_context->PSSetConstantBuffers(0, 1, &m_bloomBuffer);
        for (int level = 0; level < 3; ++level) {
            UINT sourceWidth = UINT(std::max(1, m_bufferW >> level));
            UINT sourceHeight = UINT(std::max(1, m_bufferH >> level));
            XMFLOAT4 info{1.0f / sourceWidth, 1.0f / sourceHeight, level == 0 ? 1.0f : 0.0f, 0};
            m_context->UpdateSubresource(m_bloomBuffer, 0, nullptr, &info, 0, 0);
            D3D11_VIEWPORT viewport{};
            viewport.Width = float(std::max(1, m_bufferW >> (level + 1)));
            viewport.Height = float(std::max(1, m_bufferH >> (level + 1)));
            viewport.MaxDepth = 1;
            m_context->RSSetViewports(1, &viewport);
            m_context->OMSetRenderTargets(1, &m_bloomTarget[level], nullptr);
            ID3D11ShaderResourceView* source = level == 0 ? m_sceneView : m_bloomView[level - 1];
            m_context->PSSetShaderResources(0, 1, &source);
            m_context->Draw(4, 0);
            ++drawCalls;
            ID3D11ShaderResourceView* empty = nullptr;
            m_context->PSSetShaderResources(0, 1, &empty);
        }
    }
    PostConstants post{};
    post.m_viewProjection = constants.m_viewProjection;
    XMStoreFloat4x4(&post.m_inverseViewProjection,
                    XMMatrixInverse(nullptr, XMLoadFloat4x4(&constants.m_viewProjection)));
    post.m_cameraEye = constants.m_eye;
    post.m_pixelSize = {1.0f / m_bufferW, 1.0f / m_bufferH, 2.0f,
                        std::max(5000.0f, 1250.0f * ui::drawDistanceScale())};
    const auto biome = regions::biomeAt(player);
    XMFLOAT4 tint{1, 1, 1, 1.15f};
    if (biome == regions::Biome::Snow)
        tint = {0.90f, 0.97f, 1.08f, 1.12f};
    else if (biome == regions::Biome::Desert)
        tint = {1.09f, 1.01f, 0.86f, 1.12f};
    else if (biome == regions::Biome::Savanna)
        tint = {1.07f, 1.04f, 0.88f, 1.12f};
    float twilight = std::max(0.0f, 1.0f - std::abs(solar) * 4.0f);
    tint.x += twilight * 0.10f;
    tint.y -= twilight * 0.04f;
    tint.z -= twilight * 0.12f;
    float night = 1.0f - daylight;
    tint.x -= night * 0.09f;
    tint.y -= night * 0.06f;
    tint.z += night * 0.04f;
    tint.w *= 1.0f - weather::current().clouds * 0.13f;
    post.m_grade = tint;
    post.m_skyTop = {0.015f + 0.10f * daylight, 0.025f + 0.32f * daylight, 0.09f + 0.60f * daylight,
                     weather::current().clouds};
    post.m_skyHorizon = {0.055f + 0.55f * daylight, 0.075f + 0.68f * daylight,
                         0.15f + 0.69f * daylight, gameHour};
    post.m_skyTop.x += twilight * 0.10f;
    post.m_skyTop.y -= twilight * 0.07f;
    post.m_skyHorizon.x += twilight * 0.34f;
    post.m_skyHorizon.y -= twilight * 0.18f;
    post.m_skyHorizon.z -= twilight * 0.27f;
    float cloudFade = weather::current().clouds * 0.55f;
    const XMFLOAT4 overcast{0.36f, 0.40f, 0.45f, 1};
    post.m_skyTop.x = post.m_skyTop.x * (1 - cloudFade) + overcast.x * cloudFade;
    post.m_skyTop.y = post.m_skyTop.y * (1 - cloudFade) + overcast.y * cloudFade;
    post.m_skyTop.z = post.m_skyTop.z * (1 - cloudFade) + overcast.z * cloudFade;
    post.m_skyHorizon.x = post.m_skyHorizon.x * (1 - cloudFade) + overcast.x * cloudFade;
    post.m_skyHorizon.y = post.m_skyHorizon.y * (1 - cloudFade) + overcast.y * cloudFade;
    post.m_skyHorizon.z = post.m_skyHorizon.z * (1 - cloudFade) + overcast.z * cloudFade;
    post.m_effects = {float(ui::aoQuality), ui::graphicsQuality > 0 ? 0.12f : 0.0f,
                      temporalAA                     ? 0.0f
                      : ui::antiAliasingQuality == 0 ? 0.75f
                                                     : float(ui::antiAliasingQuality),
                      ui::graphicsQuality > 0 ? float(ui::reflectionQuality) : 0.0f};
    const bool probeDebug =
        std::strstr(commandLine, "--probe-view") || std::strstr(commandLine, "--probe-weight-view");
    if (probeDebug)
        post.m_effects = {0, 0, 0, 0};
    post.m_debug.x = std::strstr(commandLine, "--normal-view")      ? 1.0f
                     : std::strstr(commandLine, "--roughness-view") ? 2.0f
                     : std::strstr(commandLine, "--indirect-view")  ? 5.0f
                     : std::strstr(commandLine, "--bloom-view")     ? 4.0f
                     : std::strstr(commandLine, "--shadow-cascade-view") ||
                             std::strstr(commandLine, "--material-view") ||
                             std::strstr(commandLine, "--mip-view") ||
                             std::strstr(commandLine, "--probe-weight-view")
                         ? 3.0f
                         : 0.0f;
    post.m_previousViewProjection =
        m_historyValid ? m_previousViewProjection : constants.m_viewProjection;
    post.m_temporal = {m_historyValid ? 1.0f : 0.0f, motionDebug ? 1.0f : 0.0f, 0, 0};
    XMVECTOR solarDirection =
        XMVector3Normalize(XMVectorSet(constants.m_sun.x, constants.m_sun.y, constants.m_sun.z, 0));
    XMStoreFloat4(&post.m_sunDirection, solarDirection);
    post.m_sunDirection.w =
        constants.m_sun.w * (1 - weather::current().clouds) * (1 - weather::current().clouds);
    XMFLOAT4 solarClip;
    XMStoreFloat4(&solarClip,
                  XMVector4Transform(solarDirection, XMLoadFloat4x4(&constants.m_viewProjection)));
    post.m_sunScreen = {solarClip.w > 0 ? solarClip.x / solarClip.w * .5f + .5f : -2,
                        solarClip.w > 0 ? .5f - solarClip.y / solarClip.w * .5f : -2,
                        .00465f / (2 * std::tan(camera::fieldOfView() * PI / 360.0f)),
                        ui::effectsQuality > 0 && solarClip.w > 0 && !probeDebug &&
                                !post.m_debug.x && !std::strstr(commandLine, "--no-lens-flare")
                            ? 1.0f
                            : 0.0f};
    post.m_debug.w = std::strstr(commandLine, "--flare-view") ? 1.0f : 0.0f;
    post.m_skyWeather = {worldTime * weather::current().wind.x * 18,
                         worldTime * weather::current().wind.z * 18,
                         ui::graphicsQuality == 0   ? 24.0f
                         : ui::graphicsQuality == 1 ? 36.0f
                                                    : 48.0f,
                         daylight};
    m_context->UpdateSubresource(m_postBuffer, 0, nullptr, &post, 0, 0);
    if (post.m_effects.w > 0.5f) {
        D3D11_VIEWPORT reflectionViewport{};
        reflectionViewport.Width = float(std::max(1, m_bufferW / 2));
        reflectionViewport.Height = float(std::max(1, m_bufferH / 2));
        reflectionViewport.MaxDepth = 1;
        m_context->RSSetViewports(1, &reflectionViewport);
        m_context->OMSetRenderTargets(1, &m_reflectionTarget, nullptr);
        m_context->OMSetDepthStencilState(m_noDepth, 0);
        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_context->VSSetShader(m_hudVS, nullptr, 0);
        m_context->PSSetShader(m_reflectionPS, nullptr, 0);
        m_context->PSSetSamplers(0, 1, &m_clampSampler);
        m_context->PSSetConstantBuffers(0, 1, &m_postBuffer);
        ID3D11ShaderResourceView* inputs[4] = {m_sceneView, m_depthViewSRV, m_surfaceView,
                                               m_reflectionResponseView};
        m_context->PSSetShaderResources(0, 4, inputs);
        m_context->Draw(4, 0);
        ++drawCalls;
        ID3D11ShaderResourceView* empty[4]{};
        m_context->PSSetShaderResources(0, 4, empty);
    }
    D3D11_VIEWPORT postViewport{};
    postViewport.Width = float(m_bufferW);
    postViewport.Height = float(m_bufferH);
    postViewport.MaxDepth = 1;
    m_context->RSSetViewports(1, &postViewport);
    m_context->OMSetRenderTargets(1, &m_composedTarget, nullptr);
    m_context->OMSetDepthStencilState(m_noDepth, 0);
    m_context->IASetInputLayout(nullptr);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_context->VSSetShader(m_hudVS, nullptr, 0);
    m_context->PSSetShader(m_postPS, nullptr, 0);
    m_context->PSSetSamplers(0, 1, &m_clampSampler);
    m_context->PSSetConstantBuffers(0, 1, &m_postBuffer);
    ID3D11ShaderResourceView* postInputs[8] = {m_sceneView,      m_depthViewSRV, m_surfaceView,
                                               m_bloomView[0],   m_bloomView[1], m_bloomView[2],
                                               m_reflectionView, m_indirectView};
    m_context->PSSetShaderResources(0, 8, postInputs);
    m_context->PSSetShaderResources(8, 1, &m_cloudNoiseView);
    m_context->PSSetSamplers(1, 1, &m_cloudSampler);
    m_context->Draw(4, 0);
    ++drawCalls;
    ID3D11ShaderResourceView* emptyCloud = nullptr;
    m_context->PSSetShaderResources(8, 1, &emptyCloud);
    ID3D11ShaderResourceView* emptyInputs[8]{};
    m_context->PSSetShaderResources(0, 8, emptyInputs);
    dx11::tone::Constants tone;
    tone.grade[0] = post.m_grade.x;
    tone.grade[1] = post.m_grade.y;
    tone.grade[2] = post.m_grade.z;
    tone.grade[3] = post.m_grade.w;
    const auto now = std::chrono::steady_clock::now();
    tone.adaptation[0] = std::strstr(commandLine, "--smoke") ? 1.0f / 60
                         : m_exposureValid
                             ? std::chrono::duration<float>(now - m_exposureClock).count()
                             : 0;
    m_exposureClock = now;
    tone.adaptation[1] = m_exposureValid ? 0 : 1;
    tone.adaptation[2] = std::strstr(commandLine, "--fixed-exposure") ? 0 : 1;
    tone.adaptation[3] = post.m_debug.x || post.m_debug.w || motionDebug ? 1 : 0;
    tone.options[0] = std::strstr(commandLine, "--legacy-tonemap") ? 1 : 0;
    m_context->UpdateSubresource(m_toneBuffer, 0, nullptr, &tone, 0, 0);
    m_context->PSSetConstantBuffers(1, 1, &m_toneBuffer);
    ID3D11ShaderResourceView* toneEmpty[2]{};
    if (!tone.adaptation[3]) {
        m_context->PSSetShader(m_meterPS, nullptr, 0);
        for (unsigned level = 0; level < dx11::tone::meterLevels; ++level) {
            D3D11_VIEWPORT viewport{};
            viewport.Width = viewport.Height = float(dx11::tone::meterSize >> level);
            viewport.MaxDepth = 1;
            m_context->RSSetViewports(1, &viewport);
            m_context->OMSetRenderTargets(1, &m_meterTarget[level], nullptr);
            ID3D11ShaderResourceView* inputs[2] = {level ? m_meterView[level - 1] : m_composedView,
                                                   level ? nullptr : m_depthViewSRV};
            m_context->PSSetShaderResources(0, 2, inputs);
            m_context->PSSetShader(level ? m_meterReducePS : m_meterPS, nullptr, 0);
            m_context->Draw(4, 0);
            ++drawCalls;
            m_context->PSSetShaderResources(0, 2, toneEmpty);
        }
        const int writeIndex = 1 - m_exposureIndex;
        m_context->OMSetRenderTargets(1, &m_exposureTarget[writeIndex], nullptr);
        m_context->PSSetShader(m_exposurePS, nullptr, 0);
        ID3D11ShaderResourceView* inputs[2] = {m_meterView[dx11::tone::meterLevels - 1],
                                               m_exposureView[m_exposureIndex]};
        m_context->PSSetShaderResources(0, 2, inputs);
        m_context->Draw(4, 0);
        ++drawCalls;
        m_context->PSSetShaderResources(0, 2, toneEmpty);
        m_exposureIndex = writeIndex;
        m_exposureValid = true;
    }
    m_context->RSSetViewports(1, &postViewport);
    m_context->OMSetRenderTargets(1, &m_postTarget, nullptr);
    m_context->PSSetShader(m_tonePS, nullptr, 0);
    ID3D11ShaderResourceView* toneInputs[2] = {m_composedView, m_exposureView[m_exposureIndex]};
    m_context->PSSetShaderResources(0, 2, toneInputs);
    m_context->Draw(4, 0);
    ++drawCalls;
    m_context->PSSetShaderResources(0, 2, toneEmpty);
    ID3D11ShaderResourceView* displayImage = m_postView;
    if (temporalAA || motionDebug) {
        m_context->OMSetRenderTargets(1, &m_motionTarget, nullptr);
        m_context->PSSetShader(m_motionPS, nullptr, 0);
        ID3D11ShaderResourceView* motionInputs[3] = {m_depthViewSRV, m_indirectView,
                                                     m_objectMotionView};
        m_context->PSSetShaderResources(0, 3, motionInputs);
        m_context->Draw(4, 0);
        ++drawCalls;
        ID3D11ShaderResourceView* emptyMotion[3]{};
        m_context->PSSetShaderResources(0, 3, emptyMotion);
        ID3D11ShaderResourceView* empty = nullptr;
        int writeIndex = 1 - m_historyIndex;
        m_context->OMSetRenderTargets(1, &m_historyTarget[writeIndex], nullptr);
        m_context->PSSetShader(m_temporalPS, nullptr, 0);
        ID3D11ShaderResourceView* temporalInputs[4] = {m_postView, m_depthViewSRV, m_motionView,
                                                       m_historyView[m_historyIndex]};
        m_context->PSSetShaderResources(0, 4, temporalInputs);
        m_context->Draw(4, 0);
        ++drawCalls;
        ID3D11ShaderResourceView* emptyTemporal[4]{};
        m_context->PSSetShaderResources(0, 4, emptyTemporal);
        displayImage = m_historyView[writeIndex];
        m_historyIndex = writeIndex;
        m_historyValid = temporalAA || motionDebug;
    }
    m_context->OMSetRenderTargets(1, &m_target, nullptr);
    m_context->PSSetShader(m_temporalCopyPS, nullptr, 0);
    m_context->PSSetShaderResources(0, 1, &displayImage);
    m_context->Draw(4, 0);
    ++drawCalls;
    ID3D11ShaderResourceView* displayEmpty = nullptr;
    m_context->PSSetShaderResources(0, 1, &displayEmpty);
    if (std::strstr(commandLine, "--exposure-log") && m_exposureFrame % 30 == 0) {
        D3D11_TEXTURE2D_DESC description{};
        m_exposureTexture[m_exposureIndex]->GetDesc(&description);
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* staging = nullptr;
        if (SUCCEEDED(m_device->CreateTexture2D(&description, nullptr, &staging))) {
            m_context->CopyResource(staging, m_exposureTexture[m_exposureIndex]);
            D3D11_MAPPED_SUBRESOURCE read{};
            if (SUCCEEDED(m_context->Map(staging, 0, D3D11_MAP_READ, 0, &read))) {
                const float* values = static_cast<const float*>(read.pData);
                char info[180]{};
                std::snprintf(info, sizeof(info),
                              "Exposure frame %u: hour %.2f, scale %.6f, target %.6f, EV %.6f",
                              m_exposureFrame, gameHour, std::exp2(values[0]), std::exp2(values[1]),
                              values[0]);
                logging::write(info);
                m_context->Unmap(staging, 0);
            }
            Release(staging);
        }
    }
    ++m_exposureFrame;
    auto hudBegin = std::chrono::steady_clock::now();
    dx11::buildHud(m_hudPixels.data(), m_bufferW, m_bufferH);
    if (GetEnvironmentVariableA("MINICITY_CPU_PROFILE", nullptr, 0)) {
        char timing[100]{};
        std::snprintf(
            timing, sizeof(timing), "HUD profile: %.3f ms",
            std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - hudBegin)
                .count());
        logging::write(timing);
    }
    if (SUCCEEDED(m_context->Map(m_hudTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        for (int row = 0; row < m_bufferH; ++row)
            std::memcpy(static_cast<unsigned char*>(mapped.pData) + size_t(row) * mapped.RowPitch,
                        m_hudPixels.data() + size_t(row) * m_bufferW * 4, size_t(m_bufferW) * 4);
        m_context->Unmap(m_hudTexture, 0);
        m_context->OMSetDepthStencilState(m_noDepth, 0);
        float blend[4]{0, 0, 0, 0};
        m_context->OMSetBlendState(m_hudBlend, blend, 0xffffffffu);
        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_context->VSSetShader(m_hudVS, nullptr, 0);
        m_context->PSSetShader(m_hudPS, nullptr, 0);
        m_context->PSSetShaderResources(0, 1, &m_hudView);
        m_context->Draw(4, 0);
        ++drawCalls;
        m_context->OMSetBlendState(nullptr, blend, 0xffffffffu);
        m_context->OMSetDepthStencilState(nullptr, 0);
    }
    m_profiler.End(*m_context);
    CaptureIfRequested();
    auto beforePresent = std::chrono::steady_clock::now();
    HRESULT presentResult =
        m_swapChain->Present(std::strstr(GetCommandLineA(), "--benchmark") ? 0 : 1, 0);
    if (FAILED(presentResult)) {
        char failure[128]{};
        std::snprintf(failure, sizeof(failure),
                      "DX11 Present failed: 0x%08lX; device reason: 0x%08lX",
                      static_cast<unsigned long>(presentResult),
                      static_cast<unsigned long>(m_device->GetDeviceRemovedReason()));
        logging::write(failure);
        m_deviceLost = true;
    }
    if (SUCCEEDED(presentResult)) {
        m_previousViewProjection = constants.m_viewProjection;
        m_previousCameraEye = {pose.eye.x, pose.eye.y, pose.eye.z};
        m_previousCameraForward = forward;
        ++m_temporalFrame;
    }
    auto afterPresent = std::chrono::steady_clock::now();
    renderSceneMs = std::chrono::duration<float, std::milli>(sceneBuilt - renderBegin).count();
    renderUploadMs = std::chrono::duration<float, std::milli>(instancesReady - sceneBuilt).count();
    renderDrawMs = std::chrono::duration<float, std::milli>(beforePresent - instancesReady).count();
    renderPresentMs =
        std::chrono::duration<float, std::milli>(afterPresent - beforePresent).count();
    if (GetEnvironmentVariableA("MINICITY_CPU_PROFILE", nullptr, 0)) {
        char timing[240]{};
        std::snprintf(timing, sizeof(timing),
                      "Render profile: scene %.3f ms, upload/cull %.3f ms, draw/HUD %.3f ms, "
                      "present %.3f ms, draws %d",
                      renderSceneMs, renderUploadMs, renderDrawMs, renderPresentMs, drawCalls);
        logging::write(timing);
    }
    if (std::strstr(GetCommandLineA(), "--benchmark-travel") &&
        std::chrono::duration<float, std::milli>(afterPresent - renderBegin).count() > 30) {
        char timing[200]{};
        std::snprintf(
            timing, sizeof(timing),
            "Travel frame at %.0f,%.0f: scene %.1f ms, uploads %.1f ms, draw/HUD %.1f ms, present "
            "%.1f ms",
            player.x, player.z,
            std::chrono::duration<float, std::milli>(sceneBuilt - renderBegin).count(),
            std::chrono::duration<float, std::milli>(instancesReady - sceneBuilt).count(),
            std::chrono::duration<float, std::milli>(beforePresent - instancesReady).count(),
            std::chrono::duration<float, std::milli>(afterPresent - beforePresent).count());
        logging::write(timing);
    }
}
} // namespace game
