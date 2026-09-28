#pragma once
#include <cstdint>
#include <atomic>
#include <vector>

namespace dx11::texture {
enum class Kind { Color, MaskedColor, Linear, Normal };
struct Level {
    unsigned width=0,height=0;
    // Windows BGRA8 pixels, tightly packed.
    std::vector<std::uint8_t> pixels;
};
std::vector<Level> generate(unsigned width,unsigned height,
    const std::uint8_t* bgra,Kind kind,const std::atomic<bool>* cancelled=nullptr);
}
