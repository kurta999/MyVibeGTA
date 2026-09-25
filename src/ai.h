#pragma once

#include "game.h"

namespace ai {
void update(float dt);
int activeCount();
void notifyGunshot(game::Vec2 origin);
int notifyThreat(game::Vec2 origin,float facing);
void reactToHit(game::Ped& pedestrian,game::Vec2 threat);
bool vehicleImpact(game::Ped& pedestrian,const game::Vehicle& vehicle);
}
