#pragma once
#include "game.h"
#include <cstddef>

namespace jolt_world {
void reset();
void shutdown();
void step(float dt);
void impulse(std::size_t index,game::Vec3 value);
void remove(std::size_t index);
void spawnRagdoll(const game::Ped& ped,game::Vec3 impulse,
    const game::Vec2* pinAnchor=nullptr);
void removeRagdoll(const std::string& pedId);
void clearRagdolls();
void moveCharacter(game::Vec2 horizontal,bool jump,float dt);
void moveGrappleCharacter(game::Vec3 velocity,float dt);
bool staticAnchor(game::Vec3 origin,game::Vec3 direction,float range,game::Vec3& point);
void teleportCharacter(game::Vec2 position,float height);
void movePed(std::size_t index,game::Vec2 horizontal,float dt);
void addPed();
void driveVehicle(std::size_t index,float throttle,float steering,float dt,bool brake=false);
void flyHelicopter(std::size_t index,float throttle,float steering,float lift,float dt);
void flyAirplane(std::size_t index,float throttle,float roll,float pitch,float rudder,float dt);
void vehicleImpulse(std::size_t index,game::Vec3 impulse);
bool toggleTrailer(std::size_t index);
void breakVehicle(std::size_t index,bool fragments=true);
void rebuildBuilding(std::size_t index);
void spawnFragment(game::Vec3 p,game::Vec3 size,game::Vec3 velocity,game::Color color,int shape=0);
void teleportVehicle(std::size_t index,game::Vec2 position,float angle,float altitude=0);
void stopVehicle(std::size_t index);
void coastVehicle(std::size_t index,float dt);
float treeRadius(const game::Tree& tree);
struct TreeFragment {
    game::Vec3 p{},size{};
    float qx=0,qy=0,qz=0,qw=1;
    bool foliage=false;
    int category=0,shape=0;game::Color color{1,1,1};std::string mesh;
};
const std::vector<TreeFragment>& treeFragments();
std::size_t activeTreeColliderCount();
std::size_t activeAnimalColliderCount();
int wheelContactCount(std::size_t index);
std::size_t activeBuildingColliderCount();
std::size_t activePedCharacterCount();
}
