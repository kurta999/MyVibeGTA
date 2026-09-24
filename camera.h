#pragma once
#include "game.h"

namespace camera {
struct Pose { game::Vec3 eye,target; };
Pose compute(game::Vec2 focus,float playerHeight,bool aiming,int occupied);
game::Vec3 traceReticle(const Pose& pose,float maximumDistance);
}
