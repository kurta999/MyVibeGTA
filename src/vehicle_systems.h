#pragma once
#include "game.h"
namespace vehicle_systems {
struct Part {game::Vec3 center,size;game::Color color{1,1,1};int shape=0;std::string mesh;int spin=0;};
std::vector<Part> parts(const game::Vehicle& vehicle);
game::RagdollPart partRotation(const game::Vehicle& vehicle,const Part& part);
game::Vec3 partPosition(const game::Vehicle& vehicle,const Part& part);
const char* name(game::Kind kind);
bool aircraft(game::Kind kind);
bool pedal(game::Kind kind);
void update(float dt);
void fireTank();
void toggleTrailer();
}
