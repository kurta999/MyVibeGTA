#pragma once
#include "game.h"
#include <cstddef>

namespace jolt_world {
void reset();
void shutdown();
void step(float dt);
void impulse(std::size_t index,game::Vec3 value);
void remove(std::size_t index);
void spawnRagdoll(const game::Ped& ped,game::Vec3 impulse,
    const game::Vec2* pinAnchor=nullptr);
void removeRagdoll(const std::string& pedId);
void clearRagdolls();
void moveCharacter(game::Vec2 horizontal,bool jump,float dt);
void teleportCharacter(game::Vec2 position,float height);
void movePed(std::size_t index,game::Vec2 horizontal,float dt);
void addPed();
void driveVehicle(std::size_t index,float throttle,float steering,float dt,bool brake=false);
void teleportVehicle(std::size_t index,game::Vec2 position,float angle);
int wheelContactCount(std::size_t index);
std::size_t activeBuildingColliderCount();
std::size_t activePedCharacterCount();
}
