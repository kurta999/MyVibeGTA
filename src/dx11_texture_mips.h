#pragma once
#include <cstdint>
#include <atomic>
#include <vector>

namespace dx11::texture {
enum class Kind { Color, MaskedColor, Linear, Normal };
struct Level {
    unsigned width=0,height=0;
    // Tightly packed pixels or DDS blocks; Prepared supplies the format.
    std::vector<std::uint8_t> pixels;
};
std::vector<Level> generate(unsigned width,unsigned height,
    const std::uint8_t* bgra,Kind kind,const std::atomic<bool>* cancelled=nullptr);
}
