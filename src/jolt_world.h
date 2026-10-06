#pragma once
#include "game.h"
#include <cstddef>
namespace wildlife {struct Animal;}

namespace jolt_world {
void reset();
void shutdown();
void step(float dt);
void impulse(std::size_t index,game::Vec3 value);
void remove(std::size_t index);
bool spawnRagdoll(const game::Ped& ped,game::Vec3 impulse,
    const game::Vec2* pinAnchor=nullptr,bool fallen=false);
// Returns true for an actual live/captured physical pose; legacy bounds are
// still provided when false. Interactions must use the body's current height.
bool corpsePose(const game::Ped& ped,game::Vec3& low,game::Vec3& high,game::Vec3& contact);
bool corpseDropClear(game::Vec3 feet,float angle);
void removeRagdoll(const std::string& pedId);
void clearRagdolls();
void moveCharacter(game::Vec2 horizontal,bool jump,float dt);
void moveGrappleCharacter(game::Vec3 velocity,float dt);
bool staticAnchor(game::Vec3 origin,game::Vec3 direction,float range,game::Vec3& point);
void teleportCharacter(game::Vec2 position,float height);
void movePed(std::size_t index,game::Vec2 horizontal,float dt);
float pedHeight(std::size_t index);
float pedHeight(const game::Ped& ped);
void teleportPed(std::size_t index,game::Vec2 position,float height);
// Navigation uses the live static collision, including multiple walkable
// floors at the same XZ and the exact pedestrian capsule's head clearance.
void preparePedNavigation();
std::vector<float> pedestrianFloors(game::Vec2 point,unsigned* queryCount=nullptr,unsigned queryLimit=~0u);
bool pedestrianClear(game::Vec3 feet,bool includeDynamic=false);
bool standingCharacterClear(game::Vec3 feet);
bool animalClear(const wildlife::Animal& animal,game::Vec3 feet,float angle);
std::vector<float> animalFloors(const wildlife::Animal& animal,game::Vec2 point,float angle);
void moveAnimal(wildlife::Animal& animal,game::Vec2 velocity,float dt);
void addPed();
void driveVehicle(std::size_t index,float throttle,float steering,float dt,bool brake=false);
void flyHelicopter(std::size_t index,float throttle,float steering,float lift,float dt);
void flyAirplane(std::size_t index,float throttle,float roll,float pitch,float rudder,float dt);
void vehicleImpulse(std::size_t index,game::Vec3 impulse);
bool toggleTrailer(std::size_t index);
void breakVehicle(std::size_t index,bool fragments=true);
void rebuildBuilding(std::size_t index);
// Refresh editable scenery without resetting vehicles, props or ragdolls.
void refreshScenery();
struct SceneryRecovery {unsigned props=0,ragdolls=0,corpses=0,releasedPins=0,unresolved=0;};
// Reconcile loose actors once against the incoming F5 layer, without advancing time.
SceneryRecovery reconcileLooseActors();
const SceneryRecovery& lastSceneryRecovery();
std::size_t activeBuilderColliderCount();
void spawnFragment(game::Vec3 p,game::Vec3 size,game::Vec3 velocity,game::Color color,int shape=0,int masonryKind=-1);
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
