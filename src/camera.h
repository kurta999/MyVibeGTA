#pragma once
#include "game.h"

namespace camera {
struct Pose { game::Vec3 eye,target; };
Pose compute(game::Vec2 focus,float playerHeight,bool aiming,int occupied);
// Render-only pose: same as compute, but eases the third-person camera back
// out after a wall or obstacle stops blocking it. Gameplay traces keep using
// compute so aiming stays exact.
Pose renderPose(game::Vec2 focus,float playerHeight,bool aiming,int occupied);
float fieldOfView();
bool isScoped();
bool zoomActive();
bool firstPersonActive();
int scopeMagnification();
game::Vec3 traceReticle(const Pose& pose,float maximumDistance);
game::Vec3 weaponMuzzle(game::Vec2 player,float height,float yaw,game::Vec3 target);
}
