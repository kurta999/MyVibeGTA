#pragma once
#include "game.h"
#include <string>
#include <vector>

namespace mission_script {
bool Load(const char* path, std::vector<game::MissionDef>& definitions);
const std::string& LastError();
void Start();
int StartCandidate();
void Update(float dt);
void TargetHit();
bool IsTarget();
const char* Objective();
int Next();
}
