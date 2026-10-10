#include "renderer.h"

namespace game {
void Dx11Renderer::LogAdapter() {
    IDXGIDevice* dxgi = nullptr;
    IDXGIAdapter* adapter = nullptr;
    if (FAILED(m_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi))))
        return;
    if (SUCCEEDED(dxgi->GetAdapter(&adapter))) {
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(adapter->GetDesc(&desc))) {
            char name[160]{};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr,
                                nullptr);
            char line[256]{};
            std::snprintf(
                line, sizeof(line),
                "DX11 adapter: %s (vendor %04X, device %04X, dedicated %llu MiB)", name,
                desc.VendorId, desc.DeviceId,
                static_cast<unsigned long long>(desc.DedicatedVideoMemory / (1024 * 1024)));
            logging::write(line);
        }
    }
    Release(adapter);
    Release(dxgi);
}

bool Dx11Renderer::ExclusiveFullscreenEnabled() {
    BOOL enabled = FALSE;
    return m_swapChain && SUCCEEDED(m_swapChain->GetFullscreenState(&enabled, nullptr)) && enabled;
}

bool Dx11Renderer::SetExclusiveFullscreen(bool enabled, int width, int height) {
    if (!m_swapChain)
        return !enabled;
    BOOL current = FALSE;
    if (FAILED(m_swapChain->GetFullscreenState(&current, nullptr)))
        return false;
    if (bool(current) == enabled)
        return true;
    HRESULT result = m_swapChain->SetFullscreenState(enabled, nullptr);
    bool ok = SUCCEEDED(result);
    if (ok && enabled) {
        DXGI_MODE_DESC mode{};
        mode.Width = width;
        mode.Height = height;
        mode.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        result = m_swapChain->ResizeTarget(&mode);
        ok = SUCCEEDED(result);
        if (!ok)
            m_swapChain->SetFullscreenState(FALSE, nullptr);
    }
    if (!ok) {
        char message[128];
        std::snprintf(message, sizeof(message), "DXGI fullscreen transition failed: 0x%08lx",
                      static_cast<unsigned long>(result));
        logging::write(message);
    }
    if (ok) {
        RECT client{};
        GetClientRect(win, &client);
        screenW = std::max(1, int(client.right));
        screenH = std::max(1, int(client.bottom));
    }
    return ok;
}

