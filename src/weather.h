#pragma once
#include "game.h"
#include <string>
#include <vector>

namespace weather {
struct State {std::string id,name;float clouds=0,precipitation=0,visibility=1;
    game::Vec2 wind{};float duration=90;bool snow=false;};
bool load(const char* path=nullptr);
const std::string& lastError();
void reset();
void update(float dt);
bool set(const std::string& id,float remaining=-1);
const State& current();
float remaining();
const std::vector<State>& states();
}
