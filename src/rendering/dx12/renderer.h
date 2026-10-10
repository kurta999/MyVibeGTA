#pragma once
#include "timing_api.h"
#include "resource_api.h"
#include "../growable_buffer.h"
#include "../texture_library.h"
#include "../loading.h"
#include "../frame_types.h"
#include "../screenshot_writer.h"
#include "../../dx12_skin_shader.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "../../physics.h"
#include "../../dx12_backend.h"
#include "../../dx12_visibility.h"
#include "../../dx12_cloud_shader.h"
#include "../../fsr2.h"
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include "../../game.h"
#include "../../builder.h"
#include "../../excavation.h"
#include "../../camera.h"
#include "../../dx11_assets.h"
#include "../../dx11_texture_mips.h"
#include "../../dx11_texture_loading.h"
#include "../../dx11_shader_loading.h"
#include "../../cpu_jobs.h"
#include "../../dx11_probes.h"
#include "../../dx11_tonemapping.h"
#include "../../dx11_sky.h"
#include "../../ui.h"
#include "../../weather.h"
#include "../../regions.h"
#include "../../commerce.h"
#include "../../fire.h"
#include "../../logging.h"
#include "../../startup.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {
using namespace DirectX;

// Owns one backend session. Frame orchestration and resource operations are
// implemented in the adjacent files, grouped by rendering responsibility.
class Dx12Renderer final {
  public:
    Dx12Renderer() = default;
    ~Dx12Renderer() {
        ShutdownRenderer();
    }
    Dx12Renderer(const Dx12Renderer&) = delete;
    Dx12Renderer& operator=(const Dx12Renderer&) = delete;
    bool ExclusiveFullscreenEnabled();
    bool SetExclusiveFullscreen(bool enabled, int width, int height);
    bool InitRenderer();
    void Render();
    bool BakeReflectionProbes();
    void ShutdownRenderer();

