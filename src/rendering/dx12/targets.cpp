#include "renderer.h"

namespace game {
bool Dx12Renderer::CreateTargets(int width, int height) {
    if (width < 1 || height < 1)
        return false;
    const int outputWidth = std::max(1, screenW), outputHeight = std::max(1, screenH);
    if (m_displayW && FAILED(m_device->resize(outputWidth, outputHeight)))
        return false;
    m_displayW = outputWidth;
    m_displayH = outputHeight;
    dx12::Texture2D* backBuffer = nullptr;
    HRESULT result = m_device->backBuffer(&backBuffer);
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_target);
    Release(backBuffer);
    if (FAILED(result))
        return false;
    dx12::TextureDesc sceneDescription{};
    sceneDescription.Width = width;
    sceneDescription.Height = height;
    sceneDescription.MipLevels = 1;
    sceneDescription.ArraySize = 1;
    sceneDescription.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    sceneDescription.SampleDesc.Count = 1;
    sceneDescription.Usage = dx12::Default;
    sceneDescription.BindFlags = dx12::RenderTarget | dx12::ShaderInput;
    result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_sceneTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(m_sceneTexture, nullptr, &m_sceneTarget);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_sceneTexture, nullptr, &m_sceneView);
    if (SUCCEEDED(result))
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_opaqueTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_opaqueTexture, nullptr, &m_opaqueView);
    if (SUCCEEDED(result))
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_surfaceTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(m_surfaceTexture, nullptr, &m_surfaceTarget);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_surfaceTexture, nullptr, &m_surfaceView);
    if (SUCCEEDED(result))
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_indirectTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(m_indirectTexture, nullptr, &m_indirectTarget);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_indirectTexture, nullptr, &m_indirectView);
    if (SUCCEEDED(result))
        result =
            m_device->CreateTexture2D(&sceneDescription, nullptr, &m_reflectionResponseTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(m_reflectionResponseTexture, nullptr,
                                                  &m_reflectionResponseTarget);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_reflectionResponseTexture, nullptr,
                                                    &m_reflectionResponseView);
    if (SUCCEEDED(result))
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_objectMotionTexture);
    if (SUCCEEDED(result))
        result =
            m_device->CreateRenderTargetView(m_objectMotionTexture, nullptr, &m_objectMotionTarget);
    if (SUCCEEDED(result))
        result =
            m_device->CreateShaderResourceView(m_objectMotionTexture, nullptr, &m_objectMotionView);
    if (FAILED(result))
        return false;
    for (int level = 0; level < 3; ++level) {
        sceneDescription.Width = UINT(std::max(1, width >> (level + 1)));
        sceneDescription.Height = UINT(std::max(1, height >> (level + 1)));
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_bloomTexture[level]);
        if (SUCCEEDED(result))
            result = m_device->CreateRenderTargetView(m_bloomTexture[level], nullptr,
                                                      &m_bloomTarget[level]);
        if (SUCCEEDED(result))
            result = m_device->CreateShaderResourceView(m_bloomTexture[level], nullptr,
                                                        &m_bloomView[level]);
        if (FAILED(result))
            return false;
    }
    sceneDescription.Width = UINT(std::max(1, width / 2));
    sceneDescription.Height = UINT(std::max(1, height / 2));
    result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_reflectionTexture);
    if (SUCCEEDED(result))
        result =
            m_device->CreateRenderTargetView(m_reflectionTexture, nullptr, &m_reflectionTarget);
    if (SUCCEEDED(result))
        result =
            m_device->CreateShaderResourceView(m_reflectionTexture, nullptr, &m_reflectionView);
    if (FAILED(result))
        return false;
    sceneDescription.Width = UINT(std::max(1, (width + 1) / 2));
    sceneDescription.Height = UINT(std::max(1, (height + 1) / 2));
    sceneDescription.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_cloudVolumeTexture);
    if (SUCCEEDED(result))
        result =
            m_device->CreateRenderTargetView(m_cloudVolumeTexture, nullptr, &m_cloudVolumeTarget);
    if (SUCCEEDED(result))
        result =
            m_device->CreateShaderResourceView(m_cloudVolumeTexture, nullptr, &m_cloudVolumeView);
    sceneDescription.Format = DXGI_FORMAT_R32_FLOAT;
    if (SUCCEEDED(result))
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_cloudGuideTexture);
    if (SUCCEEDED(result))
        result =
            m_device->CreateRenderTargetView(m_cloudGuideTexture, nullptr, &m_cloudGuideTarget);
    if (SUCCEEDED(result))
        result =
            m_device->CreateShaderResourceView(m_cloudGuideTexture, nullptr, &m_cloudGuideView);
    if (FAILED(result))
        return false;
    sceneDescription.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    sceneDescription.Width = UINT(width);
    sceneDescription.Height = UINT(height);
    result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_postTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(m_postTexture, nullptr, &m_postTarget);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_postTexture, nullptr, &m_postView);
    if (SUCCEEDED(result))
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_motionTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(m_motionTexture, nullptr, &m_motionTarget);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_motionTexture, nullptr, &m_motionView);
    for (int index = 0; index < 2 && SUCCEEDED(result); ++index) {
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_historyTexture[index]);
        if (SUCCEEDED(result))
            result = m_device->CreateRenderTargetView(m_historyTexture[index], nullptr,
                                                      &m_historyTarget[index]);
        if (SUCCEEDED(result))
            result = m_device->CreateShaderResourceView(m_historyTexture[index], nullptr,
                                                        &m_historyView[index]);
    }
    if (SUCCEEDED(result))
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_composedTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(m_composedTexture, nullptr, &m_composedTarget);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_composedTexture, nullptr, &m_composedView);
    sceneDescription.Format = DXGI_FORMAT_R32G32_FLOAT;
    for (unsigned level = 0; level < dx11::tone::meterLevels && SUCCEEDED(result); ++level) {
        sceneDescription.Width = sceneDescription.Height = dx11::tone::meterSize >> level;
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_meterTexture[level]);
        if (SUCCEEDED(result))
            result = m_device->CreateRenderTargetView(m_meterTexture[level], nullptr,
                                                      &m_meterTarget[level]);
        if (SUCCEEDED(result))
            result = m_device->CreateShaderResourceView(m_meterTexture[level], nullptr,
                                                        &m_meterView[level]);
    }
    sceneDescription.Width = sceneDescription.Height = 1;
    for (int index = 0; index < 2 && SUCCEEDED(result); ++index) {
        result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_exposureTexture[index]);
        if (SUCCEEDED(result))
            result = m_device->CreateRenderTargetView(m_exposureTexture[index], nullptr,
                                                      &m_exposureTarget[index]);
        if (SUCCEEDED(result))
            result = m_device->CreateShaderResourceView(m_exposureTexture[index], nullptr,
                                                        &m_exposureView[index]);
        if (SUCCEEDED(result)) {
            const float zero[4]{};
            m_context->ClearRenderTargetView(m_exposureTarget[index], zero);
        }
    }
    if (FAILED(result))
        return false;
    m_exposureValid = false;
    m_exposureIndex = 0;
    m_exposureFrame = 0;
    m_exposureClock = {};
    m_historyValid = false;
    m_historyIndex = 0;
    dx12::TextureDesc depthDescription{};
    depthDescription.Width = width;
    depthDescription.Height = height;
    depthDescription.MipLevels = 1;
    depthDescription.ArraySize = 1;
    depthDescription.Format = DXGI_FORMAT_R32_TYPELESS;
    depthDescription.SampleDesc.Count = 1;
    depthDescription.Usage = dx12::Default;
    depthDescription.BindFlags = dx12::DepthTarget | dx12::ShaderInput;
    result = m_device->CreateTexture2D(&depthDescription, nullptr, &m_depthTexture);
    D3D12_DEPTH_STENCIL_VIEW_DESC depthDesc{};
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    if (SUCCEEDED(result))
        result = m_device->CreateDepthStencilView(m_depthTexture, &depthDesc, &m_depthView);
    D3D12_SHADER_RESOURCE_VIEW_DESC depthResource{};
    depthResource.Format = DXGI_FORMAT_R32_FLOAT;
    depthResource.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthResource.Texture2D.MipLevels = 1;
    if (SUCCEEDED(result))
        result =
            m_device->CreateShaderResourceView(m_depthTexture, &depthResource, &m_depthViewSRV);
    if (FAILED(result))
        return false;
    dx12::TextureDesc reactiveDescription{};
    reactiveDescription.Width = width;
    reactiveDescription.Height = height;
    reactiveDescription.Format = DXGI_FORMAT_R8_UNORM;
    reactiveDescription.BindFlags = dx12::RenderTarget | dx12::ShaderInput;
    if (FAILED(m_device->CreateTexture2D(&reactiveDescription, nullptr, &m_reactiveTexture)) ||
        FAILED(m_device->CreateRenderTargetView(m_reactiveTexture, nullptr, &m_reactiveTarget)) ||
        FAILED(m_device->CreateShaderResourceView(m_reactiveTexture, nullptr, &m_reactiveView)))
        return false;
    dx12::TextureDesc hudDescription{};
    hudDescription.Width = m_displayW;
    hudDescription.Height = m_displayH;
    hudDescription.MipLevels = 1;
    hudDescription.ArraySize = 1;
    hudDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    hudDescription.SampleDesc.Count = 1;
    hudDescription.Usage = dx12::Dynamic;
    hudDescription.BindFlags = dx12::ShaderInput;
    hudDescription.CPUAccessFlags = dx12::Write;
    result = m_device->CreateTexture2D(&hudDescription, nullptr, &m_hudTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_hudTexture, nullptr, &m_hudView);
    if (FAILED(result))
        return false;
    D3D12_VIEWPORT viewport{};
    viewport.Width = float(width);
    viewport.Height = float(height);
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    m_context->RSSetViewports(1, &viewport);
    m_bufferW = width;
    m_bufferH = height;
    m_hudPixels.resize(size_t(m_displayW) * m_displayH * 4);
    return true;
}