bool Dx11Renderer::InitRenderer() {
    if (!startup::report(2, "Starting Direct3D 11"))
        return false;
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Width = UINT(std::max(1, screenW));
    description.BufferDesc.Height = UINT(std::max(1, screenH));
    description.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = win;
    description.SampleDesc.Count = 1;
    description.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0, created{};
    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, &requested, 1,
        D3D11_SDK_VERSION, &description, &m_swapChain, &m_device, &created, &m_context);
    if (FAILED(result))
        result = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, &requested, 1,
            D3D11_SDK_VERSION, &description, &m_swapChain, &m_device, &created, &m_context);
    if (FAILED(result))
        return false;
    IDXGIFactory* displayFactory = nullptr;
    if (SUCCEEDED(m_swapChain->GetParent(__uuidof(IDXGIFactory),
                                         reinterpret_cast<void**>(&displayFactory)))) {
        displayFactory->MakeWindowAssociation(win, DXGI_MWA_NO_ALT_ENTER);
        displayFactory->Release();
    }
    LogAdapter();
    Gdiplus::GdiplusStartupInput startup;
    if (Gdiplus::GdiplusStartup(&m_gdiplusToken, &startup, nullptr) != Gdiplus::Ok)
        return false;
    if (!startup::report(5, "Compiling graphics shaders"))
        return false;
    if (!CreateShaders())
        return false;
    if (!startup::report(30, "Creating graphics buffers and shadows"))
        return false;
    if (!CreateStates() || !CreateTargets(std::max(1, screenW), std::max(1, screenH)))
        return false;
    if (!std::strstr(GetCommandLineA(), "--cpu-skinning") && !CreateSkinResources())
        logging::write("GPU skin resources unavailable; using CPU deformation");
    if (!CreateHeadlightShadowTarget())
        logging::write("Headlight shadow target unavailable; continuing without local shadows");
    if (!CreateStreetShadowTarget())
        logging::write("Streetlight shadow target unavailable; continuing without its shadow");
    if (!m_profiler.Initialize(*m_device))
        logging::write("GPU timestamp queries unavailable");
    m_dynamicVertices = std::make_unique<::rendering::GrowableBuffer<ResourceApi>>(
        *m_device, sizeof(dx11::Vertex), 100000, 3000000);
    m_dynamicInstances = std::make_unique<::rendering::GrowableBuffer<ResourceApi>>(
        *m_device, sizeof(InstanceData), 128);
    m_meshCache = std::make_unique<::rendering::MeshCache<ResourceApi>>(*m_device);
    m_textureLibrary = std::make_unique<::rendering::TextureLibrary<ResourceApi>>(
        *m_device, *m_context, std::strstr(GetCommandLineA(), "--validate-loading") != nullptr);
    std::wstring base = ::rendering::ExecutableFolder();
    if (!startup::report(35, "Loading surface textures"))
        return false;
    std::vector<dx11::texture::Request> surfaceRequests;
    std::vector<ID3D11ShaderResourceView**> surfaceViews;
    auto surface = [&](const std::wstring& file, ID3D11ShaderResourceView** view,
                       dx11::texture::Kind kind = dx11::texture::Kind::Color) {
        surfaceRequests.push_back({file, kind});
        surfaceViews.push_back(view);
    };
    surface(base + L"\\assets\\texture_atlas.png", &m_textures[1]);
    const wchar_t* materials[] = {nullptr,      nullptr,      L"Bricks001", L"Concrete001",
                                  L"Bark001",   L"Fabric001", L"Metal001",  L"Asphalt001",
                                  L"Ground054", L"Grass001",  nullptr};
    for (int i = 2; i < 10; ++i) {
        std::wstring path = base + L"\\assets\\materials\\" + materials[i];
        surface(path + L"_Color.jpg", &m_detailTextures[i]);
        surface(path + L"_NormalDX.jpg", &m_normalTextures[i], dx11::texture::Kind::Normal);
    }
    surface(base + L"\\assets\\models\\baked\\marina\\MarinaFacade_NormalDX.png",
            &m_normalTextures[10], dx11::texture::Kind::Normal);
    if (!::rendering::PreloadTextures(
            surfaceRequests,
            [&](size_t index, const dx11::texture::Prepared& prepared) {
                const auto& request = surfaceRequests[index];
                if (!m_textureLibrary->EnsurePrepared(request.file, request.kind, prepared))
                    return false;
                *surfaceViews[index] = m_textureLibrary->Find(request.file, request.kind);
                return true;
            },
            35, 9, "Loading surface textures"))
        return false;
    m_detailTextures[11] = m_detailTextures[3];
    m_normalTextures[11] = m_normalTextures[3];
    m_detailTextures[12] = m_detailTextures[3];
    m_normalTextures[12] = m_normalTextures[3];
    if (!startup::report(45, "Loading city models and animations"))
        return false;
    dx11::loadMeshes(base + L"\\assets\\models\\baked");
    for (const auto& issue : dx11::assetIssues())
        logging::write(issue.c_str());
    if (!startup::report(55, "Preparing city geometry"))
        return false;
    if (!CreateStaticGeometry())
        return false;
    // A region jump must not synchronously upload dozens of nature and city
    // meshes on its first visible frame. Upload them behind the loading window;
    // the immutable buffers are shared by later instances.
    auto regionalMeshes = dx11::regionalMeshes();
    // Equipping a pickup must not decode its held-weapon texture or create its
    // static buffers on the first gameplay frame that uses that weapon.
    for (const char* name : {"weapons/pistol", "weapons/ak", "weapons/lightning", "weapons/c4",
                             "weapons/remote-trigger", "weapons/grenade", "weapons/smoke-grenade",
                             "weapons/molotov", "weapons/flashbang", "weapons/timed-bomb"})
        if (const auto* weaponMesh = dx11::mesh(name))
            regionalMeshes.push_back(weaponMesh);
    // Gather and deduplicate before workers start. Only the main thread touches
    // renderer caches and D3D; the CPU pipeline holds at most N prepared images.
    std::map<std::wstring, dx11::texture::Request> uniqueTextures;
    auto request = [&](const std::wstring& file, dx11::texture::Kind kind) {
        if (!file.empty() && !m_textureLibrary->Find(file, kind))
            uniqueTextures.emplace(::rendering::TextureKey(file, kind),
                                   dx11::texture::Request{file, kind});
    };
    for (const auto* mesh : regionalMeshes) {
        if (mesh->materialRanges.empty())
            request(mesh->textureFile, mesh->alphaTest ? dx11::texture::Kind::MaskedColor
                                                       : dx11::texture::Kind::Color);
        for (const auto& range : mesh->materialRanges) {
            request(range.baseFile, range.alphaTest ? dx11::texture::Kind::MaskedColor
                                                    : dx11::texture::Kind::Color);
            request(range.normalFile, dx11::texture::Kind::Normal);
            request(range.ormFile, dx11::texture::Kind::Linear);
            request(range.occlusionFile, dx11::texture::Kind::Linear);
            request(range.emissiveFile, dx11::texture::Kind::Color);
        }
    }
    std::vector<dx11::texture::Request> regionalRequests;
    for (const auto& item : uniqueTextures)
        regionalRequests.push_back(item.second);
    if (!::rendering::PreloadTextures(
            regionalRequests,
            [&](size_t index, const dx11::texture::Prepared& prepared) {
                const auto& request = regionalRequests[index];
                return m_textureLibrary->EnsurePrepared(request.file, request.kind, prepared);
            },
            58, 18, "Preparing regional textures"))
        return false;
    size_t uploaded = 0;
    for (const dx11::Mesh* mesh : regionalMeshes) {
        if (!startup::report(76 + int(3 * uploaded / std::max(size_t(1), regionalMeshes.size())),
                             "Uploading regional graphics"))
            return false;
        if (!CacheModel(mesh)) {
            logging::write("Regional resource prewarm incomplete; using on-demand loading");
            break;
        }
        ++uploaded;
    }
    if (!startup::report(80, "Loading HDR lighting"))
        return false;
    const auto& textureStats = m_textureLibrary->Statistics();
    char textureSummary[240]{};
    std::snprintf(textureSummary, sizeof(textureSummary),
                  "Loaded image textures: %u, %.2f MiB payload; block-compressed DDS: %u, %.2f MiB "
                  "(excludes HDR probes and render targets)",
                  textureStats.m_textureCount, double(textureStats.m_payloadBytes) / (1024 * 1024),
                  textureStats.m_compressedCount,
                  double(textureStats.m_compressedBytes) / (1024 * 1024));
    logging::write(textureSummary);
    m_scenePool = std::make_unique<cpu::Pool>(::rendering::SceneWorkers());
    logging::write(("CPU scene workers: " + std::to_string(m_scenePool->concurrency())).c_str());
    return CreateProbes();
}

