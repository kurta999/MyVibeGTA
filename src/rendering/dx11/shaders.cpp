#include "renderer.h"
#include "../shaders/shared_shaders.h"
#include "../shaders/dx11_shaders.h"

namespace game {
using namespace ::rendering::shaders;

bool Dx11Renderer::CreateShaders() {
    const std::string sceneSource =
        std::string(probeShader) + sceneShaderPrelude + sceneShaderPixel;
    const std::string reflectionSource = std::string(probeShader) + reflectionShader;
    std::vector<dx11::shader::Request> requests = {{sceneSource, "VSSkinned", "vs_5_0"},
                                                   {sceneSource, "VS", "vs_5_0"},
                                                   {sceneSource, "PS", "ps_5_0"},
                                                   {sceneSource, "VSInstanced", "vs_5_0"},
                                                   {sceneSource, "PSShadowAlpha", "ps_5_0"},
                                                   {sceneSource, "HS", "hs_5_0"},
                                                   {sceneSource, "DS", "ds_5_0"},
                                                   {hudShader, "VS", "vs_5_0"},
                                                   {hudShader, "PS", "ps_5_0"},
                                                   {postShader, "PS", "ps_5_0"},
                                                   {bloomShader, "PS", "ps_5_0"},
                                                   {reflectionSource, "PS", "ps_5_0"},
                                                   {motionShader, "PS", "ps_5_0"},
                                                   {temporalShader, "PS", "ps_5_0"},
                                                   {dx11::tone::shader, "CopyPS", "ps_5_0"},
                                                   {dx11::tone::shader, "MeterPS", "ps_5_0"},
                                                   {dx11::tone::shader, "ReducePS", "ps_5_0"},
                                                   {dx11::tone::shader, "ExposurePS", "ps_5_0"},
                                                   {dx11::tone::shader, "TonePS", "ps_5_0"}};
    std::vector<dx11::shader::Compiled> compiled;
    dx11::shader::Stats stats;
    bool ok = dx11::shader::compileBatch(
        requests, ::rendering::LoadingWorkers(),
        [](size_t done, size_t total) {
            return startup::report(5 + int(25 * done / std::max(size_t(1), total)),
                                   "Compiling graphics shaders");
        },
        compiled, stats);
    char timing[200]{};
    std::snprintf(
        timing, sizeof(timing),
        "Shader loading: workers %u, peak active %u, completed %zu/%zu, wall %.3f s, CPU %.3f s%s",
        stats.workers, stats.peakActive, stats.completed, requests.size(), stats.wallSeconds,
        stats.cpuSeconds, ok ? "" : " (stopped)");
    logging::write(timing);
    if (!ok) {
        for (size_t i = 0; i < compiled.size(); ++i)
            if (!compiled[i].error.empty()) {
                std::ofstream log("shader-error.log", std::ios::app);
                if (log)
                    log << requests[i].entry << ": " << compiled[i].error << '\n';
                logging::write("Graphics shader compilation failed; see shader-error.log");
                if (!std::strstr(GetCommandLineA(), "--smoke"))
                    MessageBoxA(win, compiled[i].error.c_str(), "Direct3D shader compile error",
                                MB_ICONERROR);
                break;
            }
        return false;
    }
    if (std::strstr(GetCommandLineA(), "--validate-loading")) {
        std::uint64_t checksum = 14695981039346656037ULL;
        for (const auto& item : compiled) {
            const auto* bytes = static_cast<const unsigned char*>(item.blob->GetBufferPointer());
            for (size_t i = 0; i < item.blob->GetBufferSize(); ++i)
                checksum = (checksum ^ bytes[i]) * 1099511628211ULL;
        }
        std::snprintf(timing, sizeof(timing), "Shader checksum: %016llx",
                      static_cast<unsigned long long>(checksum));
        logging::write(timing);
    }
    ID3DBlob *skinned = nullptr, *vs = nullptr, *instanced = nullptr, *ps = nullptr,
             *shadowAlpha = nullptr, *hull = nullptr, *domain = nullptr, *hudVertex = nullptr,
             *hudPixel = nullptr, *postPixel = nullptr, *bloomPixel = nullptr,
             *reflectionPixel = nullptr, *motionPixel = nullptr, *temporalPixel = nullptr,
             *temporalCopyPixel = nullptr;
    ID3DBlob *meterPixel = nullptr, *reducePixel = nullptr, *exposurePixel = nullptr,
             *tonePixel = nullptr;
    ID3DBlob** destinations[] = {&skinned,
                                 &vs,
                                 &ps,
                                 &instanced,
                                 &shadowAlpha,
                                 &hull,
                                 &domain,
                                 &hudVertex,
                                 &hudPixel,
                                 &postPixel,
                                 &bloomPixel,
                                 &reflectionPixel,
                                 &motionPixel,
                                 &temporalPixel,
                                 &temporalCopyPixel,
                                 &meterPixel,
                                 &reducePixel,
                                 &exposurePixel,
                                 &tonePixel};
    for (size_t i = 0; i < compiled.size(); ++i) {
        *destinations[i] = compiled[i].blob.get();
        (*destinations[i])->AddRef();
    }
    HRESULT result = m_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(),
                                                  nullptr, &m_sceneVS);
    if (SUCCEEDED(result))
        result = m_device->CreateVertexShader(skinned->GetBufferPointer(), skinned->GetBufferSize(),
                                              nullptr, &m_skinVS);
    if (SUCCEEDED(result))
        result = m_device->CreateVertexShader(instanced->GetBufferPointer(),
                                              instanced->GetBufferSize(), nullptr, &m_instanceVS);
    if (SUCCEEDED(result))
        result = m_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr,
                                             &m_scenePS);
    if (SUCCEEDED(result))
        result =
            m_device->CreatePixelShader(shadowAlpha->GetBufferPointer(),
                                        shadowAlpha->GetBufferSize(), nullptr, &m_alphaShadowPS);
    if (SUCCEEDED(result))
        result = m_device->CreateHullShader(hull->GetBufferPointer(), hull->GetBufferSize(),
                                            nullptr, &m_sceneHS);
    if (SUCCEEDED(result))
        result = m_device->CreateDomainShader(domain->GetBufferPointer(), domain->GetBufferSize(),
                                              nullptr, &m_sceneDS);
    if (SUCCEEDED(result))
        result = m_device->CreateVertexShader(hudVertex->GetBufferPointer(),
                                              hudVertex->GetBufferSize(), nullptr, &m_hudVS);
    if (SUCCEEDED(result))
        result = m_device->CreatePixelShader(hudPixel->GetBufferPointer(),
                                             hudPixel->GetBufferSize(), nullptr, &m_hudPS);
    if (SUCCEEDED(result))
        result = m_device->CreatePixelShader(postPixel->GetBufferPointer(),
                                             postPixel->GetBufferSize(), nullptr, &m_postPS);
    if (SUCCEEDED(result))
        result = m_device->CreatePixelShader(bloomPixel->GetBufferPointer(),
                                             bloomPixel->GetBufferSize(), nullptr, &m_bloomPS);
    if (SUCCEEDED(result))
        result =
            m_device->CreatePixelShader(reflectionPixel->GetBufferPointer(),
                                        reflectionPixel->GetBufferSize(), nullptr, &m_reflectionPS);
    if (SUCCEEDED(result))
        result = m_device->CreatePixelShader(motionPixel->GetBufferPointer(),
                                             motionPixel->GetBufferSize(), nullptr, &m_motionPS);
    if (SUCCEEDED(result))
        result =
            m_device->CreatePixelShader(temporalPixel->GetBufferPointer(),
                                        temporalPixel->GetBufferSize(), nullptr, &m_temporalPS);
    if (SUCCEEDED(result))
        result = m_device->CreatePixelShader(temporalCopyPixel->GetBufferPointer(),
                                             temporalCopyPixel->GetBufferSize(), nullptr,
                                             &m_temporalCopyPS);
    if (SUCCEEDED(result))
        result = m_device->CreatePixelShader(meterPixel->GetBufferPointer(),
                                             meterPixel->GetBufferSize(), nullptr, &m_meterPS);
    if (SUCCEEDED(result))
        result =
            m_device->CreatePixelShader(reducePixel->GetBufferPointer(),
                                        reducePixel->GetBufferSize(), nullptr, &m_meterReducePS);
    if (SUCCEEDED(result))
        result =
            m_device->CreatePixelShader(exposurePixel->GetBufferPointer(),
                                        exposurePixel->GetBufferSize(), nullptr, &m_exposurePS);
    if (SUCCEEDED(result))
        result = m_device->CreatePixelShader(tonePixel->GetBufferPointer(),
                                             tonePixel->GetBufferSize(), nullptr, &m_tonePS);
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    if (SUCCEEDED(result))
        result = m_device->CreateInputLayout(layout, 4, vs->GetBufferPointer(), vs->GetBufferSize(),
                                             &m_inputLayout);
    D3D11_INPUT_ELEMENT_DESC skinElements[5]{};
    std::copy(std::begin(layout), std::end(layout), skinElements);
    skinElements[4] = {
        "POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
    if (SUCCEEDED(result))
        result = m_device->CreateInputLayout(skinElements, 5, skinned->GetBufferPointer(),
                                             skinned->GetBufferSize(), &m_skinLayout);
    D3D11_INPUT_ELEMENT_DESC instanceElements[] = {
        {"INSTANCE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1}};
    D3D11_INPUT_ELEMENT_DESC fullLayout[9]{};
    std::copy(std::begin(layout), std::end(layout), fullLayout);
    std::copy(std::begin(instanceElements), std::end(instanceElements), fullLayout + 4);
    if (SUCCEEDED(result))
        result = m_device->CreateInputLayout(fullLayout, 9, instanced->GetBufferPointer(),
                                             instanced->GetBufferSize(), &m_instanceLayout);
    Release(vs);
    Release(instanced);
    Release(ps);
    Release(shadowAlpha);
    Release(hull);
    Release(domain);
    Release(hudVertex);
    Release(hudPixel);
    Release(postPixel);
    Release(bloomPixel);
    Release(reflectionPixel);
    Release(motionPixel);
    Release(temporalPixel);
    Release(temporalCopyPixel);
    Release(meterPixel);
    Release(reducePixel);
    Release(exposurePixel);
    Release(tonePixel);
    Release(skinned);
    return SUCCEEDED(result);
}
} // namespace game
