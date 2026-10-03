#pragma once
#include "game.h"
namespace grapple {
void reset();
bool fire();
void release();
bool active();
bool update(float dt);
game::Vec3 anchor();
game::Vec3 hook();
float cooldown();
}
