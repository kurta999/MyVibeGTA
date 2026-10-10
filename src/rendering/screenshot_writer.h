#pragma once
#include <string>

namespace rendering {
// Converts a mapped BGRA image to a uniquely named PNG. GPU readback remains
// the backend's responsibility; this class owns only capture naming/encoding.
class ScreenshotWriter final {
  public:
    bool SavePng(void* pixels, unsigned rowPitch, unsigned width, unsigned height,
                 std::string& filename);

  private:
    unsigned m_sequence = 0;
};
} // namespace rendering
