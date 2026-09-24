#pragma once

#include "game.h"

namespace ai {
void update(float dt);
int activeCount();
void notifyGunshot(game::Vec2 origin);
void reactToHit(game::Ped& pedestrian,game::Vec2 threat);
}
