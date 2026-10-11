#pragma once
#include "game.h"
namespace props {
bool load(const char* path=nullptr);
const std::string& lastError();
void reset();
void update(float dt);
bool hit(game::Vec3 point,game::Vec3 impulse,int damage);
}
