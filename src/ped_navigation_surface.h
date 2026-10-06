#pragma once
#include "game.h"

namespace ped_navigation_surface {
struct Stats {unsigned plans=0,expanded=0,physicsQueries=0;};
void beginFrame();
game::Vec2 velocity(game::Ped& ped,game::Vec2 goal,float goalHeight,float speed,float dt);
const Stats& stats();
// Select a fully supported, reachable destination, retaining its real floor
// and route. An enclosed intended point may resolve to a reachable approach.
// Cover additionally requires that the final destination hides the body.
bool destination(game::Ped& ped,game::Vec2 intended,game::Vec3& feet,const game::Vec3* threatEye=nullptr);
bool hasDestination(const game::Ped& ped);
// Dispatch shares the same exact capsule, footprint and frame budgets. Unlike
// destination(), reachable() requires the complete endpoint, not an approach.
bool supported(game::Vec2 point,float referenceHeight,game::Vec3& feet,float tolerance=4000);
bool reachable(game::Vec3 from,game::Vec3 to);
}
