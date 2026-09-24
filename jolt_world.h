#pragma once
#include "game.h"
#include <cstddef>

namespace jolt_world {
void reset();
void shutdown();
void step(float dt);
void impulse(std::size_t index,game::Vec3 value);
void remove(std::size_t index);
void spawnRagdoll(const game::Ped& ped,game::Vec3 impulse);
}
