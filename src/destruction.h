#pragma once
#include "game.h"
namespace destruction {
struct Box {game::Vec3 low,high;};
std::vector<Box> boxes(const game::Building& building);
bool contains(const game::Building& building,game::Vec3 point,float pad=0);
bool segment(const game::Building& building,game::Vec3 start,game::Vec3 end,float& entry);
bool cut(std::size_t index,Box volume);
void blast(game::Vec3 point,float radius);
}