  private:
    IDXGISwapChain* m_swapChain = nullptr;
    dx12::Device* m_device = nullptr;
    dx12::Context* m_context = nullptr;
    std::unique_ptr<cpu::Pool> m_scenePool;
    std::unique_ptr<cpu::Pool> m_recordPool;
    dx12::RenderTargetView* m_target = nullptr;
    dx12::Texture2D* m_sceneTexture = nullptr;
    dx12::RenderTargetView* m_sceneTarget = nullptr;
    dx12::ShaderResourceView* m_sceneView = nullptr;
    dx12::Texture2D* m_surfaceTexture = nullptr;
    dx12::RenderTargetView* m_surfaceTarget = nullptr;
    dx12::ShaderResourceView* m_surfaceView = nullptr;
    dx12::Texture2D* m_indirectTexture = nullptr;
    dx12::RenderTargetView* m_indirectTarget = nullptr;
    dx12::ShaderResourceView* m_indirectView = nullptr;
    dx12::Texture2D* m_reflectionResponseTexture = nullptr;
    dx12::RenderTargetView* m_reflectionResponseTarget = nullptr;
    dx12::ShaderResourceView* m_reflectionResponseView = nullptr;
    dx12::ShaderResourceView* m_probeView = nullptr;
    dx12::ShaderResourceView* m_brdfView = nullptr;
    dx12::Buffer* m_probeBuffer = nullptr;
    dx11::probes::Set m_probeSet;
    using ProbeConstants = ::rendering::ProbeConstants;
    bool m_probeBakeActive = false, m_probeBakeFailed = false;
    unsigned m_probeBakeFrame = 0;
    inline static constexpr XMFLOAT4 m_probePositions[2] = {{100, 42, 100, 750},
                                                            {300, 42, 350, 650}};
    inline static constexpr float m_probeHours[3] = {12, 18.5f, 22};
    dx12::Texture2D* m_depthTexture = nullptr;
    dx12::DepthStencilView* m_depthView = nullptr;
    dx12::ShaderResourceView* m_depthViewSRV = nullptr;
    dx12::VertexShader* m_sceneVS = nullptr;
    dx12::VertexShader* m_instanceVS = nullptr;
    dx12::PixelShader* m_scenePS = nullptr;
    dx12::PixelShader* m_alphaShadowPS = nullptr;
    dx12::HullShader* m_sceneHS = nullptr;
    dx12::DomainShader* m_sceneDS = nullptr;
    dx12::VertexShader* m_hudVS = nullptr;
    dx12::PixelShader* m_hudPS = nullptr;
    dx12::PixelShader* m_postPS = nullptr;
    dx12::PixelShader* m_cloudPS = nullptr;
    dx12::Texture2D *m_cloudVolumeTexture = nullptr, *m_cloudGuideTexture = nullptr;
    dx12::RenderTargetView *m_cloudVolumeTarget = nullptr, *m_cloudGuideTarget = nullptr;
    dx12::ShaderResourceView *m_cloudVolumeView = nullptr, *m_cloudGuideView = nullptr;
    dx12::PixelShader* m_bloomPS = nullptr;
    dx12::PixelShader* m_reflectionPS = nullptr;
    dx12::PixelShader* m_motionPS = nullptr;
    dx12::PixelShader* m_temporalPS = nullptr;
    dx12::PixelShader* m_temporalCopyPS = nullptr;
    dx12::PixelShader *m_meterPS = nullptr, *m_meterReducePS = nullptr, *m_exposurePS = nullptr,
                      *m_tonePS = nullptr;
    dx12::Buffer* m_toneBuffer = nullptr;
    dx12::Texture2D* m_composedTexture = nullptr;
    dx12::RenderTargetView* m_composedTarget = nullptr;
    dx12::ShaderResourceView* m_composedView = nullptr;
    dx12::Texture2D* m_meterTexture[dx11::tone::meterLevels]{};
    dx12::RenderTargetView* m_meterTarget[dx11::tone::meterLevels]{};
    dx12::ShaderResourceView* m_meterView[dx11::tone::meterLevels]{};
    dx12::Texture2D* m_exposureTexture[2]{};
    dx12::RenderTargetView* m_exposureTarget[2]{};
    dx12::ShaderResourceView* m_exposureView[2]{};
    int m_exposureIndex = 0;
    bool m_exposureValid = false;
    unsigned m_exposureFrame = 0;
    std::chrono::steady_clock::time_point m_exposureClock{};
    dx12::Buffer* m_postBuffer = nullptr;
    dx12::ShaderResourceView* m_cloudNoiseView = nullptr;
    dx12::SamplerState* m_cloudSampler = nullptr;
    dx12::Buffer* m_bloomBuffer = nullptr;
    dx12::Texture2D* m_bloomTexture[3]{};
    dx12::RenderTargetView* m_bloomTarget[3]{};
    dx12::ShaderResourceView* m_bloomView[3]{};
    dx12::Texture2D* m_reflectionTexture = nullptr;
    dx12::RenderTargetView* m_reflectionTarget = nullptr;
    dx12::ShaderResourceView* m_reflectionView = nullptr;
    dx12::Texture2D* m_postTexture = nullptr;
    dx12::RenderTargetView* m_postTarget = nullptr;
    dx12::ShaderResourceView* m_postView = nullptr;
    dx12::Texture2D* m_motionTexture = nullptr;
    dx12::RenderTargetView* m_motionTarget = nullptr;
    dx12::ShaderResourceView* m_motionView = nullptr;
    dx12::Texture2D* m_objectMotionTexture = nullptr;
    dx12::RenderTargetView* m_objectMotionTarget = nullptr;
    dx12::ShaderResourceView* m_objectMotionView = nullptr;
    dx12::Texture2D* m_historyTexture[2]{};
    dx12::RenderTargetView* m_historyTarget[2]{};
    dx12::ShaderResourceView* m_historyView[2]{};
    int m_historyIndex = 0;
    bool m_historyValid = false;
    unsigned m_temporalFrame = 0;
    XMFLOAT4X4 m_previousViewProjection{};
    XMFLOAT3 m_previousCameraEye{}, m_previousCameraForward{};
    dx12::InputLayout* m_inputLayout = nullptr;
    dx12::InputLayout* m_instanceLayout = nullptr;
    dx12::Buffer* m_staticBuffer = nullptr;
    dx12::ComputeShader* m_skinCS = nullptr;
    dx12::Buffer* m_skinConstants = nullptr;
    dx12::Buffer* m_skinOutput = nullptr;
    dx12::UnorderedAccessView* m_skinOutputUav = nullptr;
    dx12::Buffer* m_skinPreviousOutput = nullptr;
    dx12::UnorderedAccessView* m_skinPreviousUav = nullptr;
    dx12::VertexShader* m_skinVS = nullptr;
    dx12::InputLayout* m_skinLayout = nullptr;
    using PreviousSkin = ::rendering::PreviousSkin;
    std::unordered_map<std::uint64_t, PreviousSkin> m_previousSkins;
    std::uint64_t m_skinFrame = 0;
    std::unordered_map<const dx11::SkinMesh*, dx12::ShaderResourceView*> m_skinSources;
    std::vector<dx11::SkinInstance> m_gpuSkins;
    size_t m_skinCapacity = 0, m_skinVertexCount = 0;
    bool m_skinValidationDone = false;
    using SkinConstants = Dx12SkinConstants;
    using ResourceApi = ::rendering::Dx12ResourceApi;
    std::unique_ptr<::rendering::GrowableBuffer<ResourceApi>> m_dynamicVertices;
    std::unique_ptr<::rendering::GrowableBuffer<ResourceApi>> m_dynamicInstances;
    std::unique_ptr<::rendering::MeshCache<ResourceApi>> m_meshCache;
    std::unique_ptr<::rendering::TextureLibrary<ResourceApi>> m_textureLibrary;
    dx12::Buffer* m_sceneBuffer = nullptr;
    dx12::RasterizerState* m_rasterState = nullptr;
    dx12::RasterizerState* m_shadowRaster = nullptr;
    dx12::Texture2D* m_shadowTexture = nullptr;
    dx12::DepthStencilView* m_shadowDepth[3]{};
    dx12::ShaderResourceView* m_shadowView = nullptr;
    dx12::Texture2D* m_headlightShadowTexture = nullptr;
    dx12::DepthStencilView* m_headlightShadowDepth = nullptr;
    dx12::ShaderResourceView* m_headlightShadowView = nullptr;
    dx12::Texture2D* m_streetShadowTexture = nullptr;
    dx12::DepthStencilView* m_streetShadowDepth = nullptr;
    dx12::ShaderResourceView* m_streetShadowView = nullptr;
    dx12::SamplerState* m_shadowSampler = nullptr;
    int m_shadowSize = 0;
    int m_shadowCascadeCount = 0;
    dx12::DepthStencilState* m_noDepth = nullptr;
    dx12::DepthStencilState* m_readDepth = nullptr;
    dx12::BlendState* m_alphaBlend = nullptr;
    dx12::BlendState* m_hudBlend = nullptr;
    dx12::SamplerState* m_sampler = nullptr;
    dx12::SamplerState* m_modelSampler = nullptr;
    dx12::SamplerState* m_clampSampler = nullptr;
    int m_activeFiltering = -1;
    dx12::ShaderResourceView* m_textures[2]{};
    dx12::ShaderResourceView* m_detailTextures[dx11::MATERIAL_GROUPS]{};
    dx12::ShaderResourceView* m_normalTextures[dx11::MATERIAL_GROUPS]{};
    dx12::Texture2D* m_hudTexture = nullptr;
    dx12::ShaderResourceView* m_hudView = nullptr;
    ULONG_PTR m_gdiplusToken = 0;
    int m_bufferW = 0, m_bufferH = 0, m_displayW = 0, m_displayH = 0;
    dx12::Fsr2 m_fsr;
    dx12::Texture2D* m_opaqueTexture = nullptr;
    dx12::ShaderResourceView* m_opaqueView = nullptr;
    dx12::Texture2D* m_reactiveTexture = nullptr;
    dx12::RenderTargetView* m_reactiveTarget = nullptr;
    dx12::ShaderResourceView* m_reactiveView = nullptr;
    dx12::PixelShader* m_reactivePS = nullptr;
    std::vector<dx11::Vertex> m_groups[dx11::MATERIAL_GROUPS];
    size_t m_staticStarts[dx11::MATERIAL_GROUPS]{}, m_staticCounts[dx11::MATERIAL_GROUPS]{};
    std::uint64_t m_groundRevision = ~std::uint64_t(0);
    std::vector<dx11::ModelInstance> m_models;
    using InstanceData = ::rendering::InstanceData;
    using InstanceBatch = ::rendering::InstanceBatch;
    std::vector<InstanceData> m_instanceData;
    std::vector<InstanceBatch> m_instanceBatches;
    std::array<std::vector<InstanceBatch>, 3> m_shadowInstanceBatches;
    std::vector<InstanceBatch> m_headlightShadowInstanceBatches;
    std::vector<InstanceBatch> m_streetShadowInstanceBatches;
    struct GeometryStats {
        std::uint64_t m_frames = 0, m_shadowTriangles = 0, m_sceneTriangles = 0,
                      m_casterCandidates = 0, m_receiverCulled = 0, m_lodInstances = 0;
    } m_geometryStats;
    std::vector<unsigned char> m_hudPixels;
    ::rendering::ScreenshotWriter m_screenshotWriter;
    bool m_deviceLost = false;
    ::rendering::GpuProfiler<::rendering::Dx12TimingApi> m_profiler;
    using SceneConstants = ::rendering::SceneConstants;
    using PostConstants = dx12::cloud::Constants;

