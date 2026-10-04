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
bool bulletSolidSegment(Vec3 start,Vec3 end,Vec3& impact);
bool bulletCylinderSegment(Vec3 start,Vec3 end,Vec2 center,float radius,float low,float high,float& entry);
void explodeAt(Vec3 point,float radius,int damage,bool playerCaused);
void spawnDebris(const Ped& ped,Vec3 impulse);
}