bool Dx12Renderer::CreateStates() {
    auto noise = dx11::sky::noiseVolume();
    dx12::VolumeDesc volume{};
    volume.Width = volume.Height = volume.Depth = dx11::sky::noiseSize;
    volume.MipLevels = 1;
    volume.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    volume.Usage = dx12::Immutable;
    volume.BindFlags = dx12::ShaderInput;
    dx12::InitialData noiseData{noise.data(), dx11::sky::noiseSize * 4,
                                dx11::sky::noiseSize * dx11::sky::noiseSize * 4};
    dx12::Texture3D* noiseTexture = nullptr;
    HRESULT noiseResult = m_device->CreateTexture3D(&volume, &noiseData, &noiseTexture);
    if (SUCCEEDED(noiseResult))
        noiseResult = m_device->CreateShaderResourceView(noiseTexture, nullptr, &m_cloudNoiseView);
    Release(noiseTexture);
    if (FAILED(noiseResult))
        return false;
    D3D12_SAMPLER_DESC noiseSampling{};
    noiseSampling.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    noiseSampling.AddressU = noiseSampling.AddressV = noiseSampling.AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    noiseSampling.MaxLOD = D3D12_FLOAT32_MAX;
    if (FAILED(m_device->CreateSamplerState(&noiseSampling, &m_cloudSampler)))
        return false;
    dx12::BufferDesc constant{};
    constant.ByteWidth = sizeof(SceneConstants);
    constant.Usage = dx12::Default;
    constant.BindFlags = dx12::Constant;
    if (FAILED(m_device->CreateBuffer(&constant, nullptr, &m_sceneBuffer)))
        return false;
    constant.ByteWidth = sizeof(PostConstants);
    if (FAILED(m_device->CreateBuffer(&constant, nullptr, &m_postBuffer)))
        return false;
    constant.ByteWidth = sizeof(dx11::tone::Constants);
    if (FAILED(m_device->CreateBuffer(&constant, nullptr, &m_toneBuffer)))
        return false;
    constant.ByteWidth = sizeof(XMFLOAT4);
    if (FAILED(m_device->CreateBuffer(&constant, nullptr, &m_bloomBuffer)))
        return false;
    D3D12_RASTERIZER_DESC raster{};
    raster.FillMode = D3D12_FILL_MODE_SOLID;
    raster.CullMode = D3D12_CULL_MODE_NONE;
    raster.DepthClipEnable = TRUE;
    if (FAILED(m_device->CreateRasterizerState(&raster, &m_rasterState)))
        return false;
    raster.DepthBias = 300;
    raster.SlopeScaledDepthBias = 1.5f;
    if (FAILED(m_device->CreateRasterizerState(&raster, &m_shadowRaster)))
        return false;
    D3D12_SAMPLER_DESC comparison{};
    comparison.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    comparison.AddressU = comparison.AddressV = comparison.AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    comparison.BorderColor[0] = comparison.BorderColor[1] = comparison.BorderColor[2] =
        comparison.BorderColor[3] = 1;
    comparison.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    comparison.MaxLOD = D3D12_FLOAT32_MAX;
    if (FAILED(m_device->CreateSamplerState(&comparison, &m_shadowSampler)))
        return false;
    D3D12_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    if (FAILED(m_device->CreateDepthStencilState(&depth, &m_noDepth)))
        return false;
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    if (FAILED(m_device->CreateDepthStencilState(&depth, &m_readDepth)))
        return false;
    D3D12_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    // Scene alpha stores the reflection mask. Transparent effects must keep
    // the mask beneath them instead of turning their soft edges reflective.
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_device->CreateBlendState(&blend, &m_alphaBlend)))
        return false;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    if (FAILED(m_device->CreateBlendState(&blend, &m_hudBlend)))
        return false;
    D3D12_SAMPLER_DESC sample{};
    sample.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sample.AddressU = sample.AddressV = sample.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sample.MaxLOD = D3D12_FLOAT32_MAX;
    return SUCCEEDED(m_device->CreateSamplerState(&sample, &m_clampSampler));
}

