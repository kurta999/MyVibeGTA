#pragma once

#include "game.h"

namespace ai {
// The game tick opens a shared navigation frame before dispatch and AI. Direct
// AI callers retain the convenience of opening their own frame by default.
void update(float dt,bool beginNavigationFrame=true);
int activeCount();
void notifyGunshot(game::Vec2 origin);
int notifyThreat(game::Vec2 origin,float facing);
void reactToHit(game::Ped& pedestrian,game::Vec2 threat);
bool vehicleImpact(game::Ped& pedestrian,const game::Vehicle& vehicle);
void pedestrianContact(game::Ped& pedestrian,float speed,game::Vec2 from);
bool talkToPed(int index);
bool startSocial(int first,int second,bool fight);
}
