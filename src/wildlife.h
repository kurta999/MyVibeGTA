#pragma once
#include "game.h"

namespace wildlife {
enum class State { Idle, Wander, Play, Flee, Attack, Dead };
struct Species {
    const char* id; const char* name;
    float width,height,length,speed; int health,damage,loot;
    bool defensive,predator; int preySize;
};
struct Animal {
    std::string id; int species=0; game::Vec2 p{},home{},target{},threat{};
    float angle=0,phase=0,timer=0,alert=0,cooldown=0,hit=0,attackTime=0;
    int health=0,peer=-1; State state=State::Idle;
    bool looted=false,carried=false,playerThreat=false;
};
extern std::vector<Animal> animals;
const std::vector<Species>& species();
void reset();
void update(float dt);
float radius(const Animal& animal);
bool walkable(game::Vec2 point,float clearance,bool allowRoad=false);
bool riding();
int mountedIndex();
int nearbyMount();
bool mount(int index);
bool dismount(bool force=false);
void updateRider(float dt);
void hurt(int index,int damage,game::Vec2 attacker,bool playerCaused,int source=-1);
void scare(game::Vec2 origin,float range);
int meleeTarget(float range);
bool hit(game::Vec3 point,int damage,game::Vec2 origin,bool playerCaused);
int nearbyCorpse(bool unlooted);
void loot(int index);
bool carryDrop();
bool carrying();
void clearCarry();
int activeCount();
}
