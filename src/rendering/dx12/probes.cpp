#include "renderer.h"

namespace game {
bool Dx12Renderer::CreateProbes() {
    dx12::BufferDesc buffer{};
    buffer.ByteWidth = sizeof(ProbeConstants);
    buffer.Usage = dx12::Default;
    buffer.BindFlags = dx12::Constant;
    if (FAILED(m_device->CreateBuffer(&buffer, nullptr, &m_probeBuffer)))
        return false;
    std::string error;
    if (!dx11::probes::load(::rendering::ExecutableFolder() + L"\\assets\\lighting\\showcase.mcpb",
                            m_probeSet, error)) {
        logging::write((error + "; retaining legacy ambient lighting").c_str());
        return true;
    }
    std::vector<dx12::InitialData> data(m_probeSet.subresources.size());
    for (unsigned i = 0; i < data.size(); ++i) {
        data[i].pSysMem = m_probeSet.subresources[i].data();
        data[i].SysMemPitch = std::max(1u, m_probeSet.size >> (i % m_probeSet.mips)) * 8;
    }
    dx12::TextureDesc texture{};
    texture.Width = texture.Height = m_probeSet.size;
    texture.MipLevels = m_probeSet.mips;
    texture.ArraySize = dx11::probes::CUBE_COUNT * 6;
    texture.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texture.SampleDesc.Count = 1;
    texture.Usage = dx12::Immutable;
    texture.BindFlags = dx12::ShaderInput;
    texture.MiscFlags = dx12::Cube;
    dx12::Texture2D* resource = nullptr;
    HRESULT result = m_device->CreateTexture2D(&texture, data.data(), &resource);
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = texture.Format;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
    view.TextureCubeArray.MipLevels = m_probeSet.mips;
    view.TextureCubeArray.NumCubes = dx11::probes::CUBE_COUNT;
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(resource, &view, &m_probeView);
    Release(resource);
    texture.Width = texture.Height = m_probeSet.lutSize;
    texture.MipLevels = texture.ArraySize = 1;
    texture.Format = DXGI_FORMAT_R32G32_FLOAT;
    texture.MiscFlags = 0;
    dx12::InitialData lut{};
    lut.pSysMem = m_probeSet.brdf.data();
    lut.SysMemPitch = m_probeSet.lutSize * 2 * sizeof(float);
    if (SUCCEEDED(result))
        result = m_device->CreateTexture2D(&texture, &lut, &resource);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(resource, nullptr, &m_brdfView);
    Release(resource);
    if (FAILED(result)) {
        Release(m_probeView);
        Release(m_brdfView);
        m_probeSet = {};
        logging::write("Probe resource allocation failed; retaining legacy ambient lighting");
    } else {
        // Keep the compact SH/position metadata; release CPU texture copies.
        m_probeSet.subresources.clear();
        m_probeSet.brdf.clear();
        logging::write(
            "HDR probes loaded: two local positions, day/dusk/night, nine cubes and BRDF LUT");
    }
    return true;
}

void Dx12Renderer::UpdateProbes() {
    ProbeConstants constants{};
    bool enabled = m_probeView && m_brdfView && !m_probeBakeActive &&
                   !std::strstr(GetCommandLineA(), "--no-probes");
    const auto weights = dx11::probes::timeWeights(gameHour);
    constants.m_timeWeights = {weights[0], weights[1], weights[2], 0};
    constants.m_info = {float(m_probeSet.mips ? m_probeSet.mips - 1 : 0),
                        1 - weather::current().clouds * 0.42f, enabled ? 1.0f : 0.0f,
                        std::strstr(GetCommandLineA(), "--probe-weight-view") ? 2.0f
                        : std::strstr(GetCommandLineA(), "--probe-view")      ? 1.0f
                                                                              : 0.0f};
    for (unsigned p = 0; p < 2; ++p) {
        const auto& v = enabled
                            ? m_probeSet.positions[p]
                            : std::array<float, 4>{m_probePositions[p].x, m_probePositions[p].y,
                                                   m_probePositions[p].z, m_probePositions[p].w};
        constants.m_positions[p] = {v[0], v[1], v[2], v[3]};
    }
    for (unsigned provider = 0; provider < 3; ++provider)
        for (unsigned i = 0; i < 9; ++i) {
            XMFLOAT4& sh = constants.m_sh[provider][i];
            unsigned first = provider == 0 ? 6 : (provider - 1) * 3;
            for (unsigned time = 0; time < 3; ++time) {
                const auto& c = m_probeSet.sh[first + time][i];
                sh.x += c[0] * weights[time];
                sh.y += c[1] * weights[time];
                sh.z += c[2] * weights[time];
            }
        }
    m_context->UpdateSubresource(m_probeBuffer, 0, nullptr, &constants, 0, 0);
    m_context->PSSetConstantBuffers(1, 1, &m_probeBuffer);
    dx12::ShaderResourceView* views[] = {m_probeView, m_brdfView};
    m_context->PSSetShaderResources(11, 2, views);
    m_context->PSSetSamplers(3, 1, &m_clampSampler);
}

bool Dx12Renderer::CaptureProbe(const SceneConstants& constants) {
    dx12::Texture2D *colorCopy = nullptr, *depthCopy = nullptr;
    dx12::TextureDesc desc{};
    m_sceneTexture->GetDesc(&desc);
    desc.Usage = dx12::Readback;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = dx12::Read;
    desc.MiscFlags = 0;
    HRESULT result = m_device->CreateTexture2D(&desc, nullptr, &colorCopy);
    m_depthTexture->GetDesc(&desc);
    desc.Usage = dx12::Readback;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = dx12::Read;
    desc.MiscFlags = 0;
    if (SUCCEEDED(result))
        result = m_device->CreateTexture2D(&desc, nullptr, &depthCopy);
    dx12::MappedData color{}, depth{};
    bool colorMapped = false, depthMapped = false;
    if (SUCCEEDED(result)) {
        if (!m_context)
            return false;
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_context->CopyResource(colorCopy, m_sceneTexture);
        m_context->CopyResource(depthCopy, m_depthTexture);
        result = m_context->Map(colorCopy, 0, dx12::MapRead, 0, &color);
        colorMapped = SUCCEEDED(result);
        if (SUCCEEDED(result)) {
            result = m_context->Map(depthCopy, 0, dx12::MapRead, 0, &depth);
            depthMapped = SUCCEEDED(result);
        }
    }
    bool ok = false;
    if (SUCCEEDED(result)) {
        unsigned probe = m_probeBakeFrame / 18, state = (m_probeBakeFrame / 6) % 3,
                 face = m_probeBakeFrame % 6;
        std::vector<std::uint16_t> pixels(size_t(m_bufferW) * m_bufferH * 4);
        XMMATRIX inverse = XMMatrixInverse(nullptr, XMLoadFloat4x4(&constants.m_viewProjection));
        float solar = std::sin((gameHour - 6) * PI / 12),
              day = std::clamp(solar * 2.3f + 0.42f, 0.0f, 1.0f);
        float twilight = std::max(0.0f, 1.0f - std::abs(solar) * 4.0f);
        float top[3] = {0.015f + 0.10f * day + twilight * 0.10f,
                        0.025f + 0.32f * day - twilight * 0.07f, 0.09f + 0.60f * day};
        float horizon[3] = {0.055f + 0.55f * day + twilight * 0.34f,
                            0.075f + 0.68f * day - twilight * 0.18f,
                            0.15f + 0.69f * day - twilight * 0.27f};
        for (int y = 0; y < m_bufferH; ++y) {
            auto* source = reinterpret_cast<const std::uint16_t*>(
                static_cast<const char*>(color.pData) + size_t(y) * color.RowPitch);
            auto* depths = reinterpret_cast<const std::uint32_t*>(
                static_cast<const char*>(depth.pData) + size_t(y) * depth.RowPitch);
            for (int x = 0; x < m_bufferW; ++x) {
                auto* output = pixels.data() + (size_t(y) * m_bufferW + x) * 4;
                std::copy_n(source + x * 4, 4, output);
                output[3] = DirectX::PackedVector::XMConvertFloatToHalf(1);
                if ((depths[x] & 0xffffff) >= 0xfffff0) {
                    XMVECTOR farPoint =
                        XMVector3TransformCoord(XMVectorSet((x + 0.5f) * 2 / m_bufferW - 1,
                                                            1 - (y + 0.5f) * 2 / m_bufferH, 1, 1),
                                                inverse);
                    XMVECTOR ray = XMVector3Normalize(
                        XMVectorSubtract(farPoint, XMVectorSet(constants.m_eye.x, constants.m_eye.y,
                                                               constants.m_eye.z, 1)));
                    float gradient = std::clamp(0.34f + XMVectorGetY(ray) * 1.8f, 0.0f, 1.0f);
                    for (int c = 0; c < 3; ++c)
                        output[c] = DirectX::PackedVector::XMConvertFloatToHalf(
                            std::max(0.0f, horizon[c] * (1 - gradient) + top[c] * gradient));
                }
            }
        }
        auto folder = std::filesystem::path(::rendering::ExecutableFolder()) / "probe-captures";
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        char filename[48]{};
        std::snprintf(filename, sizeof(filename), "probe-%u-time-%u-face-%u.hdrface", probe, state,
                      face);
        std::ofstream file(folder / filename, std::ios::binary);
        const std::uint32_t header[] = {unsigned(m_bufferW), probe, state, face};
        file.write("MCENV1\0\0", 8);
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write(reinterpret_cast<const char*>(&m_probePositions[probe]), sizeof(XMFLOAT4));
        file.write(reinterpret_cast<const char*>(&gameHour), sizeof(float));
        file.write(reinterpret_cast<const char*>(pixels.data()),
                   std::streamsize(pixels.size() * 2));
        ok = bool(file);
        if (ok)
            logging::write(("Captured HDR " + std::string(filename)).c_str());
    }
    if (depthMapped)
        m_context->Unmap(depthCopy, 0);
    if (colorMapped)
        m_context->Unmap(colorCopy, 0);
    Release(depthCopy);
    Release(colorCopy);
    return ok;
}
} // namespace game
