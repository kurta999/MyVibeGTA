#pragma once
#include "game.h"

// DX11/Jolt traffic. Index references are valid for the lifetime of a populated
// world (pedestrians, including police reinforcements, are appended, not erased).
namespace traffic {
void reset();
void update(float dt);
void provoke(game::Ped& ped,game::Vec2 origin);
void damaged(int vehicle,float amount);
void carjacked(int vehicle);
bool updatePed(game::Ped& ped,float dt);
void release(game::Ped& ped);
void afterLoad();
std::size_t roadNodeCount();
}
