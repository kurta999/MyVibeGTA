#pragma once
#include "game.h"

namespace game {
float randf(float low,float high);
int randi(int count);
void announce(const std::string& message,float seconds);
bool solid(Vec2 point,float radius);
Vec2 randomWalkable();
void move(Vec2& point,Vec2 delta,float radius,Kind kind);
bool clearLine(Vec2 from,Vec2 to);
void spawnDebris(const Ped& ped,Vec3 impulse);
}
