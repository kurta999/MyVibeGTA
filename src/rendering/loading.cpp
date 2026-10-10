#include "loading.h"
#include "../logging.h"
#include "../startup.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <wrl/client.h>

namespace rendering {
std::wstring ExecutableFolder() {
    wchar_t filename[MAX_PATH]{};
    GetModuleFileNameW(nullptr, filename, MAX_PATH);
    std::wstring result(filename);
    auto slash = result.find_last_of(L"\\/");
    return result.substr(0, slash);
}

unsigned LoadingWorkers() {
    const char* option = std::strstr(GetCommandLineA(), "--loader-workers=");
    if (!option)
        return dx11::texture::defaultLoadingWorkers();
    char* end = nullptr;
    auto value = std::strtoul(option + 17, &end, 10);
    return end != option + 17 && value >= 1 && value <= 8 ? unsigned(value)
                                                          : dx11::texture::defaultLoadingWorkers();
}

unsigned SceneWorkers() {
    const char* option = std::strstr(GetCommandLineA(), "--scene-workers=");
    if (!option)
        return dx11::texture::defaultLoadingWorkers();
    char* end = nullptr;
    auto value = std::strtoul(option + 16, &end, 10);
    return end != option + 16 && value >= 1 && value <= 8 ? unsigned(value)
                                                          : dx11::texture::defaultLoadingWorkers();
}

bool PreloadTextures(const std::vector<dx11::texture::Request>& requests,
                     const std::function<bool(size_t, const dx11::texture::Prepared&)>& consume,
                     int start, int span, const char* stage) {
    dx11::texture::LoadingStats stats;
    const bool validate = std::strstr(GetCommandLineA(), "--validate-loading") != nullptr;
    std::uint64_t checksum = 14695981039346656037ULL;
    bool ok = dx11::texture::prepareBatch(
        requests, LoadingWorkers(),
        [&](size_t index, const dx11::texture::Prepared& prepared) {
            if (!prepared.error.empty() || prepared.levels.empty()) {
                logging::write(("Texture preparation failed: " +
                                std::filesystem::path(requests[index].file).u8string() + ": " +
                                prepared.error)
                                   .c_str());
                return false;
            }
            if (validate)
                for (const auto& level : prepared.levels)
                    for (auto byte : level.pixels)
                        checksum = (checksum ^ byte) * 1099511628211ULL;
            return consume(index, prepared);
        },
        [&](size_t done, size_t total) {
            return startup::report(start + int(span * done / std::max(size_t(1), total)), stage);
        },
        stats);
    char line[320]{};
    std::snprintf(line, sizeof(line),
                  "Texture loading: %s; workers %u, peak active %u, completed %zu/%zu, pending <= "
                  "%zu, wall %.3f s, decode CPU %.3f s, mip CPU %.3f s, upload %.3f s%s",
                  stage, stats.workers, stats.peakActive, stats.completed, requests.size(),
                  stats.peakPending, stats.wallSeconds, stats.decodeSeconds, stats.mipSeconds,
                  stats.consumeSeconds, ok ? "" : " (stopped)");
    logging::write(line);
    if (validate) {
        std::snprintf(line, sizeof(line), "Texture checksum: %s %016llx", stage,
                      static_cast<unsigned long long>(checksum));
        logging::write(line);
    }
    return ok;
}

bool CompileShader(HWND window, const char* source, const char* entry, const char* profile,
                   ID3DBlob** output) {
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT status = D3DCompile(source, std::strlen(source), "MiniCityShader", nullptr, nullptr,
                                entry, profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, output, &errors);
    if (FAILED(status) && errors) {
        std::ofstream log("shader-error.log", std::ios::app);
        if (log)
            log << entry << ": " << static_cast<const char*>(errors->GetBufferPointer()) << '\n';
        if (std::strstr(GetCommandLineA(), "--smoke") == nullptr)
            MessageBoxA(window, static_cast<const char*>(errors->GetBufferPointer()),
                        "Direct3D shader compile error", MB_ICONERROR);
    }
    return SUCCEEDED(status);
}

} // namespace rendering
