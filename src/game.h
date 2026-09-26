#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace game {
#ifdef MINI_CITY_JOLT
constexpr float PLAYER_MAX_HEALTH=400.0f;
#else
constexpr float PLAYER_MAX_HEALTH=100.0f;
#endif
constexpr float PI=3.14159265359f;
constexpr float WORLD_W=2400,WORLD_D=2200,BEACH_START=1600,SHORE=1850,ROAD_W=110;
struct Vec2{float x=0,z=0;};
inline Vec2 operator+(Vec2 a,Vec2 b){return {a.x+b.x,a.z+b.z};}
inline Vec2 operator-(Vec2 a,Vec2 b){return {a.x-b.x,a.z-b.z};}
inline Vec2 operator*(Vec2 a,float n){return {a.x*n,a.z*n};}
inline float len(Vec2 a){return std::sqrt(a.x*a.x+a.z*a.z);}
inline Vec2 norm(Vec2 a){float n=len(a);return n>0.001f?a*(1/n):Vec2{};}
inline Vec2 forward(float a){return {std::cos(a),std::sin(a)};}
struct Vec3{float x=0,y=0,z=0;};
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(Vec3 a,float n){return {a.x*n,a.y*n,a.z*n};}
inline float len(Vec3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
inline Vec3 norm(Vec3 a){float n=len(a);return n>0.001f?a*(1/n):Vec3{};}
struct Color{float r,g,b;};
inline Color rgb(int r,int g,int b){return {r/255.0f,g/255.0f,b/255.0f};}
inline void color(Color c,float light=1){glColor3f(c.r*light,c.g*light,c.b*light);}
struct Building{float x,z,w,d,h;Color c;std::string id;};
struct Tree{Vec2 p;bool palm=false;int variant=0;float scale=1;
    int health=100;bool burning=false,destroyed=false;std::string id;
    std::string modelId;float crownWidth=54,height=68,climbHeight=0;};
enum class PedState{Wander,Investigate,Flee,TakeCover,Defend,Attack};
struct Ped{Vec2 p,target;float speed,angle,respawn=0;bool alive=true;Color shirt;int health=100;float panic=0;int style=0;bool armed=false,hostile=false;float fireCooldown=0,hitFlash=0;
    PedState state=PedState::Wander;float alertTime=0;int armor=0,maxArmor=0;Vec2 knockback{};
    Vec2 lastKnown{};float sightMemory=0,tacticTimer=0;int burstShots=0;bool strafeRight=false;
    std::string id;int cash=0;bool looted=false,carried=false;float corpseVisualDelay=0,knockedDown=0;
    float impactAnimationTotal=0,vehicleImpactCooldown=0;
    bool police=false;int weaponIndex=0;float accuracy=0.045f;
    bool pinned=false;Vec2 pinAnchor{};float burnTime=0;};
enum class Kind{Car,SportCar,Bike,Boat};
enum class CameraMode{FirstClose,FirstWide,ThirdNear,ThirdFar,Overview};
struct Vehicle{Vec2 p;float angle=0,speed=0;Kind kind;Color c;
    Vec2 velocity{};float yawRate=0,lean=0,damage=0,rideHeight=0;std::string id;
    int trafficRoute=-1;bool trafficForward=true;
    bool exploded=false;float explosionVisualTime=0,collisionCooldown=0,burnTime=0;
    bool lightsOn=false,lightsManual=false;
    float driftTime=0,driftDistance=0,driftTurn=0,driftCooldown=0,driftLastAngle=0;
    bool driftTracking=false,owned=false;std::string garageHouseId;};
struct Bullet{Vec3 p,v;float life;int damage;float gravity;float distance=0,range=700,falloff=0;
    bool hostile=false,rocket=false;float explosionRadius=0;int explosionDamage=0;
    int streamType=0;bool silent=false,arrow=false;int sourceWeapon=-1;};
struct Impact{Vec2 p;float life;bool person;};
struct HitFlash{Vec3 p;float life;Color c;bool person=false;};
struct Blast{Vec3 p;float life=0,radius=0;};
struct Debris{Vec3 p,v;float life,rotation,spin,w,h,d;int tile;};
struct RagdollPart{Vec3 p,rest,origin;float qx,qy,qz,qw,yaw;int style,part;};
struct CorpseSnapshot{std::string pedId;std::array<RagdollPart,6> parts;};
struct Prop{Vec2 p,v;float y=0,vy=0,rotation=0,spin=0;int health=80;bool barrel=false,alive=true;};
struct Pickup{Vec2 p;int weapon;bool available=true;float respawn=0;std::string id;};
enum class MissionKind{Drive,Collect,Boat,Targets,Bike,Finale};
struct MissionDef{const char* name;Vec2 start;MissionKind kind;std::vector<Vec2> goals;float seconds;int reward;std::string id;};

extern HWND win;extern HDC dc;extern HGLRC glrc;extern GLuint fontBase;
extern int screenW,screenH;
extern bool keys[256],leftMouse,rightMouse,showMap;
extern bool debugHud;
extern float frameRate,frameMs,simulationMs,physicsMs;
extern int drawCalls,activeAi;
extern POINT lastMouse;
extern float cameraYaw,cameraPitch,renderAlpha,health,fireCooldown,invulnerable,walkPhase,stepTimer,muzzleFlash;
extern CameraMode cameraMode;
extern int scopeLevel;
extern float scopeBlend,vehicleLookTime;
extern bool telescopeActive;
extern float armor;
extern int repairKits;
extern float airTime,vehicleEntryTime;
extern int weapon,occupied,money,activeMission,missionStep;
extern int enteringVehicle;
extern float missionTime,messageTime,worldTime,gameHour;
extern std::string message;
#ifdef MINI_CITY_JOLT
extern bool screenshotRequested;
void requestScreenshot();
#endif
extern float missionBannerTime;
extern std::string missionBannerTitle,missionBannerDetail;
extern std::vector<bool> unlocked;
extern std::vector<int> ammo;
extern std::vector<int> magazine;
extern std::vector<int> armedKills;
extern float reloadRemaining,recoil;
extern float meleeVisualTime;
extern const char* weaponNames[];
extern Vec2 player;
extern Vec3 lastMuzzle;
extern Vec3 lastMuzzleLeft;
extern Vec2 previousPlayer;
extern Vec2 playerVelocity;
extern float playerY,playerVerticalSpeed;
extern bool grounded;
extern bool swimming;
extern bool crouched;
extern std::vector<Building> buildings;
extern std::vector<Tree> trees;
extern std::vector<Ped> peds;
extern std::vector<Vehicle> vehicles;
extern std::vector<Bullet> bullets;
extern std::vector<Impact> impacts;
extern std::vector<HitFlash> hitFlashes;
extern std::vector<Blast> blasts;
extern std::vector<Debris> debris;
extern std::vector<RagdollPart> ragdollParts;
extern std::vector<CorpseSnapshot> corpseSnapshots;
extern std::vector<Prop> props;
extern std::vector<Pickup> pickups;
extern std::vector<MissionDef> missions;
extern std::array<bool,10> missionDone;

void reset();
void update(float dt);
void enterExit();
void startMission();
int nextMission();
const char* missionObjective();
void startReload();
void shoot();
bool dualWieldActive(int weaponIndex);
void interact();
void cycleInteraction();
void carryDrop();
void clearCarry();
bool carryingBody();
std::string interactionPrompt();
std::string carryPrompt();
void applyDamage(float amount);
void creditMoney(int amount);
bool spendMoney(int amount);
void damageVehicle(int index,float amount);
bool repairVehicle(int index);
float vehicleHealth(int index);
bool vehicleLightsOn(const Vehicle& vehicle);
void render();
bool initRenderer();
void shutdownRenderer();
}
