#include "renderer.h"

namespace game {
bool Dx11Renderer::CreateTargets(int width, int height) {
    if (width < 1 || height < 1)
        return false;
    if (m_bufferW && FAILED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN,
                                                       DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)))
        return false;
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT result =
        m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_target);
    Release(backBuffer);
    if (FAILED(result))
        return false;
    D3D11_TEXTURE2D_DESC sceneDescription{};
    sceneDescription.Width = width;
    sceneDescription.Height = height;
    sceneDescription.MipLevels = 1;
    sceneDescription.ArraySize = 1;
    sceneDescription.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    sceneDescription.SampleDesc.Count = 1;
    sceneDescription.Usage = D3D11_USAGE_DEFAULT;
    sceneDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    result = m_device->CreateTexture2D(&sceneDescription, nullptr, &m_sceneTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateRenderTargetView(m_sceneTexture, nullptr, &m_sceneTarget);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_sceneTexture, nullptr, &m_sceneView);
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
    D3D11_TEXTURE2D_DESC depthDescription{};
    depthDescription.Width = width;
    depthDescription.Height = height;
    depthDescription.MipLevels = 1;
    depthDescription.ArraySize = 1;
    depthDescription.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthDescription.SampleDesc.Count = 1;
    depthDescription.Usage = D3D11_USAGE_DEFAULT;
    depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    result = m_device->CreateTexture2D(&depthDescription, nullptr, &m_depthTexture);
    D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc{};
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (SUCCEEDED(result))
        result = m_device->CreateDepthStencilView(m_depthTexture, &depthDesc, &m_depthView);
    D3D11_SHADER_RESOURCE_VIEW_DESC depthResource{};
    depthResource.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthResource.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    depthResource.Texture2D.MipLevels = 1;
    if (SUCCEEDED(result))
        result =
            m_device->CreateShaderResourceView(m_depthTexture, &depthResource, &m_depthViewSRV);
    if (FAILED(result))
        return false;
    D3D11_TEXTURE2D_DESC hudDescription{};
    hudDescription.Width = width;
    hudDescription.Height = height;
    hudDescription.MipLevels = 1;
    hudDescription.ArraySize = 1;
    hudDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    hudDescription.SampleDesc.Count = 1;
    hudDescription.Usage = D3D11_USAGE_DYNAMIC;
    hudDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    hudDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_device->CreateTexture2D(&hudDescription, nullptr, &m_hudTexture);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(m_hudTexture, nullptr, &m_hudView);
    if (FAILED(result))
        return false;
    D3D11_VIEWPORT viewport{};
    viewport.Width = float(width);
    viewport.Height = float(height);
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    m_context->RSSetViewports(1, &viewport);
    m_bufferW = width;
    m_bufferH = height;
    m_hudPixels.resize(size_t(width) * height * 4);
    return true;
}

bool Dx11Renderer::CreateStates() {
    auto noise = dx11::sky::noiseVolume();
    D3D11_TEXTURE3D_DESC volume{};
    volume.Width = volume.Height = volume.Depth = dx11::sky::noiseSize;
    volume.MipLevels = 1;
    volume.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    volume.Usage = D3D11_USAGE_IMMUTABLE;
    volume.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA noiseData{noise.data(), dx11::sky::noiseSize * 4,
                                     dx11::sky::noiseSize * dx11::sky::noiseSize * 4};
    ID3D11Texture3D* noiseTexture = nullptr;
    HRESULT noiseResult = m_device->CreateTexture3D(&volume, &noiseData, &noiseTexture);
    if (SUCCEEDED(noiseResult))
        noiseResult = m_device->CreateShaderResourceView(noiseTexture, nullptr, &m_cloudNoiseView);
    Release(noiseTexture);
    if (FAILED(noiseResult))
        return false;
    D3D11_SAMPLER_DESC noiseSampling{};
    noiseSampling.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    noiseSampling.AddressU = noiseSampling.AddressV = noiseSampling.AddressW =
        D3D11_TEXTURE_ADDRESS_WRAP;
    noiseSampling.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_device->CreateSamplerState(&noiseSampling, &m_cloudSampler)))
        return false;
    D3D11_BUFFER_DESC constant{};
    constant.ByteWidth = sizeof(SceneConstants);
    constant.Usage = D3D11_USAGE_DEFAULT;
    constant.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
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
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = TRUE;
    if (FAILED(m_device->CreateRasterizerState(&raster, &m_rasterState)))
        return false;
    raster.DepthBias = 300;
    raster.SlopeScaledDepthBias = 1.5f;
    if (FAILED(m_device->CreateRasterizerState(&raster, &m_shadowRaster)))
        return false;
    D3D11_SAMPLER_DESC comparison{};
    comparison.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    comparison.AddressU = comparison.AddressV = comparison.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    comparison.BorderColor[0] = comparison.BorderColor[1] = comparison.BorderColor[2] =
        comparison.BorderColor[3] = 1;
    comparison.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    comparison.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_device->CreateSamplerState(&comparison, &m_shadowSampler)))
        return false;
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if (FAILED(m_device->CreateDepthStencilState(&depth, &m_noDepth)))
        return false;
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(m_device->CreateDepthStencilState(&depth, &m_readDepth)))
        return false;
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    // Scene alpha stores the reflection mask. Transparent effects must keep
    // the mask beneath them instead of turning their soft edges reflective.
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_device->CreateBlendState(&blend, &m_alphaBlend)))
        return false;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    if (FAILED(m_device->CreateBlendState(&blend, &m_hudBlend)))
        return false;
    D3D11_SAMPLER_DESC sample{};
    sample.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sample.AddressU = sample.AddressV = sample.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sample.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(m_device->CreateSamplerState(&sample, &m_clampSampler));
}