bool Dx12Renderer::UpdateTextureSampler() {
    int requested = ui::textureQuality * 3 + ui::filteringQuality + m_fsr.mode * 9;
    if (m_sampler && requested == m_activeFiltering)
        return true;
    D3D12_SAMPLER_DESC sample{};
    sample.Filter = D3D12_FILTER_ANISOTROPIC;
    sample.AddressU = sample.AddressV = sample.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sample.MaxAnisotropy = ui::filteringQuality == 2 ? 16 : ui::filteringQuality == 1 ? 8 : 4;
    sample.MinLOD = float(2 - ui::textureQuality);
    sample.MipLODBias =
        m_fsr.active ? std::log2(float(m_fsr.renderWidth) / m_fsr.width) - 1.0f : 0.0f;
    sample.MaxLOD = D3D12_FLOAT32_MAX;
    dx12::SamplerState* replacement = nullptr;
    if (FAILED(m_device->CreateSamplerState(&sample, &replacement)))
        return false;
    sample.AddressU = sample.AddressV = sample.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    dx12::SamplerState* modelReplacement = nullptr;
    if (FAILED(m_device->CreateSamplerState(&sample, &modelReplacement))) {
        Release(replacement);
        return false;
    }
    Release(m_sampler);
    m_sampler = replacement;
    m_activeFiltering = requested;
    Release(m_modelSampler);
    m_modelSampler = modelReplacement;
    return true;
}

