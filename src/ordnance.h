#pragma once
#include "game.h"
#include "weapons.h"

namespace ordnance {
constexpr int MAX_C4=40;
struct Device {
    weapons::Payload kind=weapons::Payload::None;
    game::Vec3 p{},v{},normal{0,1,0},local{},localNormal{0,1,0};
    float fuse=3,radius=0;int damage=0;
    bool settled=false;int vehicle=-1;
};
struct Smoke {game::Vec3 p;float life=24,radius=90;bool dark=false;};
extern std::vector<Device> devices;
extern std::vector<Smoke> smoke;
extern float flash;
void reset();
bool use(const weapons::Stats& stats);
void detonateRemote();
void update(float dt);
int c4Count();
bool obscured(game::Vec2 start,game::Vec2 end);
bool timerOpen();
const std::string& timerText();
void timerKey(int key);
}