bool Dx11Renderer::UpdateTextureSampler() {
    int requested = ui::textureQuality * 3 + ui::filteringQuality;
    if (m_sampler && requested == m_activeFiltering)
        return true;
    D3D11_SAMPLER_DESC sample{};
    sample.Filter = D3D11_FILTER_ANISOTROPIC;
    sample.AddressU = sample.AddressV = sample.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sample.MaxAnisotropy = ui::filteringQuality == 2 ? 16 : ui::filteringQuality == 1 ? 8 : 4;
    sample.MinLOD = float(2 - ui::textureQuality);
    sample.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState* replacement = nullptr;
    if (FAILED(m_device->CreateSamplerState(&sample, &replacement)))
        return false;
    sample.AddressU = sample.AddressV = sample.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ID3D11SamplerState* modelReplacement = nullptr;
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

bool Dx11Renderer::CreateShadowTargets(int size, int layers) {
    ID3D11ShaderResourceView* empty = nullptr;
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
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width = texture.Height = UINT(size);
    texture.MipLevels = 1;
    texture.ArraySize = UINT(layers);
    texture.Format = DXGI_FORMAT_R32_TYPELESS;
    texture.SampleDesc.Count = 1;
    texture.Usage = D3D11_USAGE_DEFAULT;
    texture.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(m_device->CreateTexture2D(&texture, nullptr, &m_shadowTexture)))
        return false;
    D3D11_DEPTH_STENCIL_VIEW_DESC depth{};
    depth.Format = DXGI_FORMAT_D32_FLOAT;
    depth.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    depth.Texture2DArray.ArraySize = 1;
    for (int layer = 0; layer < layers; ++layer) {
        depth.Texture2DArray.FirstArraySlice = UINT(layer);
        if (FAILED(
                m_device->CreateDepthStencilView(m_shadowTexture, &depth, &m_shadowDepth[layer]))) {
            clear();
            return false;
        }
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_R32_FLOAT;
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
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

bool Dx11Renderer::CreateHeadlightShadowTarget() {
    constexpr UINT size = 1024;
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width = texture.Height = size;
    texture.MipLevels = texture.ArraySize = 1;
    texture.Format = DXGI_FORMAT_R32_TYPELESS;
    texture.SampleDesc.Count = 1;
    texture.Usage = D3D11_USAGE_DEFAULT;
    texture.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    HRESULT result = m_device->CreateTexture2D(&texture, nullptr, &m_headlightShadowTexture);
    if (SUCCEEDED(result)) {
        D3D11_DEPTH_STENCIL_VIEW_DESC depth{};
        depth.Format = DXGI_FORMAT_D32_FLOAT;
        depth.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        result = m_device->CreateDepthStencilView(m_headlightShadowTexture, &depth,
                                                  &m_headlightShadowDepth);
    }
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = DXGI_FORMAT_R32_FLOAT;
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
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

bool Dx11Renderer::CreateStreetShadowTarget() {
    constexpr UINT size = 512;
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width = texture.Height = size;
    texture.MipLevels = texture.ArraySize = 1;
    texture.Format = DXGI_FORMAT_R32_TYPELESS;
    texture.SampleDesc.Count = 1;
    texture.Usage = D3D11_USAGE_DEFAULT;
    texture.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    HRESULT result = m_device->CreateTexture2D(&texture, nullptr, &m_streetShadowTexture);
    if (SUCCEEDED(result)) {
        D3D11_DEPTH_STENCIL_VIEW_DESC depth{};
        depth.Format = DXGI_FORMAT_D32_FLOAT;
        depth.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        result =
            m_device->CreateDepthStencilView(m_streetShadowTexture, &depth, &m_streetShadowDepth);
    }
    if (SUCCEEDED(result)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = DXGI_FORMAT_R32_FLOAT;
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
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

void Dx11Renderer::ReleaseTargets() {
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* nullViews[13]{};
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
    for (int level = 0; level < 3; ++level) {
        Release(m_bloomView[level]);
        Release(m_bloomTarget[level]);
        Release(m_bloomTexture[level]);
    }
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
