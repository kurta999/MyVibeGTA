#pragma once
#include "texture_key.h"
#include "../dx11_texture_loading.h"
#include <d3dcompiler.h>
#include <functional>
#include <string>
#include <vector>

namespace rendering {
std::wstring ExecutableFolder();
unsigned LoadingWorkers();
unsigned SceneWorkers();
bool PreloadTextures(const std::vector<dx11::texture::Request>& requests,
                     const std::function<bool(size_t, const dx11::texture::Prepared&)>& consume,
                     int start, int span, const char* stage);
bool CompileShader(HWND window, const char* source, const char* entry, const char* profile,
                   ID3DBlob** output);
} // namespace rendering
