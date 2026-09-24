#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace game {
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
struct Building{float x,z,w,d,h;Color c;};
enum class PedState{Wander,Investigate,Flee,TakeCover,Defend,Attack};
struct Ped{Vec2 p,target;float speed,angle,respawn=0;bool alive=true;Color shirt;int health=100;float panic=0;int style=0;bool armed=false,hostile=false;float fireCooldown=0,hitFlash=0;
    PedState state=PedState::Wander;float alertTime=0;int armor=0;Vec2 knockback{};};
enum class Kind{Car,SportCar,Bike,Boat};
struct Vehicle{Vec2 p;float angle=0,speed=0;Kind kind;Color c;
    Vec2 velocity{};float yawRate=0,lean=0,damage=0;};
struct Bullet{Vec3 p,v;float life;int damage;float gravity;float distance=0,range=700,falloff=0;bool hostile=false;};
struct Impact{Vec2 p;float life;bool person;};
struct Debris{Vec3 p,v;float life,rotation,spin,w,h,d;int tile;};
struct RagdollPart{Vec3 p,rest,origin;float qx,qy,qz,qw,yaw;int style,part;};
struct Prop{Vec2 p,v;float y=0,vy=0,rotation=0,spin=0;int health=80;bool barrel=false,alive=true;};
struct Pickup{Vec2 p;int weapon;bool available=true;float respawn=0;};
enum class MissionKind{Drive,Collect,Boat,Targets,Bike,Finale};
struct MissionDef{const char* name;Vec2 start;MissionKind kind;std::vector<Vec2> goals;float seconds;int reward;};

extern HWND win;extern HDC dc;extern HGLRC glrc;extern GLuint fontBase;
extern int screenW,screenH;
extern bool keys[256],leftMouse,rightMouse,showMap;
extern bool debugHud;
extern float frameRate,frameMs,simulationMs,physicsMs;
extern int drawCalls,activeAi;
extern POINT lastMouse;
extern float cameraYaw,cameraPitch,renderAlpha,health,fireCooldown,invulnerable,walkPhase,stepTimer,muzzleFlash;
extern int weapon,occupied,money,activeMission,missionStep;
extern float missionTime,messageTime,worldTime,gameHour;
extern std::string message;
extern std::array<bool,5> unlocked;
extern std::array<int,5> ammo;
extern std::array<int,5> magazine;
extern float reloadRemaining,recoil;
extern const char* weaponNames[];
extern Vec2 player;
extern Vec2 previousPlayer;
extern Vec2 playerVelocity;
extern float playerY,playerVerticalSpeed;
extern bool grounded;
extern std::vector<Building> buildings;
extern std::vector<Ped> peds;
extern std::vector<Vehicle> vehicles;
extern std::vector<Bullet> bullets;
extern std::vector<Impact> impacts;
extern std::vector<Debris> debris;
extern std::vector<RagdollPart> ragdollParts;
extern std::vector<Prop> props;
extern std::vector<Pickup> pickups;
extern std::vector<MissionDef> missions;
extern std::array<bool,6> missionDone;

void reset();
void update(float dt);
void enterExit();
void startMission();
int nextMission();
const char* missionObjective();
void startReload();
void render();
bool initRenderer();
void shutdownRenderer();
}