bool Dx11Renderer::BakeReflectionProbes() {
    m_probeBakeActive = true;
    m_probeBakeFailed = false;
    screenW = screenH = 128;
    ui::shadowQuality = 1;
    ui::antiAliasingQuality = 0;
    ui::graphicsQuality = 2;
    ui::textureQuality = 2;
    ui::filteringQuality = 2;
    ui::reflectionQuality = 0;
    ui::effectsQuality = 0;
    ui::drawDistance = 21;
    ui::lodDistance = 50;
    ui::vegetationDensity = 2;
    ui::grassDistance = 50;
    weather::set("clear");
    worldTime = 0;
    for (m_probeBakeFrame = 0; m_probeBakeFrame < 36 && !m_probeBakeFailed; ++m_probeBakeFrame) {
        // Any early return in render is a failed capture, not a successful bake.
        m_probeBakeFailed = true;
        Render();
    }
    m_probeBakeActive = false;
    if (!m_probeBakeFailed && m_probeBakeFrame == 36) {
        std::ofstream manifest(std::filesystem::path(::rendering::ExecutableFolder()) /
                               "probe-captures" / "capture.json");
        manifest << R"JSON({"schema":1,"faces":36,"size":128,"field_of_view":90,
"hours":[12,18.5,22],"graphics":2,"shadows":1,"textures":2,"filtering":2,
"vegetation":2,"grass_distance":50,"draw_distance":21,"lod_distance":50,
"weather":"clear","world_time":0,"dynamic_geometry":false,"dynamic_lights":false,
"probe_feedback":false,"tone_mapping":false,"coordinates":"D3D cube face orientation; world Y up"}
)JSON";
        m_probeBakeFailed = !manifest;
    }
    return !m_probeBakeFailed && m_probeBakeFrame == 36;
}

