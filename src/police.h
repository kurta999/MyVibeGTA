#pragma once
#include "game.h"
#include <string>

namespace police {
enum class Crime { Threat, Gunfire, Assault, Murder, Theft, CarTheft, Arson, AttackOfficer };
bool load(const char* path=nullptr);
const std::string& lastError();
void reset();
void setWantedLevel(int level);
int wantedLevel();
bool report(Crime crime,game::Vec2 place,bool silent=false,bool directWitness=false);
void update(float dt);
}
