#pragma once
#include "game.h"
#include <string>
#include <vector>

namespace traversal {
struct Ladder {std::string id,buildingId;float offset=0;game::Vec2 bottom,roof;
    float height=0;};
struct ClimbTree {std::string id,treeId;game::Vec2 bottom;float height=0;
    int treeIndex=-1;bool generated=false;};
extern std::vector<Ladder> ladders;
extern std::vector<ClimbTree> trees;
bool load(const char* path=nullptr);
const std::string& lastError();
void reset();
bool active();
bool startLadder(int index);
bool startTree(int index);
void detach();
void update(float dt);
}
