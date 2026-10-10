#pragma once
#include "../dx11_texture_mips.h"
#include <string>

namespace rendering {
inline std::wstring TextureKey(const std::wstring& file, dx11::texture::Kind kind) {
    return file + L"#" + std::to_wstring(int(kind));
}
} // namespace rendering
