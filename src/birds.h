#pragma once
#include "game.h"

namespace birds {
struct Species {const char* id;float span,height,length,speed;int health;};
struct Bird {
    std::string id;int species=0,health=0;
    game::Vec3 p{},home{},velocity{},previous{};
    float phase=0,angle=0,turn=0,planTime=0,hit=0;
    bool settled=false;
};
extern std::vector<Bird> flock;
const std::array<Species,5>& species();
float radius(const Bird& bird);
// The same animated-wing envelope is used by reticle picking and projectiles.
bool segmentHit(const Bird& bird,game::Vec3 from,game::Vec3 to,float& entry,bool swept=false);
void reset();
void update(float dt);
bool clearFlight(game::Vec3 from,game::Vec3 to,float clearance);
float landingHeight(game::Vec3 position,float clearance);
void hurt(int index,int damage);
bool hit(game::Vec3 point,int damage);
int activeCount();
}
