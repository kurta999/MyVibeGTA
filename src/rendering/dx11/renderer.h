#pragma once
#include "timing_api.h"
#include "resource_api.h"
#include "../growable_buffer.h"
#include "../texture_library.h"
#include "../loading.h"
#include "../frame_types.h"
#include "../screenshot_writer.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "../../physics.h"
#include <d3d11.h>
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
class Dx11Renderer final {
  public:
    Dx11Renderer() = default;
    ~Dx11Renderer() {
        ShutdownRenderer();
    }
    Dx11Renderer(const Dx11Renderer&) = delete;
    Dx11Renderer& operator=(const Dx11Renderer&) = delete;
    bool ExclusiveFullscreenEnabled();
    bool SetExclusiveFullscreen(bool enabled, int width, int height);
    bool InitRenderer();
    void Render();
    bool BakeReflectionProbes();
    void ShutdownRenderer();

  private:
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    std::unique_ptr<cpu::Pool> m_scenePool;
    ID3D11RenderTargetView* m_target = nullptr;
    ID3D11Texture2D* m_sceneTexture = nullptr;
    ID3D11RenderTargetView* m_sceneTarget = nullptr;
    ID3D11ShaderResourceView* m_sceneView = nullptr;
    ID3D11Texture2D* m_surfaceTexture = nullptr;
    ID3D11RenderTargetView* m_surfaceTarget = nullptr;
    ID3D11ShaderResourceView* m_surfaceView = nullptr;
    ID3D11Texture2D* m_indirectTexture = nullptr;
    ID3D11RenderTargetView* m_indirectTarget = nullptr;
    ID3D11ShaderResourceView* m_indirectView = nullptr;
    ID3D11Texture2D* m_reflectionResponseTexture = nullptr;
    ID3D11RenderTargetView* m_reflectionResponseTarget = nullptr;
    ID3D11ShaderResourceView* m_reflectionResponseView = nullptr;
    ID3D11ShaderResourceView* m_probeView = nullptr;
    ID3D11ShaderResourceView* m_brdfView = nullptr;
    ID3D11Buffer* m_probeBuffer = nullptr;
    dx11::probes::Set m_probeSet;
    using ProbeConstants = ::rendering::ProbeConstants;
    bool m_probeBakeActive = false, m_probeBakeFailed = false;
    unsigned m_probeBakeFrame = 0;
    inline static constexpr XMFLOAT4 m_probePositions[2] = {{100, 42, 100, 750},
                                                            {300, 42, 350, 650}};
    inline static constexpr float m_probeHours[3] = {12, 18.5f, 22};
    ID3D11Texture2D* m_depthTexture = nullptr;
    ID3D11DepthStencilView* m_depthView = nullptr;
    ID3D11ShaderResourceView* m_depthViewSRV = nullptr;
    ID3D11VertexShader* m_sceneVS = nullptr;
    ID3D11VertexShader* m_instanceVS = nullptr;
    ID3D11PixelShader* m_scenePS = nullptr;
    ID3D11PixelShader* m_alphaShadowPS = nullptr;
    ID3D11HullShader* m_sceneHS = nullptr;
    ID3D11DomainShader* m_sceneDS = nullptr;
    ID3D11VertexShader* m_hudVS = nullptr;
    ID3D11PixelShader* m_hudPS = nullptr;
    ID3D11PixelShader* m_postPS = nullptr;
    ID3D11PixelShader* m_bloomPS = nullptr;
    ID3D11PixelShader* m_reflectionPS = nullptr;
    ID3D11PixelShader* m_motionPS = nullptr;
    ID3D11PixelShader* m_temporalPS = nullptr;
    ID3D11PixelShader* m_temporalCopyPS = nullptr;
    ID3D11PixelShader *m_meterPS = nullptr, *m_meterReducePS = nullptr, *m_exposurePS = nullptr,
                      *m_tonePS = nullptr;
    ID3D11Buffer* m_toneBuffer = nullptr;
    ID3D11Texture2D* m_composedTexture = nullptr;
    ID3D11RenderTargetView* m_composedTarget = nullptr;
    ID3D11ShaderResourceView* m_composedView = nullptr;
    ID3D11Texture2D* m_meterTexture[dx11::tone::meterLevels]{};
    ID3D11RenderTargetView* m_meterTarget[dx11::tone::meterLevels]{};
    ID3D11ShaderResourceView* m_meterView[dx11::tone::meterLevels]{};
    ID3D11Texture2D* m_exposureTexture[2]{};
    ID3D11RenderTargetView* m_exposureTarget[2]{};
    ID3D11ShaderResourceView* m_exposureView[2]{};
    int m_exposureIndex = 0;
    bool m_exposureValid = false;
    unsigned m_exposureFrame = 0;
    std::chrono::steady_clock::time_point m_exposureClock{};
    ID3D11Buffer* m_postBuffer = nullptr;
    ID3D11ShaderResourceView* m_cloudNoiseView = nullptr;
    ID3D11SamplerState* m_cloudSampler = nullptr;
    ID3D11Buffer* m_bloomBuffer = nullptr;
    ID3D11Texture2D* m_bloomTexture[3]{};
    ID3D11RenderTargetView* m_bloomTarget[3]{};
    ID3D11ShaderResourceView* m_bloomView[3]{};
    ID3D11Texture2D* m_reflectionTexture = nullptr;
    ID3D11RenderTargetView* m_reflectionTarget = nullptr;
    ID3D11ShaderResourceView* m_reflectionView = nullptr;
    ID3D11Texture2D* m_postTexture = nullptr;
    ID3D11RenderTargetView* m_postTarget = nullptr;
    ID3D11ShaderResourceView* m_postView = nullptr;
    ID3D11Texture2D* m_motionTexture = nullptr;
    ID3D11RenderTargetView* m_motionTarget = nullptr;
    ID3D11ShaderResourceView* m_motionView = nullptr;
    ID3D11Texture2D* m_objectMotionTexture = nullptr;
    ID3D11RenderTargetView* m_objectMotionTarget = nullptr;
    ID3D11ShaderResourceView* m_objectMotionView = nullptr;
    ID3D11Texture2D* m_historyTexture[2]{};
    ID3D11RenderTargetView* m_historyTarget[2]{};
    ID3D11ShaderResourceView* m_historyView[2]{};
    int m_historyIndex = 0;
    bool m_historyValid = false;
    unsigned m_temporalFrame = 0;
    XMFLOAT4X4 m_previousViewProjection{};
    XMFLOAT3 m_previousCameraEye{}, m_previousCameraForward{};
    ID3D11InputLayout* m_inputLayout = nullptr;
    ID3D11InputLayout* m_instanceLayout = nullptr;
    ID3D11Buffer* m_staticBuffer = nullptr;
    ID3D11ComputeShader* m_skinCS = nullptr;
    ID3D11Buffer* m_skinConstants = nullptr;
    ID3D11Buffer* m_skinOutput = nullptr;
    ID3D11UnorderedAccessView* m_skinOutputUav = nullptr;
    ID3D11Buffer* m_skinPreviousOutput = nullptr;
    ID3D11UnorderedAccessView* m_skinPreviousUav = nullptr;
    ID3D11VertexShader* m_skinVS = nullptr;
    ID3D11InputLayout* m_skinLayout = nullptr;
    using PreviousSkin = ::rendering::PreviousSkin;
    std::unordered_map<std::uint64_t, PreviousSkin> m_previousSkins;
    std::uint64_t m_skinFrame = 0;
    std::unordered_map<const dx11::SkinMesh*, ID3D11ShaderResourceView*> m_skinSources;
    std::vector<dx11::SkinInstance> m_gpuSkins;
    size_t m_skinCapacity = 0, m_skinVertexCount = 0;
    bool m_skinValidationDone = false;
    struct SkinConstants {
        std::array<float, 16> m_palette[dx11::MAX_GPU_SKIN_JOINTS];
        std::array<float, 16> m_previousPalette[dx11::MAX_GPU_SKIN_JOINTS];
        std::array<float, 4> m_scale, m_origin, m_transform, m_yaw;
        std::array<float, 4> m_previousScale, m_previousOrigin, m_previousTransform, m_previousYaw;
        UINT m_count, m_start, m_joints, m_padding;
    };
    static_assert(sizeof(SkinConstants) % 16 == 0);
    using ResourceApi = ::rendering::Dx11ResourceApi;
    std::unique_ptr<::rendering::GrowableBuffer<ResourceApi>> m_dynamicVertices;
    std::unique_ptr<::rendering::GrowableBuffer<ResourceApi>> m_dynamicInstances;
    std::unique_ptr<::rendering::MeshCache<ResourceApi>> m_meshCache;
    std::unique_ptr<::rendering::TextureLibrary<ResourceApi>> m_textureLibrary;
    ID3D11Buffer* m_sceneBuffer = nullptr;
    ID3D11RasterizerState* m_rasterState = nullptr;
    ID3D11RasterizerState* m_shadowRaster = nullptr;
    ID3D11Texture2D* m_shadowTexture = nullptr;
    ID3D11DepthStencilView* m_shadowDepth[3]{};
    ID3D11ShaderResourceView* m_shadowView = nullptr;
    ID3D11Texture2D* m_headlightShadowTexture = nullptr;
    ID3D11DepthStencilView* m_headlightShadowDepth = nullptr;
    ID3D11ShaderResourceView* m_headlightShadowView = nullptr;
    ID3D11Texture2D* m_streetShadowTexture = nullptr;
    ID3D11DepthStencilView* m_streetShadowDepth = nullptr;
    ID3D11ShaderResourceView* m_streetShadowView = nullptr;
    ID3D11SamplerState* m_shadowSampler = nullptr;
    int m_shadowSize = 0;
    int m_shadowCascadeCount = 0;
    ID3D11DepthStencilState* m_noDepth = nullptr;
    ID3D11DepthStencilState* m_readDepth = nullptr;
    ID3D11BlendState* m_alphaBlend = nullptr;
    ID3D11BlendState* m_hudBlend = nullptr;
    ID3D11SamplerState* m_sampler = nullptr;
    ID3D11SamplerState* m_modelSampler = nullptr;
    ID3D11SamplerState* m_clampSampler = nullptr;
    int m_activeFiltering = -1;
    ID3D11ShaderResourceView* m_textures[2]{};
    ID3D11ShaderResourceView* m_detailTextures[dx11::MATERIAL_GROUPS]{};
    ID3D11ShaderResourceView* m_normalTextures[dx11::MATERIAL_GROUPS]{};
    ID3D11Texture2D* m_hudTexture = nullptr;
    ID3D11ShaderResourceView* m_hudView = nullptr;
    ULONG_PTR m_gdiplusToken = 0;
    int m_bufferW = 0, m_bufferH = 0;
    std::vector<dx11::Vertex> m_groups[dx11::MATERIAL_GROUPS];
    size_t m_staticStarts[dx11::MATERIAL_GROUPS]{}, m_staticCounts[dx11::MATERIAL_GROUPS]{};
    std::uint64_t m_groundRevision = ~std::uint64_t(0);
    std::vector<dx11::Vertex> m_vertices;
    std::vector<dx11::ModelInstance> m_models;
    using InstanceData = ::rendering::InstanceData;
    using InstanceBatch = ::rendering::InstanceBatch;
    std::vector<InstanceData> m_instanceData;
    std::vector<InstanceBatch> m_instanceBatches;
    std::array<std::vector<InstanceBatch>, 3> m_shadowInstanceBatches;
    std::vector<InstanceBatch> m_headlightShadowInstanceBatches;
    std::vector<InstanceBatch> m_streetShadowInstanceBatches;
    std::vector<unsigned char> m_hudPixels;
    ::rendering::ScreenshotWriter m_screenshotWriter;
    bool m_deviceLost = false;
    ::rendering::GpuProfiler<::rendering::Dx11TimingApi> m_profiler;
    using SceneConstants = ::rendering::SceneConstants;
    struct PostConstants {
        XMFLOAT4X4 m_viewProjection, m_inverseViewProjection;
        XMFLOAT4 m_cameraEye, m_pixelSize, m_grade, m_effects, m_skyTop, m_skyHorizon, m_debug;
        XMFLOAT4X4 m_previousViewProjection;
        XMFLOAT4 m_temporal, m_sunDirection, m_sunScreen, m_skyWeather;
    };

    template <class T> void Release(T*& object) {
        if (object) {
            object->Release();
            object = nullptr;
        }
    }
    void LogAdapter();

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