void Dx11Renderer::ShutdownRenderer() {
    m_scenePool.reset();
    dx11::shutdownHud();
    if (m_context)
        m_context->ClearState();
    Release(m_cloudNoiseView);
    Release(m_cloudSampler);
    m_profiler.Reset();
    ReleaseTargets();
    Release(m_shadowView);
    for (auto& depth : m_shadowDepth)
        Release(depth);
    Release(m_shadowTexture);
    Release(m_headlightShadowView);
    Release(m_headlightShadowDepth);
    Release(m_headlightShadowTexture);
    Release(m_streetShadowView);
    Release(m_streetShadowDepth);
    Release(m_streetShadowTexture);
    m_shadowSize = m_shadowCascadeCount = 0;
    // These views are borrowed from the texture library.
    std::fill(std::begin(m_textures), std::end(m_textures), nullptr);
    std::fill(std::begin(m_detailTextures), std::end(m_detailTextures), nullptr);
    std::fill(std::begin(m_normalTextures), std::end(m_normalTextures), nullptr);
    Release(m_sampler);
    Release(m_modelSampler);
    Release(m_clampSampler);
    Release(m_shadowSampler);
    Release(m_hudBlend);
    Release(m_alphaBlend);
    Release(m_readDepth);
    Release(m_noDepth);
    m_activeFiltering = -1;
    Release(m_rasterState);
    Release(m_shadowRaster);
    Release(m_toneBuffer);
    Release(m_bloomBuffer);
    Release(m_postBuffer);
    Release(m_sceneBuffer);
    m_dynamicVertices.reset();
    Release(m_staticBuffer);
    Release(m_inputLayout);
    Release(m_probeBuffer);
    Release(m_probeView);
    Release(m_brdfView);
    m_probeSet = {};
    m_probeBakeActive = m_probeBakeFailed = false;
    m_probeBakeFrame = 0;
    m_dynamicInstances.reset();
    Release(m_instanceLayout);
    Release(m_skinOutputUav);
    Release(m_skinOutput);
    Release(m_skinConstants);
    Release(m_skinCS);
    Release(m_skinPreviousUav);
    Release(m_skinPreviousOutput);
    Release(m_skinLayout);
    Release(m_skinVS);
    m_previousSkins.clear();
    m_skinFrame = 0;
    for (auto& item : m_skinSources)
        Release(item.second);
    m_skinSources.clear();
    m_gpuSkins.clear();
    m_skinCapacity = m_skinVertexCount = 0;
    m_skinValidationDone = false;
    m_meshCache.reset();
    m_textureLibrary.reset();
    Release(m_sceneVS);
    Release(m_instanceVS);
    Release(m_scenePS);
    Release(m_alphaShadowPS);
    Release(m_sceneHS);
    Release(m_sceneDS);
    Release(m_hudVS);
    Release(m_hudPS);
    Release(m_postPS);
    Release(m_bloomPS);
    Release(m_reflectionPS);
    Release(m_motionPS);
    Release(m_temporalPS);
    Release(m_temporalCopyPS);
    Release(m_meterPS);
    Release(m_meterReducePS);
    Release(m_exposurePS);
    Release(m_tonePS);
    if (m_swapChain)
        m_swapChain->SetFullscreenState(FALSE, nullptr);
    Release(m_swapChain);
    Release(m_context);
    Release(m_device);
    m_deviceLost = false;
    if (m_gdiplusToken) {
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
        m_gdiplusToken = 0;
    }
}
} // namespace game
