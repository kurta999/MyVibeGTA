#pragma once
#include "game.h"

namespace ped_navigation {
// Bound expensive searches per AI tick; unserved pedestrians wait safely.
void beginFrame();
game::Vec2 velocity(game::Ped& ped,game::Vec2 goal,float speed,float dt);
void clear(game::Ped& ped);
}
