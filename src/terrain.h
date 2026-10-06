#pragma once
#include "game.h"
#include <cstdint>
#include <string>
#include <vector>

// The rendered surface, Jolt heightfield and ground queries share this grid.
namespace terrain {
constexpr float spacing=50;
constexpr unsigned samples=340; // Jolt block size 4, covers the 16,800-unit map.
struct Landform {std::string id;game::Vec2 center;float rx,rz,height;};
bool load(const char* path=nullptr);
const std::string& lastError();
const std::vector<Landform>& landforms();
const std::vector<float>& heights();
std::uint64_t revision();
float height(game::Vec2 point);
float baseHeight(game::Vec2 point);
bool contains(game::Vec3 point);
game::Vec3 normal(game::Vec2 point);
bool segmentHit(game::Vec3 start,game::Vec3 end,float& fraction);
}
