#pragma once
#include "game.h"
#include <cstdint>
namespace dx11::grass {
enum class Surface { None, Lawn, Meadow, Savanna, Desert, Snow, Coastal };
struct Profile {
    const char* mesh;
    float coverage,width,height;
};
Surface surfaceAt(game::Vec2 point);
const Profile& profile(Surface surface);
std::uint32_t hash(int x,int z,int variant);
float unit(std::uint32_t value);
// Ring spacing is fixed in world space; LOD and density do not move the roots.
float ringFade(float distance,int ring,float radius);
}
