#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dx11::probes {
constexpr unsigned LOCAL_COUNT=2, TIME_COUNT=3, CUBE_COUNT=9;
struct Set {
    unsigned size=0,mips=0,lutSize=0;
    std::array<std::array<float,4>,LOCAL_COUNT> positions{};
    // Local probe/time first, then three global sky cubes. SH coefficients
    // include cosine convolution and 1/pi for Lambertian diffuse radiance.
    std::array<std::array<std::array<float,4>,9>,CUBE_COUNT> sh{};
    // D3D subresource order: cube, face (+X,-X,+Y,-Y,+Z,-Z), mip.
    std::vector<std::vector<std::uint16_t>> subresources;
    std::vector<float> brdf;
};
bool load(const std::wstring& path,Set& result,std::string& error);
std::array<float,3> timeWeights(float hour);
}