    template <class T> void Release(T*& object) {
        if (object) {
            object->Release();
            object = nullptr;
        }
    }

    bool CreateProbes();
    void UpdateProbes();
    bool CaptureProbe(const SceneConstants& constants);

    bool CreateSkinResources();
    bool CacheSkin(const dx11::SkinMesh* skin);
    bool GrowSkinBuffer(size_t count);
    bool ValidateSkinOutput(const std::vector<dx11::SkinInstance>& previous,
                            const std::vector<unsigned>& validity);
    void PrepareSkins();
    bool CreateShaders();

    bool CreateTargets(int width, int height);
    bool CacheModel(const dx11::Mesh* source);
    float ShadowCascadeExtent(int cascade);
    bool PrepareInstances(const camera::Pose& pose, const SceneConstants& constants);
    bool CreateStaticGeometry();
    void SetTessellation(int material);
    XMFLOAT4 PbrForGroup(int group);
    void DrawSkins(bool shadow, const SceneConstants& frame);
    void DrawInstances(bool shadow, SceneConstants& constants, bool transparent = false,
                       int cascade = 0, int localShadow = 0);
    bool CreateStates();
    bool UpdateTextureSampler();
    bool CreateShadowTargets(int size, int layers);
    bool CreateHeadlightShadowTarget();
    bool CreateStreetShadowTarget();
    void ReleaseTargets();
    SceneConstants ConstantsForFrame(const camera::Pose& pose, float solar, float daylight,
                                     bool temporalAA);

    void CaptureIfRequested();
};
} // namespace game