bool Dx12Renderer::CreateShadowTargets(int size, int layers) {
    dx12::ShaderResourceView* empty = nullptr;
    m_context->PSSetShaderResources(4, 1, &empty);
    auto clear = [&]() {
        Release(m_shadowView);
        for (auto& depth : m_shadowDepth)
            Release(depth);
        Release(m_shadowTexture);
        m_shadowSize = m_shadowCascadeCount = 0;
    };
    clear();
    if (size == 0 || layers == 0)
        return true;
    dx12::TextureDesc texture{};
    texture.Width = texture.Height = UINT(size);
    texture.MipLevels = 1;
    texture.ArraySize = UINT(layers);
    texture.Format = DXGI_FORMAT_R32_TYPELESS;
    texture.SampleDesc.Count = 1;
    texture.Usage = dx12::Default;
    texture.BindFlags = dx12::DepthTarget | dx12::ShaderInput;
    if (FAILED(m_device->CreateTexture2D(&texture, nullptr, &m_shadowTexture)))
        return false;
    D3D12_DEPTH_STENCIL_VIEW_DESC depth{};
    depth.Format = DXGI_FORMAT_D32_FLOAT;
    depth.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    depth.Texture2DArray.ArraySize = 1;
    for (int layer = 0; layer < layers; ++layer) {
        depth.Texture2DArray.FirstArraySlice = UINT(layer);
        if (FAILED(
                m_device->CreateDepthStencilView(m_shadowTexture, &depth, &m_shadowDepth[layer]))) {
            clear();
            return false;
        }
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_R32_FLOAT;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    view.Texture2DArray.MipLevels = 1;
    view.Texture2DArray.ArraySize = UINT(layers);
    if (FAILED(m_device->CreateShaderResourceView(m_shadowTexture, &view, &m_shadowView))) {
        clear();
        return false;
    }
    m_shadowSize = size;
    m_shadowCascadeCount = layers;
    return true;
}

bool Dx12Renderer::CreateHeadlightShadowTarget() {
    constexpr UINT size = 1024;
    dx12::TextureDesc texture{};
    texture.Width = texture.Height = size;
    texture.MipLevels = texture.ArraySize = 1;
    texture.Format = DXGI_FORMAT_R32_TYPELESS;
    texture.SampleDesc.Count = 1;
    texture.Usage = dx12::Default;
    texture.BindFlags = dx12::DepthTarget | dx12::ShaderInput;
    HRESULT result = m_device->CreateTexture2D(&texture, nullptr, &m_headlightShadowTexture);
    if (SUCCEEDED(result)) {
        D3D12_DEPTH_STENCIL_VIEW_DESC depth{};
        depth.Format = DXGI_FORMAT_D32_FLOAT;
        depth.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        result = m_device->CreateDepthStencilView(m_headlightShadowTexture, &depth,
                                                  &m_headlightShadowDepth);
    }
    if (SUCCEEDED(result)) {
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = DXGI_FORMAT_R32_FLOAT;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels = 1;
        result = m_device->CreateShaderResourceView(m_headlightShadowTexture, &view,
                                                    &m_headlightShadowView);
    }
    if (FAILED(result)) {
        Release(m_headlightShadowView);
        Release(m_headlightShadowDepth);
        Release(m_headlightShadowTexture);
    }
    return SUCCEEDED(result);
}

bool Dx12Renderer::CreateStreetShadowTarget() {
    constexpr UINT size = 512;
    dx12::TextureDesc texture{};
    texture.Width = texture.Height = size;
    texture.MipLevels = texture.ArraySize = 1;
    texture.Format = DXGI_FORMAT_R32_TYPELESS;
    texture.SampleDesc.Count = 1;
    texture.Usage = dx12::Default;
    texture.BindFlags = dx12::DepthTarget | dx12::ShaderInput;
    HRESULT result = m_device->CreateTexture2D(&texture, nullptr, &m_streetShadowTexture);
    if (SUCCEEDED(result)) {
        D3D12_DEPTH_STENCIL_VIEW_DESC depth{};
        depth.Format = DXGI_FORMAT_D32_FLOAT;
        depth.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        result =
            m_device->CreateDepthStencilView(m_streetShadowTexture, &depth, &m_streetShadowDepth);
    }
    if (SUCCEEDED(result)) {
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = DXGI_FORMAT_R32_FLOAT;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels = 1;
        result =
            m_device->CreateShaderResourceView(m_streetShadowTexture, &view, &m_streetShadowView);
    }
    if (FAILED(result)) {
        Release(m_streetShadowView);
        Release(m_streetShadowDepth);
        Release(m_streetShadowTexture);
    }
    return SUCCEEDED(result);
}

void Dx12Renderer::ReleaseTargets() {
    if (!m_context)
        return;
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    dx12::ShaderResourceView* nullViews[13]{};
    m_context->PSSetShaderResources(0, 13, nullViews);
    for (int index = 0; index < 2; ++index) {
        Release(m_historyView[index]);
        Release(m_historyTarget[index]);
        Release(m_historyTexture[index]);
    }
    Release(m_motionView);
    Release(m_motionTarget);
    Release(m_motionTexture);
    Release(m_objectMotionView);
    Release(m_objectMotionTarget);
    Release(m_objectMotionTexture);
    Release(m_postView);
    Release(m_postTarget);
    Release(m_postTexture);
    Release(m_composedView);
    Release(m_composedTarget);
    Release(m_composedTexture);
    for (unsigned level = 0; level < dx11::tone::meterLevels; ++level) {
        Release(m_meterView[level]);
        Release(m_meterTarget[level]);
        Release(m_meterTexture[level]);
    }
    for (int index = 0; index < 2; ++index) {
        Release(m_exposureView[index]);
        Release(m_exposureTarget[index]);
        Release(m_exposureTexture[index]);
    }
    m_exposureValid = false;
    Release(m_reflectionView);
    Release(m_reflectionTarget);
    Release(m_reflectionTexture);
    Release(m_cloudVolumeView);
    Release(m_cloudVolumeTarget);
    Release(m_cloudVolumeTexture);
    Release(m_cloudGuideView);
    Release(m_cloudGuideTarget);
    Release(m_cloudGuideTexture);
    for (int level = 0; level < 3; ++level) {
        Release(m_bloomView[level]);
        Release(m_bloomTarget[level]);
        Release(m_bloomTexture[level]);
    }
    Release(m_reactiveView);
    Release(m_reactiveTarget);
    Release(m_reactiveTexture);
    Release(m_opaqueView);
    Release(m_opaqueTexture);
    Release(m_hudView);
    Release(m_hudTexture);
    Release(m_depthViewSRV);
    Release(m_depthView);
    Release(m_depthTexture);
    Release(m_indirectView);
    Release(m_indirectTarget);
    Release(m_indirectTexture);
    Release(m_reflectionResponseView);
    Release(m_reflectionResponseTarget);
    Release(m_reflectionResponseTexture);
    Release(m_surfaceView);
    Release(m_surfaceTarget);
    Release(m_surfaceTexture);
    Release(m_sceneView);
    Release(m_sceneTarget);
    Release(m_sceneTexture);
    Release(m_target);
    m_historyValid = false;
}
} // namespace game
