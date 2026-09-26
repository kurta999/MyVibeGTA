#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <string>
#include "game.h"
#include "audio.h"
#include "ui.h"
#include "physics.h"
#include "weapons.h"
#include "savegame.h"
#include "camera.h"
#include "props.h"
#include "fire.h"
#include "police.h"
#include "commerce.h"
#include "traversal.h"
#include "weather.h"
#include "regions.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#include "debug_menu.h"
#endif
#include "ai.h"
#include "content.h"
#include <chrono>
namespace game {
float randf(float a,float b){return a+(b-a)*(float(std::rand())/RAND_MAX);}
int randi(int n){return std::rand()%n;}
HWND win=nullptr;HDC dc=nullptr;HGLRC glrc=nullptr;GLuint fontBase=0;
int screenW=1600,screenH=900;
bool keys[256]{},leftMouse=false,rightMouse=false;POINT lastMouse{};
float cameraYaw=0,cameraPitch=0,renderAlpha=0,health=PLAYER_MAX_HEALTH,fireCooldown=0,invulnerable=0,walkPhase=0,stepTimer=0,muzzleFlash=0;
CameraMode cameraMode=CameraMode::ThirdNear;
int scopeLevel=0;
float scopeBlend=0,vehicleLookTime=0;
bool telescopeActive=false;
float armor=0;
int repairKits=1;
float airTime=0,vehicleEntryTime=0;
int weapon=0,occupied=-1,money=0,activeMission=-1,missionStep=0;
int enteringVehicle=-1;
float missionTime=0,messageTime=0,worldTime=0,gameHour=16.5f;
float missionBannerTime=0;
std::string missionBannerTitle,missionBannerDetail;
float engineSoundTime=0,surfSoundTime=0,skidSoundTime=0,trafficSoundTime=0;
bool showMap=false;
bool debugHud=false;
float frameRate=0,frameMs=0,simulationMs=0,physicsMs=0;
int drawCalls=0,activeAi=0;
std::string message;
#ifdef MINI_CITY_JOLT
bool screenshotRequested=false;
void requestScreenshot(){screenshotRequested=true;}
#endif
std::vector<bool> unlocked{true,false,false,false,false};
std::vector<int> ammo{-1,0,0,0,0};
std::vector<int> magazine{12,0,0,0,0};
std::vector<int> armedKills;
float reloadRemaining=0,recoil=0;
float meleeVisualTime=0;
int reloadingWeapon=-1;
int carriedPed=-1;
int interactionSelection=0;
const char* weaponNames[]={"PISTOL","SMG","SHOTGUN","RIFLE","SNIPER"};
Vec2 player{300,250};
Vec3 lastMuzzle{};
Vec3 lastMuzzleLeft{};
Vec2 previousPlayer{300,250};
Vec2 playerVelocity{};
float playerY=0,playerVerticalSpeed=0;
bool grounded=true;
bool swimming=false;
bool crouched=false;
std::vector<Building> buildings;
std::vector<Tree> trees;
std::vector<Ped> peds;
std::vector<Vehicle> vehicles;
std::vector<Bullet> bullets;
std::vector<Impact> impacts;
std::vector<HitFlash> hitFlashes;
std::vector<Blast> blasts;
std::vector<Debris> debris;
std::vector<RagdollPart> ragdollParts;
std::vector<CorpseSnapshot> corpseSnapshots;
std::vector<Pickup> pickups;
std::vector<MissionDef> missions;
std::array<bool,10> missionDone{};
void spawnDebris(const Ped& ped,Vec3 impulse);

void announce(const std::string& s,float seconds=4){message=s;messageTime=seconds;}
void applyDamage(float amount){
    if(!std::isfinite(amount)||amount<=0||health<=0)return;
#ifdef MINI_CITY_JOLT
    if(debug_menu::godMode)return;
#endif
    float absorbed=std::min(armor,amount);
    armor-=absorbed;
    health=std::max(0.0f,health-(amount-absorbed));
}
void creditMoney(int amount){
    if(amount>0)money=int(std::min(9999999LL,static_cast<long long>(money)+amount));
}
bool spendMoney(int amount){
    if(amount<0||money<amount)return false;
    money-=amount;return true;
}
float vehicleHealth(int index){
    if(index<0||index>=int(vehicles.size()))return 0;
    const Vehicle& vehicle=vehicles[index];
    return physics::tuning(vehicle.kind).maxHealth*(1.0f-vehicle.damage/100.0f);
}
bool vehicleLightsOn(const Vehicle& vehicle){
    return !vehicle.exploded&&vehicle.kind!=Kind::Boat&&
        (vehicle.lightsManual?vehicle.lightsOn:
            std::sin((gameHour-6)*PI/12.0f)<0.12f);
}
void damageVehicle(int index,float amount){
    if(index<0||index>=int(vehicles.size())||!std::isfinite(amount)||amount<=0)return;
    Vehicle& vehicle=vehicles[index];
    if(vehicle.exploded)return;
    const auto tuning=physics::tuning(vehicle.kind);
    vehicle.damage=std::min(100.0f,vehicle.damage+amount/tuning.maxHealth*100.0f);
    if(vehicle.damage<100){
        if(vehicle.damage>=80)fire::igniteVehicle(vehicle);
        return;
    }
    vehicle.exploded=true;vehicle.burnTime=0;vehicle.explosionVisualTime=1.4f;
    blasts.push_back({{vehicle.p.x,vehicle.rideHeight+15.0f,vehicle.p.z},
        0.75f,95.0f});
    fire::ignite(vehicle.p,fire::Material::Metal);
    vehicle.velocity={};vehicle.speed=0;
    audio::playAt(audio::Effect::Hit,vehicle.p.x,vehicle.p.z);
    if(occupied==index){
        player=vehicle.p+Vec2{65,0};occupied=-1;applyDamage(110);
    }else if(health>0){
        float distance=len(player-vehicle.p);
        if(distance<100)applyDamage((1-distance/100)*80);
    }
    bool killedCivilian=false;
    for(auto& ped:peds)if(ped.alive){
        float distance=len(ped.p-vehicle.p);
        if(distance>=105)continue;
        int damage=int((1-distance/105)*125);
        int absorbed=std::min(ped.armor,damage);
        ped.armor-=absorbed;ped.health-=damage-absorbed;
        ped.knockback=ped.knockback+norm(ped.p-vehicle.p)*140;
        ped.hitFlash=0.3f;
        if(ped.health<=0){ped.alive=false;ped.respawn=ped.police?999999:45;ped.corpseVisualDelay=6;
            killedCivilian|=!ped.police;
            spawnDebris(ped,{0,250,0});}
    }
    if(killedCivilian)police::report(police::Crime::Murder,vehicle.p);
    for(auto& prop:props)if(prop.alive&&len(prop.p-vehicle.p)<90)
        prop.health-=int((1-len(prop.p-vehicle.p)/90)*110);
    for(int other=0;other<int(vehicles.size());++other)if(other!=index){
        float distance=len(vehicles[other].p-vehicle.p);
        if(distance<85)damageVehicle(other,(1-distance/85)*115);
    }
    announce("VEHICLE EXPLODED",3);
}
void explodeAt(Vec3 point,float radius,int damage){
    if(radius<=0||damage<=0)return;
    blasts.push_back({point,0.75f,radius});
    fire::ignite({point.x,point.z},fire::surfaceAt(point));
    audio::playAt(audio::Effect::Hit,point.x,point.z);
    Vec2 center{point.x,point.z};
    float playerDistance=len(player-center);
    if(health>0&&playerDistance<radius)
        applyDamage(damage*(1-playerDistance/radius));
    bool killedCivilian=false;
    for(auto& ped:peds)if(ped.alive){
        float distance=len(ped.p-center);
        if(distance>=radius)continue;
        int hit=int(damage*(1-distance/radius));
        int absorbed=std::min(ped.armor,hit);
        ped.armor-=absorbed;ped.health-=hit-absorbed;
        ped.knockback=ped.knockback+norm(ped.p-center)*110;
        ped.hitFlash=0.3f;
        if(ped.health<=0){ped.alive=false;ped.respawn=ped.police?999999:45;ped.corpseVisualDelay=6;
            killedCivilian|=!ped.police;
            spawnDebris(ped,{0,220,0});}
    }
    if(killedCivilian)police::report(police::Crime::Murder,center);
    for(auto& prop:props)if(prop.alive){
        float distance=len(prop.p-center);
        if(distance<radius)prop.health-=int(damage*(1-distance/radius));
    }
    for(int index=0;index<int(vehicles.size());++index){
        float distance=len(vehicles[index].p-center);
        if(distance<radius)damageVehicle(index,damage*(1-distance/radius));
    }
    for(int index:regions::nearbyTreeIndices(center,radius)){
        if(index<0||std::size_t(index)>=trees.size())continue;
        Tree& tree=trees[index];
        if(tree.destroyed)continue;
        float distance=len(tree.p-center);
        if(distance>=radius)continue;
        tree.health=std::max(0,tree.health-
            std::max(1,int(damage*(1-distance/radius)*0.35f)));
        if(tree.health==0)tree.destroyed=true;
        else {tree.burning=true;fire::ignite(tree.p,fire::Material::Wood);}
    }
}
bool repairVehicle(int index){
    if(index<0||index>=int(vehicles.size())||occupied>=0||health<=0)return false;
    Vehicle& vehicle=vehicles[index];
    if(vehicle.exploded||vehicle.damage<physics::tuning(vehicle.kind).smokeThreshold||
       len(player-vehicle.p)>65)return false;
    if(repairKits<=0){announce("Need a repair kit.",3);return false;}
    --repairKits;vehicle.damage=0;vehicle.burnTime=0;vehicle.explosionVisualTime=0;
    announce("VEHICLE REPAIRED",3);audio::play(audio::Effect::Pickup);
    savegame::save();return true;
}
void missionCard(const std::string& title,const std::string& detail,float seconds=3){
    missionBannerTitle=title;missionBannerDetail=detail;missionBannerTime=seconds;
}
void completeMission(){
    const auto& m=missions[activeMission];creditMoney(m.reward);missionDone[activeMission]=true;
    health=std::min(PLAYER_MAX_HEALTH,health+20*(PLAYER_MAX_HEALTH/100.0f));
    announce(std::string("MISSION PASSED: ")+m.name+"  +$"+std::to_string(m.reward),7);
    missionCard("MISSION PASSED",std::string(m.name)+"  +$"+std::to_string(m.reward),4);
    audio::play(audio::Effect::Success);
    activeMission=-1;missionStep=0;
    savegame::save();
}
int nextMission(){
    for(int index=0;index<int(missions.size());++index)if(!missionDone[index])return index;
    return -1;
}
const char* missionObjective(){
    if(activeMission<0)return "Follow the gold line to the next mission.";
    const auto& mission=missions[activeMission];
    if(mission.kind==MissionKind::Finale)return missionStep==0?"Drive a car to the beach approach":
        missionStep==1?"Exit and shoot the orange beach target":"Pilot a boat to the harbor marker";
    if(mission.kind==MissionKind::Drive)return "Drive a car through the checkpoints";
    if(mission.kind==MissionKind::Collect)return "Collect the beach caches on foot";
    if(mission.kind==MissionKind::Boat)return "Pilot a boat through the harbor";
    if(mission.kind==MissionKind::Bike)return "Ride a motorcycle through the checkpoints";
    return "Shoot the orange practice targets";
}
void startMission(){
    if(activeMission>=0||health<=0)return;
    for(int i=0;i<int(missions.size());++i)if(!missionDone[i]&&
        len(missions[i].start-player)<75){
        if(i>0&&i<6&&!missionDone[i-1]){
            announce(std::string("Finish ")+missions[i-1].name+" to unlock this mission.",4);
            return;
        }
        activeMission=i;missionStep=0;missionTime=missions[i].seconds;
        missionCard(missions[i].name,missionObjective(),3.5f);
        announce(std::string("MISSION STARTED: ")+missions[i].name,5);audio::play(audio::Effect::Pickup);return;
    }
}

bool inside(const Building& b,Vec2 p,float pad){
    return p.x>b.x-pad&&p.x<b.x+b.w+pad&&p.z>b.z-pad&&p.z<b.z+b.d+pad;
}
bool solid(Vec2 p,float radius){
    if(p.x<radius||p.x>regions::WIDTH-radius||p.z<radius||
       p.z>regions::DEPTH-radius||regions::waterAt(p))return true;
    for(const auto& b:buildings)if(inside(b,p,radius))return true;
    return false;
}
bool bulletSolid(Vec3 p){
    if(p.x<0||p.x>regions::WIDTH||p.z<0||p.z>regions::DEPTH||p.y<0)return true;
    for(const auto& b:buildings)if(p.y<b.h&&inside(b,{p.x,p.z},0))return true;
    return false;
}
bool bulletSolidSegment(Vec3 start,Vec3 end,Vec3& impact){
    Vec3 delta=end-start;
    float nearest=2.0f;
    if(bulletSolid(start))nearest=0;
    auto boundary=[&](float from,float movement,float low,float high){
        float finish=from+movement;
        if(finish<low&&movement<0)
            nearest=std::min(nearest,(low-from)/movement);
        else if(finish>high&&movement>0)
            nearest=std::min(nearest,(high-from)/movement);
    };
    boundary(start.x,delta.x,0,regions::WIDTH);
    boundary(start.z,delta.z,0,regions::DEPTH);
    if(end.y<0&&delta.y<0)nearest=std::min(nearest,-start.y/delta.y);
    for(const auto& building:buildings){
        float entry=0,exit=1;
        auto slab=[&](float from,float movement,float low,float high){
            if(std::abs(movement)<0.00001f)return from>=low&&from<=high;
            float a=(low-from)/movement,b=(high-from)/movement;
            if(a>b)std::swap(a,b);
            entry=std::max(entry,a);exit=std::min(exit,b);
            return entry<=exit;
        };
        if(slab(start.x,delta.x,building.x,building.x+building.w)&&
           slab(start.y,delta.y,0,building.h)&&
           slab(start.z,delta.z,building.z,building.z+building.d))
            nearest=std::min(nearest,entry);
    }
    if(nearest<0||nearest>1)return false;
    impact=start+delta*nearest;
    return true;
}
bool bulletCylinderSegment(Vec3 start,Vec3 end,Vec2 center,float radius,
                           float low,float high,float& entry){
    Vec3 delta=end-start;
    float first=0,last=1;
    if(std::abs(delta.y)<0.00001f){
        if(start.y<low||start.y>high)return false;
    }else{
        float a=(low-start.y)/delta.y,b=(high-start.y)/delta.y;
        if(a>b)std::swap(a,b);
        first=std::max(first,a);last=std::min(last,b);
    }
    float x=start.x-center.x,z=start.z-center.z;
    float a=delta.x*delta.x+delta.z*delta.z;
    float c=x*x+z*z-radius*radius;
    if(a<0.00001f){
        if(c>0)return false;
    }else{
        float b=2*(x*delta.x+z*delta.z);
        float discriminant=b*b-4*a*c;
        if(discriminant<0)return false;
        float root=std::sqrt(discriminant);
        first=std::max(first,(-b-root)/(2*a));
        last=std::min(last,(-b+root)/(2*a));
    }
    if(first>last||last<0||first>1)return false;
    entry=std::clamp(first,0.0f,1.0f);
    return true;
}
bool valid(Vec2 p,Kind k,float radius){
    if(k==Kind::Boat)return p.x>=radius&&p.x<=regions::WIDTH-radius&&
        p.z>=radius&&p.z<=regions::DEPTH-radius&&regions::waterAt(p);
    return !solid(p,radius);
}
Vec2 randomWalkable(){
    for(int i=0;i<1000;++i){Vec2 p{randf(25,WORLD_W-25),randf(25,SHORE-25)};if(!solid(p,13))return p;}
    return {300,250};
}
void reset(){
#ifdef MINI_CITY_JOLT
    debug_menu::reset();
    screenshotRequested=false;
#endif
    buildings.clear();trees.clear();peds.clear();vehicles.clear();bullets.clear();impacts.clear();hitFlashes.clear();blasts.clear();debris.clear();ragdollParts.clear();corpseSnapshots.clear();pickups.clear();missions.clear();
    player={300,250};previousPlayer=player;playerVelocity={};playerY=0;playerVerticalSpeed=0;grounded=true;swimming=false;crouched=false;
    health=PLAYER_MAX_HEALTH;armor=0;repairKits=1;weapon=0;occupied=-1;enteringVehicle=-1;vehicleEntryTime=0;airTime=0;
    carriedPed=-1;
    interactionSelection=0;
    fireCooldown=0;reloadRemaining=0;reloadingWeapon=-1;recoil=0;meleeVisualTime=0;
    cameraYaw=0;cameraPitch=0;cameraMode=CameraMode::ThirdNear;
    scopeLevel=0;scopeBlend=0;vehicleLookTime=0;telescopeActive=false;invulnerable=0;
    money=0;activeMission=-1;missionStep=0;missionTime=0;showMap=false;worldTime=0;gameHour=16.5f;
    missionBannerTime=0;missionBannerTitle.clear();missionBannerDetail.clear();
    walkPhase=0;stepTimer=0;muzzleFlash=0;engineSoundTime=0;surfSoundTime=0;
    skidSoundTime=0;trafficSoundTime=0;
    unlocked.assign(weapons::count(),false);ammo.assign(weapons::count(),0);
    armedKills.assign(weapons::count(),0);
    magazine.assign(weapons::count(),0);
    unlocked[0]=true;ammo[0]=-1;magazine[0]=weapons::stats(0).magazine;
    missionDone.fill(false);
    announce("Find the colored mission markers. Press F to start; M opens the map.",8);
    content::populate();
    regions::populate();
    commerce::reset();
    traversal::reset();
    weather::reset();
    police::reset();
    fire::reset();
    props::reset();
}
void move(Vec2& p,Vec2 d,float radius,Kind kind){
    Vec2 trial{p.x+d.x,p.z};if(valid(trial,kind,radius))p.x=trial.x;
    trial={p.x,p.z+d.z};if(valid(trial,kind,radius))p.z=trial.z;
}
bool clearLine(Vec2 a,Vec2 b){
    for(int i=1;i<12;++i){Vec2 point=a+(b-a)*(i/12.0f);
        for(const auto& building:buildings)if(inside(building,point,1))return false;}
    return true;
}
namespace {
enum class InteractionType{None,Loot,Pickpocket,Repair,Shop,House,Ladder,Tree,Mission};
struct Interaction{InteractionType type=InteractionType::None;int index=-1;};
std::vector<Interaction> availableInteractions(){
    std::vector<Interaction> options;
    if(health<=0||occupied>=0)return options;
    int corpse=-1;float corpseDistance=38;
    for(int i=0;i<int(peds.size());++i){
        const Ped& ped=peds[i];
        if(ped.alive||ped.looted||ped.carried)continue;
        float distance=len(ped.p-player);
        if(distance<corpseDistance&&clearLine(player,ped.p)){
            corpse=i;corpseDistance=distance;
        }
    }
    if(corpse>=0)options.push_back({InteractionType::Loot,corpse});
    int target=-1;float targetDistance=29;
    for(int i=0;i<int(peds.size());++i){
        const Ped& ped=peds[i];
        if(!ped.alive||ped.cash<=0||ped.hostile)continue;
        float distance=len(ped.p-player);
        if(distance>=targetDistance||!clearLine(player,ped.p))continue;
        Vec2 facing=forward(ped.angle),behind=norm(player-ped.p);
        if(facing.x*behind.x+facing.z*behind.z>-0.45f)continue;
        target=i;targetDistance=distance;
    }
    if(target>=0)options.push_back({InteractionType::Pickpocket,target});
    int repair=-1;float repairDistance=65;
    for(int i=0;i<int(vehicles.size());++i){
        const Vehicle& vehicle=vehicles[i];
        float distance=len(vehicle.p-player);
        if(!vehicle.exploded&&vehicle.damage>=physics::tuning(vehicle.kind).smokeThreshold&&
           distance<repairDistance&&clearLine(player,vehicle.p)){
            repair=i;repairDistance=distance;
        }
    }
    if(repair>=0)options.push_back({InteractionType::Repair,repair});
    for(int i=0;i<int(commerce::shops.size());++i)
        if(len(commerce::shops[i].p-player)<55&&clearLine(player,commerce::shops[i].p))
            options.push_back({InteractionType::Shop,i});
    for(int i=0;i<int(commerce::houses.size());++i)
        if(len(commerce::houses[i].p-player)<55&&clearLine(player,commerce::houses[i].p))
            options.push_back({InteractionType::House,i});
    for(int i=0;i<int(traversal::ladders.size());++i)
        if(len(traversal::ladders[i].bottom-player)<48&&
           clearLine(player,traversal::ladders[i].bottom))
            options.push_back({InteractionType::Ladder,i});
    for(int i=0;i<int(traversal::trees.size());++i)
        if(len(traversal::trees[i].bottom-player)<43&&
           clearLine(player,traversal::trees[i].bottom)){
            int treeIndex=traversal::trees[i].treeIndex;
            if(treeIndex>=0&&treeIndex<int(trees.size())&&!trees[treeIndex].destroyed)
                options.push_back({InteractionType::Tree,i});
        }
    if(activeMission<0)for(int i=0;i<int(missions.size());++i)
        if(len(player-missions[i].start)<75){
            options.push_back({InteractionType::Mission,i});break;
        }
    return options;
}
Interaction chooseInteraction(){
    auto options=availableInteractions();
    if(options.empty())return {};
    return options[std::clamp(interactionSelection,0,int(options.size())-1)];
}
}
void cycleInteraction(){
    auto options=availableInteractions();
    interactionSelection=options.size()>1?(interactionSelection+1)%int(options.size()):0;
}
std::string interactionPrompt(){
    if(traversal::active())return "W/S CLIMB  |  F LET GO  |  SPACE JUMP FROM TREE";
    auto options=availableInteractions();
    if(options.empty())return {};
    int selected=std::clamp(interactionSelection,0,int(options.size())-1);
    Interaction action=options[selected];
    std::string prompt;
    switch(action.type){
    case InteractionType::Loot:prompt="F  LOOT BODY";break;
    case InteractionType::Pickpocket:prompt="F  PICKPOCKET";break;
    case InteractionType::Repair:prompt="F  REPAIR VEHICLE  (KITS "+
        std::to_string(repairKits)+")";break;
    case InteractionType::Shop:prompt="F  SHOP: "+commerce::shops[action.index].name;break;
    case InteractionType::House:prompt="F  HOUSE: "+commerce::houses[action.index].name;break;
    case InteractionType::Ladder:prompt="F  CLIMB LADDER";break;
    case InteractionType::Tree:prompt="F  CLIMB TREE";break;
    case InteractionType::Mission:prompt="F  START "+std::string(missions[action.index].name);break;
    default:break;
    }
    if(options.size()>1)prompt+="  |  TAB CHOOSE "+std::to_string(selected+1)+
        "/"+std::to_string(options.size());
    return prompt;
}
std::string carryPrompt(){
    if(carriedPed>=0)return "G  DROP BODY";
    if(health<=0||occupied>=0)return {};
    for(const Ped& ped:peds)if(!ped.alive&&!ped.carried&&len(ped.p-player)<38&&
        clearLine(player,ped.p))return "G  CARRY BODY";
    return {};
}
void interact(){
    if(traversal::active()){traversal::detach();return;}
    Interaction action=chooseInteraction();
    interactionSelection=0;
    if(action.type==InteractionType::Loot){
        Ped& ped=peds[action.index];
        int found=ped.cash;ped.cash=0;ped.looted=true;
        creditMoney(found);
        announce("LOOTED $"+std::to_string(found),3);
        savegame::save();
    }else if(action.type==InteractionType::Pickpocket){
        Ped& ped=peds[action.index];
        if(randi(100)<content::pickpocketNoticePercent()){
            ped.hostile=true;ped.alertTime=8;ped.panic=0;
            ped.lastKnown=player;ped.sightMemory=5;
            ped.target=player;ped.state=PedState::Attack;
            police::report(police::Crime::Theft,player,true,true);
            announce("Pickpocket noticed! Get away!",3);
        }else{
            int stolen=ped.cash;ped.cash=0;
            creditMoney(stolen);
            announce("STOLE $"+std::to_string(stolen),3);
            savegame::save();
        }
    }else if(action.type==InteractionType::Repair)repairVehicle(action.index);
    else if(action.type==InteractionType::Shop)commerce::openShop(action.index);
    else if(action.type==InteractionType::House)commerce::openHouse(action.index);
    else if(action.type==InteractionType::Ladder)traversal::startLadder(action.index);
    else if(action.type==InteractionType::Tree)traversal::startTree(action.index);
    else if(action.type==InteractionType::Mission)startMission();
}
void carryDrop(){
    if(carriedPed>=0){
        Ped& ped=peds[carriedPed];ped.carried=false;
        Vec2 candidate=player+forward(cameraYaw)*20;
        if(solid(candidate,10))candidate=player-forward(cameraYaw)*20;
        ped.p=solid(candidate,10)?player:candidate;
        carriedPed=-1;announce("Body dropped.",2);return;
    }
    if(health<=0||occupied>=0)return;
    int best=-1;float distance=38;
    for(int i=0;i<int(peds.size());++i){
        const Ped& ped=peds[i];float d=len(ped.p-player);
        if(!ped.alive&&!ped.carried&&d<distance&&clearLine(player,ped.p)){
            best=i;distance=d;
        }
    }
    if(best<0)return;
    carriedPed=best;peds[best].carried=true;peds[best].pinned=false;
    peds[best].corpseVisualDelay=0;
#ifdef MINI_CITY_JOLT
    jolt_world::removeRagdoll(peds[best].id);
#endif
    announce("Carrying body. Press G to drop.",3);
}
void clearCarry(){
    carriedPed=-1;
    for(auto& ped:peds)ped.carried=false;
}
bool carryingBody(){return carriedPed>=0;}
void spawnDebris(const Ped& ped,Vec3 impulse){
#ifdef MINI_CITY_JOLT
    jolt_world::spawnRagdoll(ped,impulse);
    if(len(impulse)<2500)return;
#endif
    if(debris.size()>110)debris.erase(debris.begin(),debris.begin()+35);
    const int tiles[]={16+ped.style,20+ped.style,20+ped.style,24+ped.style,24+ped.style};
    const float heights[]={30,19,17,7,7};
    const float sides[]={0,7,-7,4,-4};
    for(int i=0;i<5;++i){
        Vec3 p{ped.p.x,heights[i],ped.p.z+sides[i]};
        Vec3 velocity{impulse.x*0.12f+randf(-48,48),randf(75,170),impulse.z*0.12f+randf(-48,48)};
        debris.push_back({p,velocity,randf(2.5f,4.5f),0,randf(-230,230),
            i==0?8.0f:5.0f,i==0?8.0f:11.0f,i==0?8.0f:5.0f,tiles[i]});
    }
}
void enterExit(){
    if(enteringVehicle>=0)return;
    if(carriedPed>=0)carryDrop();
    if(occupied>=0){
        Vehicle& v=vehicles[occupied];Vec2 side{-std::sin(v.angle),std::cos(v.angle)};
        if(v.kind==Kind::Boat){
            if(v.p.z<SHORE+105){Vec2 dock{v.p.x,SHORE-22};
                if(!solid(dock,12)){player=dock;occupied=-1;swimming=false;
                    audio::play(audio::Effect::Splash);return;}}
            player=v.p+side*30;occupied=-1;swimming=true;
            playerY=0;playerVerticalSpeed=0;grounded=false;
            audio::play(audio::Effect::Splash);
            return;
        }
        for(int sign:{1,-1}){Vec2 out=v.p+side*(sign*(v.kind==Kind::Bike?27.0f:38.0f));
            if(!solid(out,12)){player=out;occupied=-1;return;}}
        return;
    }
    float best=85;int index=-1;
    for(int i=0;i<int(vehicles.size());++i){
        float d=len(vehicles[i].p-player);
        if(!vehicles[i].exploded&&d<best){best=d;index=i;}
    }
    if(index>=0){
        telescopeActive=false;
        if(vehicles[index].id.rfind("traffic-",0)==0){
            police::report(police::Crime::CarTheft,player,true);
            vehicles[index].trafficRoute=-1;
        }
#ifdef MINI_CITY_JOLT
        enteringVehicle=index;vehicleEntryTime=0.65f;playerVelocity={};
#else
        occupied=index;player=vehicles[index].p;cameraYaw=vehicles[index].angle;
        if(vehicles[index].kind!=Kind::Boat){
            vehicles[index].lightsOn=true;vehicles[index].lightsManual=true;
        }
        audio::play(vehicles[index].kind==Kind::Boat?audio::Effect::Splash:audio::Effect::Engine);
#endif
    }
}
void startReload(){
    if(health<=0||carriedPed>=0||reloadRemaining>0)return;
    const auto& stats=weapons::stats(weapon);
    if(stats.melee)return;
    if(occupied>=0&&(vehicles[occupied].kind==Kind::Boat||
        vehicles[occupied].kind==Kind::Bike||!stats.driveByAllowed))return;
    if(magazine[weapon]>=stats.magazine||ammo[weapon]==0)return;
    reloadingWeapon=weapon;reloadRemaining=stats.reloadSeconds;
    audio::play(audio::Effect::Reload);
}
bool dualWieldActive(int weaponIndex){
    return weaponIndex>=0&&weaponIndex<int(armedKills.size())&&
        weapons::stats(weaponIndex).dualWieldAllowed&&armedKills[weaponIndex]>=100;
}
void recordArmedKill(const Ped& ped,int weaponIndex){
    if(!ped.armed||weaponIndex<0||weaponIndex>=int(armedKills.size()))return;
    armedKills[weaponIndex]=std::min(100000,armedKills[weaponIndex]+1);
    if(armedKills[weaponIndex]==100&&weapons::stats(weaponIndex).dualWieldAllowed)
        announce("DUAL WIELD UNLOCKED: "+weapons::stats(weaponIndex).name,5);
    savegame::save();
}
void shoot(){
    if(health<=0||carriedPed>=0||fireCooldown>0||telescopeActive)return;
    const auto& stats=weapons::stats(weapon);
    bool unarmed=keys[VK_SPACE]&&!rightMouse&&occupied<0;
    if(unarmed||stats.melee){
        if(occupied>=0||enteringVehicle>=0||swimming)return;
        int damage=unarmed?(grounded?22:32):stats.damage;
        float reach=unarmed?55.0f:stats.range;
        float interval=unarmed?0.55f:stats.secondsBetweenShots;
        fireCooldown=interval;meleeVisualTime=std::min(0.4f,interval);
        Vec2 facing=forward(cameraYaw);
        int target=-1;float nearest=reach;
        if(playerY<45)for(int index=0;index<int(peds.size());++index){
            const Ped& ped=peds[index];
            Vec2 delta=ped.p-player;float distance=len(delta);
            if(!ped.alive||distance>=nearest||
                facing.x*delta.x+facing.z*delta.z<distance*0.15f||
                !clearLine(player,ped.p))continue;
            nearest=distance;target=index;
        }
        if(target>=0){
            Ped& ped=peds[target];
            int absorbed=std::min(ped.armor,damage);
            ped.armor-=absorbed;ped.health-=damage-absorbed;
            ped.hitFlash=0.3f;ped.knockback=ped.knockback+facing*115;
            ai::reactToHit(ped,player);
            if(ped.health<=0){
                ped.alive=false;ped.respawn=ped.police?999999:45;
                ped.corpseVisualDelay=6;
                spawnDebris(ped,{facing.x*150,120,facing.z*150});
                if(!unarmed)recordArmedKill(ped,weapon);
            }
            police::report(ped.police?police::Crime::AttackOfficer:
                ped.alive?police::Crime::Assault:police::Crime::Murder,
                ped.p,true,ped.alive||ped.police);
            impacts.push_back({ped.p,1.8f,true});
            audio::playAt(audio::Effect::Hit,ped.p.x,ped.p.z);
        }
        return;
    }
    if(occupied>=0&&(vehicles[occupied].kind==Kind::Boat||
        vehicles[occupied].kind==Kind::Bike||!stats.driveByAllowed))return;
    if(reloadRemaining>0&&reloadingWeapon==weapon)return;
    if(magazine[weapon]==0){
        if(ammo[weapon]!=0)startReload();
        else announce("Out of ammo. Find a weapon pickup or switch guns.",2);
        fireCooldown=0.35f;return;
    }
    int shots=occupied<0&&dualWieldActive(weapon)?std::min(2,magazine[weapon]):1;
    magazine[weapon]-=shots;
    audio::play(stats.silenced?audio::Effect::SilencedShot:audio::Effect::Shot,weapon);
    if(!stats.silenced){
        ai::notifyGunshot(player);
        police::report(stats.streamType==1?police::Crime::Arson:police::Crime::Gunfire,
            player,false);
    }
    muzzleFlash=0.12f;
    recoil=std::min(1.0f,recoil+0.25f+(weapon==2?0.25f:0));
    Vec2 f=forward(cameraYaw),r{-f.z,f.x};
    camera::Pose pose=camera::compute(player,occupied>=0?0:playerY,true,occupied);
    Vec3 target=camera::traceReticle(pose,stats.range);
    Vec3 muzzle{player.x+f.x*16+r.x*7,17+playerY,player.z+f.z*16+r.z*7};
#ifdef MINI_CITY_JOLT
    if(occupied<0)muzzle=camera::weaponMuzzle(player,playerY,cameraYaw,target);
#endif
    if(occupied>=0){
        const Vehicle& car=vehicles[occupied];
        Vec2 carFront=forward(car.angle),carSide{-carFront.z,carFront.x};
        Vec2 toward=norm(Vec2{target.x-car.p.x,target.z-car.p.z});
        float sideSign=toward.x*carSide.x+toward.z*carSide.z>=0?1.0f:-1.0f;
        muzzle={car.p.x+carFront.x*8+carSide.x*sideSign*20,
            24+car.rideHeight,
            car.p.z+carFront.z*8+carSide.z*sideSign*20};
    }
    lastMuzzle=muzzle;
    lastMuzzleLeft=muzzle+Vec3{-r.x*14,0,-r.z*14};
    for(int shot=0;shot<shots;++shot){
        Vec3 shotMuzzle=shots==2&&shot==0?lastMuzzleLeft:muzzle;
        float flightTime=len(target-shotMuzzle)/stats.projectileSpeed;
        Vec3 ballisticTarget=target;
        ballisticTarget.y+=0.5f*stats.gravity*flightTime*flightTime;
        for(int i=0;i<stats.pellets;++i){
            float spread=randf(-stats.spread,stats.spread)*(1.0f+recoil*0.3f)+
                (weapon==2?(i-3)*0.025f:0);
            Vec3 direction=norm(ballisticTarget-shotMuzzle);
            direction=norm(Vec3{direction.x+std::cos(cameraYaw+PI/2)*spread,
                direction.y+randf(-stats.spread*0.5f,stats.spread*0.5f),
                direction.z+std::sin(cameraYaw+PI/2)*spread});
            bullets.push_back({shotMuzzle,direction*stats.projectileSpeed,
                stats.range/stats.projectileSpeed,stats.damage,stats.gravity,0,stats.range,stats.falloff,
                false,stats.rocket,stats.explosionRadius,stats.explosionDamage,stats.streamType,
                stats.silenced,stats.arrow,weapon});
        }
    }
    cameraPitch=std::clamp(cameraPitch+stats.recoilKick,-0.85f,0.8f);
    fireCooldown=stats.secondsBetweenShots;
}
void applyStreamEffect(const Bullet& bullet,Vec3 point){
    if(bullet.streamType==1&&point.y<55)
        fire::ignite({point.x,point.z},fire::surfaceAt(point));
    else if(bullet.streamType==2)
        fire::extinguish({point.x,point.z},27,3.0f);
    else if(bullet.streamType==3)
        fire::extinguish({point.x,point.z},34,4.5f);
}
void spawnHitFlash(const Bullet& bullet,Vec3 point,bool person=false){
    if(bullet.streamType!=0)return;
    Color tint=rgb(217,205,168);
    if(person)tint=rgb(214,65,55);
    else switch(fire::surfaceAt(point)){
    case fire::Material::Metal:tint=rgb(255,190,70);break;
    case fire::Material::Concrete:tint=rgb(220,223,222);break;
    case fire::Material::Wood:tint=rgb(203,153,87);break;
    case fire::Material::Water:tint=rgb(121,192,235);break;
    case fire::Material::Snow:tint=rgb(229,240,250);break;
    case fire::Material::Grass:tint=rgb(172,190,117);break;
    default:break;
    }
    if(hitFlashes.size()>=96)hitFlashes.erase(hitFlashes.begin());
    hitFlashes.push_back({point,0.22f,tint,person});
}
void update(float dt){
    previousPlayer=player;
    audio::setListener(player.x,player.z,cameraYaw);
    dt=std::min(dt,0.05f);fireCooldown=std::max(0.0f,fireCooldown-dt);
    bool scoped=occupied<0&&health>0&&(telescopeActive||
        (rightMouse&&weapon==weapons::indexOf("sniper")));
    scopeBlend=std::clamp(scopeBlend+(scoped?1.0f:-1.0f)*dt*7.0f,0.0f,1.0f);
    vehicleLookTime=std::max(0.0f,vehicleLookTime-dt);
    meleeVisualTime=std::max(0.0f,meleeVisualTime-dt);
    recoil=std::max(0.0f,recoil-dt*2.2f);
    if(reloadRemaining>0){reloadRemaining-=dt;
        if(reloadRemaining<=0&&reloadingWeapon>=0){
            int needed=weapons::stats(reloadingWeapon).magazine-magazine[reloadingWeapon];
            int loaded=ammo[reloadingWeapon]<0?needed:std::min(needed,ammo[reloadingWeapon]);
            magazine[reloadingWeapon]+=loaded;
            if(ammo[reloadingWeapon]>=0)ammo[reloadingWeapon]-=loaded;
            reloadingWeapon=-1;reloadRemaining=0;
        }
    }
    invulnerable=std::max(0.0f,invulnerable-dt);messageTime=std::max(0.0f,messageTime-dt);
    if(enteringVehicle>=0){
        vehicleEntryTime-=dt;
        if(vehicleEntryTime<=0){
            occupied=enteringVehicle;enteringVehicle=-1;vehicleEntryTime=0;
            player=vehicles[occupied].p;cameraYaw=vehicles[occupied].angle;
            if(vehicles[occupied].kind!=Kind::Boat){
                vehicles[occupied].lightsOn=true;vehicles[occupied].lightsManual=true;
            }
            playerY=0;playerVerticalSpeed=0;playerVelocity={};grounded=true;airTime=0;
            audio::play(vehicles[occupied].kind==Kind::Boat?
                audio::Effect::Splash:audio::Effect::Engine);
        }
    }
    missionBannerTime=std::max(0.0f,missionBannerTime-dt);
    muzzleFlash=std::max(0.0f,muzzleFlash-dt);worldTime+=dt;
    surfSoundTime-=dt;skidSoundTime-=dt;trafficSoundTime-=dt;
    if(player.x<WORLD_W&&player.z>BEACH_START-110&&player.z<WORLD_D&&
       surfSoundTime<=0){audio::play(audio::Effect::Surf);surfSoundTime=2.0f;}
    if(trafficSoundTime<=0){
        trafficSoundTime=randf(3.0f,6.0f);
        for(int index=0;index<int(vehicles.size());++index){
            const auto& vehicle=vehicles[index];
            if(index==occupied||vehicle.kind==Kind::Boat||len(vehicle.p-player)>=450)continue;
            audio::playAt(audio::Effect::Traffic,vehicle.p.x,vehicle.p.z);break;
        }
    }
    gameHour=std::fmod(gameHour+dt*0.075f,24.0f);
    weather::update(dt);
    for(auto& vehicle:vehicles){
        vehicle.explosionVisualTime=std::max(0.0f,vehicle.explosionVisualTime-dt);
        vehicle.collisionCooldown=std::max(0.0f,vehicle.collisionCooldown-dt);
    }
    for(int i=0;i<std::min(9,weapons::count());++i)if(keys['1'+i]&&unlocked[i])weapon=i;
    if(keys[VK_LEFT])cameraYaw-=dt*1.8f;
    if(keys[VK_RIGHT])cameraYaw+=dt*1.8f;
    auto physicsBegin=std::chrono::steady_clock::now();
    if(occupied>=0||enteringVehicle>=0)crouched=false;
    if(health>0){
        if(enteringVehicle>=0){playerVelocity={};}
        else if(occupied<0){
            if(traversal::active()){
                traversal::update(dt);
            }else{
            Vec2 f=forward(cameraYaw),r{-f.z,f.x};
            Vec2 input=f*(float(keys[ui::bindings[int(ui::Action::Forward)]])-float(keys[ui::bindings[int(ui::Action::Backward)]]))+
                r*(float(keys[ui::bindings[int(ui::Action::Right)]])-float(keys[ui::bindings[int(ui::Action::Left)]]));
            bool slowWalk=keys[VK_MENU]||keys[VK_LMENU]||keys[VK_RMENU];
            bool running=!crouched&&!slowWalk&&keys[ui::bindings[int(ui::Action::Sprint)]];
            Vec2 desired=norm(input)*(swimming?95.0f:crouched?75.0f:
                slowWalk?80.0f:carriedPed>=0?105.0f:running?250.0f:160.0f);
            float response=grounded?10.0f:3.0f;
#ifdef MINI_CITY_JOLT
            if(debug_menu::flyMode)response=10.0f;
#endif
            playerVelocity=playerVelocity+(desired-playerVelocity)*std::min(1.0f,response*dt);
            Vec2 oldPlayer=player;
#ifdef MINI_CITY_JOLT
            if(debug_menu::flyMode){
                player.x=std::clamp(player.x+playerVelocity.x*dt,0.0f,regions::WIDTH);
                player.z=std::clamp(player.z+playerVelocity.z*dt,0.0f,regions::DEPTH);
                float rise=float(keys[VK_SPACE])-float(keys[VK_CONTROL]);
                playerY=std::clamp(playerY+rise*(keys[VK_SHIFT]?250.0f:160.0f)*dt,
                    0.0f,2500.0f);
                playerVerticalSpeed=0;grounded=false;swimming=false;airTime=0;
                jolt_world::teleportCharacter(player,playerY);
            }else jolt_world::moveCharacter(playerVelocity,keys[VK_SPACE],dt);
#else
            move(player,playerVelocity*dt,11,Kind::Car);
#endif
            if(std::abs(player.x-oldPlayer.x)<0.001f)playerVelocity.x=0;
            if(std::abs(player.z-oldPlayer.z)<0.001f)playerVelocity.z=0;
#ifndef MINI_CITY_JOLT
            physics::stepCharacterVertical(dt,keys[VK_SPACE]);
#endif
            if(len(input)>0.01f){
                walkPhase+=dt*(running?13.0f:crouched||slowWalk?5.0f:8.0f);
                stepTimer-=dt;
                if(stepTimer<=0){audio::play(swimming?audio::Effect::Splash:audio::Effect::Step,
                    player.z>=BEACH_START?1:0);
                    stepTimer=running?0.29f:crouched||slowWalk?0.62f:0.43f;}
            }else stepTimer=0;
            if(leftMouse&&(rightMouse||weapons::stats(weapon).melee||keys[VK_SPACE]))shoot();
            }
        }else{
            Vehicle& v=vehicles[occupied];
            engineSoundTime-=dt;
            if(engineSoundTime<=0){audio::play(v.kind==Kind::Boat?audio::Effect::Splash:audio::Effect::Engine,
                int(std::abs(v.speed)/75));engineSoundTime=v.kind==Kind::Boat?0.65f:0.32f;}
            float throttle=float(keys[ui::bindings[int(ui::Action::Forward)]])-
                float(keys[ui::bindings[int(ui::Action::Backward)]]);
            float turn=float(keys[ui::bindings[int(ui::Action::Right)]])-float(keys[ui::bindings[int(ui::Action::Left)]]);
            bool handbrake=keys[VK_SPACE]&&v.kind!=Kind::Boat;
            if(v.kind!=Kind::Boat&&std::abs(turn)>0.5f&&std::abs(v.speed)>145&&skidSoundTime<=0){
                audio::play(audio::Effect::Skid);skidSoundTime=0.45f;
            }
#ifdef MINI_CITY_JOLT
            if(v.kind==Kind::Boat)physics::stepVehicle(v,throttle,turn,dt);
            jolt_world::driveVehicle(std::size_t(occupied),throttle,turn,dt);
#else
            Vec2 delta=physics::stepVehicle(v,throttle,turn,dt);
            Vec2 before=v.p;move(v.p,delta,v.kind==Kind::Boat?20.0f:18.0f,v.kind);
            bool vehicleHit=false;
            for(int other=0;other<int(vehicles.size());++other){
                if(other==occupied)continue;
                Vehicle& target=vehicles[other];
                if((v.kind==Kind::Boat)!=(target.kind==Kind::Boat))continue;
                float radius=v.kind==Kind::Bike||target.kind==Kind::Bike?27.0f:39.0f;
                if(len(v.p-target.p)<radius){vehicleHit=true;v.p=before;
                    float severity=std::abs(v.speed);
                    damageVehicle(occupied,severity*0.20f);
                    damageVehicle(other,severity*0.09f);
                    if(severity>115){applyDamage((severity-100)*0.045f);
                        audio::play(audio::Effect::Hit);}
                    break;
                }
            }
            if(vehicleHit||len(v.p-before)<len(delta)*0.35f){
                float impact=std::abs(v.speed);
                damageVehicle(occupied,std::max(0.0f,impact-55)*0.12f);
                if(impact>135){applyDamage((impact-100)*0.06f);
                    audio::play(audio::Effect::Hit);}
                v.velocity=v.velocity*0.12f;v.speed*=0.12f;
            }
#endif
            int driftReward=physics::updateDrift(v,turn,handbrake,dt);
            if(driftReward>0){
                creditMoney(driftReward);
                announce("DRIFT +$"+std::to_string(driftReward),3);
                savegame::save();
            }
            if(occupied>=0){
                player=v.p;
#ifndef MINI_CITY_JOLT
                float difference=std::atan2(std::sin(v.angle-cameraYaw),
                    std::cos(v.angle-cameraYaw));
                if(vehicleLookTime<=0&&!leftMouse)
                    cameraYaw+=difference*std::min(1.0f,dt*5.0f);
#endif
                if(leftMouse)shoot();
            }
        }
    }
#ifdef MINI_CITY_JOLT
    const auto& trafficRoads=regions::roads();
    for(int index=0;index<int(vehicles.size());++index){
        Vehicle& car=vehicles[index];
        if(car.trafficRoute<0||car.trafficRoute>=int(trafficRoads.size())||
           car.exploded||car.owned||index==occupied)continue;
        const auto& road=trafficRoads[car.trafficRoute];
        Vec2 axis=road.end-road.start;
        float length=len(axis);
        if(length<1500)continue;
        Vec2 tangent=axis*(1.0f/length);
        Vec2 lane{-tangent.z,tangent.x};
        Vec2 end=road.end+lane*12.0f;
        float distanceToPlayer=len(car.p-player);
        if(len(car.p-end)<110.0f){
            if(distanceToPlayer>650.0f){
                Vec2 restart=road.start+axis*0.08f+lane*12.0f;
                jolt_world::teleportVehicle(index,restart,std::atan2(axis.z,axis.x));
            }else jolt_world::driveVehicle(index,car.speed>10?-0.5f:0.0f,0,dt);
            continue;
        }
        if(distanceToPlayer>850.0f){
            jolt_world::driveVehicle(index,0,0,dt);continue;
        }
        float progress=std::clamp((car.p.x-road.start.x)*tangent.x+
            (car.p.z-road.start.z)*tangent.z,0.0f,length);
        Vec2 target=road.start+tangent*std::min(length,progress+145.0f)+lane*12.0f;
        Vec2 direction=target-car.p;
        float desired=std::atan2(direction.z,direction.x);
        float error=std::atan2(std::sin(desired-car.angle),
            std::cos(desired-car.angle));
        float steering=std::clamp(error*1.8f,-0.7f,0.7f);
        float throttle=std::abs(error)>1.0f?0.25f:car.speed<105?0.72f:0.08f;
        jolt_world::driveVehicle(index,throttle,steering,dt);
    }
#endif
    airTime=occupied>=0||grounded||swimming?0:airTime+dt;
    if(carriedPed>=0){
        if(health<=0||occupied>=0)carryDrop();
        else{
            Ped& body=peds[carriedPed];
            body.p=player+forward(cameraYaw)*18;
            body.angle=cameraYaw;
        }
    }
    auto physicsEnd=std::chrono::steady_clock::now();
    police::update(dt);
    ai::update(dt);
    activeAi=ai::activeCount();
    for(auto& pickup:pickups){
        if(!pickup.available){pickup.respawn-=dt;if(pickup.respawn<=0)pickup.available=true;continue;}
        if(occupied<0&&len(player-pickup.p)<24){
            pickup.available=false;pickup.respawn=45;
            unlocked[pickup.weapon]=true;
            if(weapons::stats(pickup.weapon).melee){
                ammo[pickup.weapon]=-1;magazine[pickup.weapon]=1;
            }else ammo[pickup.weapon]+=weapons::stats(pickup.weapon).reservePickup;
            if(magazine[pickup.weapon]==0){int loaded=std::min(ammo[pickup.weapon],weapons::stats(pickup.weapon).magazine);
                magazine[pickup.weapon]=loaded;ammo[pickup.weapon]-=loaded;}
            weapon=pickup.weapon;announce(std::string("PICKED UP ")+weapons::stats(weapon).name+" + AMMO",4);
            audio::play(audio::Effect::Pickup);
            savegame::save();
        }
    }
    for(auto& bullet:bullets){
        if(bullet.life<=0)continue;
        Vec3 next=bullet.p+bullet.v*dt;
        Vec3 wallPoint{};
        bool wallHit=bulletSolidSegment(bullet.p,next,wallPoint);
        if(wallHit)next=wallPoint;
        Vec3 impactPoint=next;
        int samples=std::clamp(int(std::ceil(len(next-bullet.p)/4.0f)),1,64);
        Vec2 horizontalStart{bullet.p.x,bullet.p.z};
        Vec2 horizontalEnd{next.x,next.z};
        const auto treeCandidates=regions::nearbyTreeIndices(
            (horizontalStart+horizontalEnd)*0.5f,
            len(horizontalEnd-horizontalStart)*0.5f+25.0f);
        if(bullet.streamType==0){
            float nearest=1.0f;
            bool dynamicHit=false;
            auto consider=[&](Vec2 center,float radius,float low,float high){
                float entry=0;
                if(bulletCylinderSegment(bullet.p,next,center,radius,low,high,entry)&&
                   entry<nearest){nearest=entry;dynamicHit=true;}
            };
            for(const auto& prop:props)if(prop.alive)
                consider(prop.p,prop.barrel?12.0f:14.0f,prop.y,prop.y+25);
            for(int index=0;index<int(vehicles.size());++index){
                const auto& car=vehicles[index];
                if(car.exploded||(!bullet.hostile&&index==occupied))continue;
                consider(car.p,car.kind==Kind::Bike?14.0f:
                    car.kind==Kind::Boat?25.0f:26.0f,
                    car.rideHeight+2,car.rideHeight+42);
            }
            for(int index:treeCandidates){
                if(index<0||std::size_t(index)>=trees.size())continue;
                const auto& tree=trees[index];
                if(!tree.destroyed)
                    consider(tree.p,6.0f*std::min(tree.scale,4.0f),
                        0,tree.height*tree.scale*0.6f);
            }
            if(bullet.hostile){
                if(health>0)consider(player,occupied>=0?19.0f:11.0f,
                    2,occupied>=0?43.0f:playerY+37);
            }else{
                if(activeMission>=0&&(missions[activeMission].kind==MissionKind::Targets||
                   (missions[activeMission].kind==MissionKind::Finale&&missionStep==1))&&
                   missionStep<int(missions[activeMission].goals.size()))
                    consider(missions[activeMission].goals[missionStep],18.0f,0,45);
                for(const auto& ped:peds)if(ped.alive)
                    consider(ped.p,10.0f,2,37);
            }
            if(dynamicHit){
                Vec3 travel=next-bullet.p;
                float distance=len(travel);
                float inside=distance>0?std::min(0.05f/distance,(1-nearest)*0.5f):0;
                next=bullet.p+travel*(nearest+inside);
                wallHit=false;
            }
            samples=1;
        }
        for(int n=1;n<=samples&&bullet.life>0;++n){
            Vec3 point=bullet.p+(next-bullet.p)*(float(n)/samples);
            impactPoint=point;
            if(n==samples&&wallHit){
                applyStreamEffect(bullet,point);
                spawnHitFlash(bullet,point);
                bullet.life=0;break;
            }
            if(n==samples&&bullet.streamType>0)applyStreamEffect(bullet,point);
            int damage=std::max(1,int(bullet.damage*(1.0f-bullet.falloff*
                std::clamp((bullet.distance+len(point-bullet.p))/bullet.range,0.0f,1.0f))));
            if(props::hit(point,bullet.v,bullet.streamType>=2?0:damage)){
                applyStreamEffect(bullet,point);
                spawnHitFlash(bullet,point);
                bullet.life=0;audio::playAt(audio::Effect::Hit,point.x,point.z);break;}
            bool hitVehicle=false;
            for(int vehicleIndex=0;vehicleIndex<int(vehicles.size());++vehicleIndex){
                const Vehicle& car=vehicles[vehicleIndex];
                if(car.exploded||(!bullet.hostile&&vehicleIndex==occupied))continue;
                float radius=car.kind==Kind::Bike?14.0f:car.kind==Kind::Boat?25.0f:26.0f;
                if(point.y<car.rideHeight+2||point.y>car.rideHeight+42||
                   len(Vec2{point.x-car.p.x,point.z-car.p.z})>=radius)continue;
                applyStreamEffect(bullet,point);
                spawnHitFlash(bullet,point);
                if(bullet.streamType==1)fire::igniteVehicle(vehicles[vehicleIndex]);
                else if(bullet.streamType==2||bullet.streamType==3)
                    vehicles[vehicleIndex].burnTime=0;
                damageVehicle(vehicleIndex,bullet.rocket?
                    physics::tuning(car.kind).maxHealth:
                    bullet.streamType>=2?0:
                    damage*physics::tuning(car.kind).bulletDamageScale);
                impacts.push_back({{point.x,point.z},0.55f,false});
                bullet.life=0;hitVehicle=true;
                audio::playAt(audio::Effect::Hit,point.x,point.z);break;
            }
            if(hitVehicle)break;
            bool hitTree=false;
            for(int treeIndex:treeCandidates){
                if(treeIndex<0||std::size_t(treeIndex)>=trees.size())continue;
                Tree& tree=trees[treeIndex];
                if(tree.destroyed||point.y<0||
                   point.y>tree.height*tree.scale*0.6f||
                   len(tree.p-Vec2{point.x,point.z})>=
                       6.0f*std::min(tree.scale,4.0f))continue;
                applyStreamEffect(bullet,point);
                spawnHitFlash(bullet,point);
                if(bullet.streamType==1){
                    tree.burning=fire::ignite(tree.p,fire::Material::Wood);
                }else if(bullet.streamType==0){
                    tree.health=std::max(0,tree.health-
                        (bullet.rocket?std::max(30,damage):damage));
                    if(tree.health==0){tree.destroyed=true;tree.burning=false;}
                }
                bullet.life=0;hitTree=true;
                audio::playAt(audio::Effect::Hit,point.x,point.z);break;
            }
            if(hitTree)break;
            if(bullet.hostile){
                float radius=occupied>=0?19.0f:11.0f;
                float height=occupied>=0?43.0f:playerY+37;
                if(health>0&&len(Vec2{player.x-point.x,player.z-point.z})<radius&&
                    point.y>=2&&point.y<=height){
                    int zoneDamage=point.y>playerY+25?damage*2:point.y<playerY+11?damage/2:damage;
                    applyDamage(float(zoneDamage));
                    spawnHitFlash(bullet,point,true);
                    if(occupied>=0)damageVehicle(occupied,damage*0.5f);
                    impacts.push_back({{point.x,point.z},1.8f,true});bullet.life=0;
                    audio::playAt(audio::Effect::Hit,point.x,point.z);break;
                }
                continue;
            }
            if(activeMission>=0&&(missions[activeMission].kind==MissionKind::Targets||
                (missions[activeMission].kind==MissionKind::Finale&&missionStep==1))&&
               missionStep<int(missions[activeMission].goals.size())&&
               len(Vec2{point.x,point.z}-missions[activeMission].goals[missionStep])<18&&
               point.y>0&&point.y<45){
                spawnHitFlash(bullet,point);
                bullet.life=0;++missionStep;
                if(activeMission>=0&&missionStep<int(missions[activeMission].goals.size()))
                    missionCard("TARGET HIT",missionObjective(),2);
                audio::playAt(audio::Effect::Hit,point.x,point.z);
                if(missionStep==int(missions[activeMission].goals.size()))completeMission();
                else if(missions[activeMission].kind==MissionKind::Finale)
                    announce("Target hit! Board a boat and follow the harbor marker.",5);
                else announce("Target hit! Find the next orange target.",3);
                break;
            }
            for(auto& ped:peds)if(ped.alive&&len(Vec2{ped.p.x-point.x,ped.p.z-point.z})<10&&
                point.y>=2&&point.y<=37){
                applyStreamEffect(bullet,point);
                spawnHitFlash(bullet,point,true);
                if(bullet.streamType==1)fire::ignitePed(ped);
                else if(bullet.streamType==2||bullet.streamType==3)ped.burnTime=0;
                Vec2 side{-std::sin(ped.angle),std::cos(ped.angle)};
                float lateral=(point.x-ped.p.x)*side.x+(point.z-ped.p.z)*side.z;
                bool headshot=bullet.streamType==0&&point.y>=29;
                int zoneDamage=bullet.streamType==2?0:bullet.streamType==3?1:
                    point.y<11?std::max(1,damage/2):
                    std::abs(lateral)>5?std::max(1,damage*2/3):damage;
                if(headshot)ped.health=0;
                else{
                    int absorbed=std::min(ped.armor,zoneDamage);
                    ped.armor-=absorbed;
                    ped.health-=zoneDamage-absorbed;
                }
                ped.hitFlash=0.24f;
                ped.knockback=ped.knockback+norm(Vec2{bullet.v.x,bullet.v.z})*
                    (bullet.streamType==3?230.0f:70.0f);
                if(bullet.streamType==3)ped.knockedDown=3.0f;
                if(bullet.streamType!=2)ai::reactToHit(ped,player);
                if(solid(ped.target,12))ped.target=ped.p;
                if(ped.health<=0){ped.alive=false;ped.respawn=ped.police?999999:45;
                    ped.corpseVisualDelay=6;
                    if(headshot)announce("HEADSHOT",1.5f);
                    if(bullet.arrow){
                        Vec2 travel=norm(Vec2{bullet.v.x,bullet.v.z});
                        for(float probe=12;probe<=30;probe+=6){
                            Vec3 wall{ped.p.x+travel.x*probe,20,
                                ped.p.z+travel.z*probe};
                            if(bulletSolid(wall)){
                                ped.pinned=true;
                                ped.pinAnchor={wall.x-travel.x*2,wall.z-travel.z*2};
                                ped.corpseVisualDelay=15;
#ifdef MINI_CITY_JOLT
                                jolt_world::spawnRagdoll(ped,bullet.v,&ped.pinAnchor);
#endif
                                break;
                            }
                        }
                    }
                    if(!ped.pinned)spawnDebris(ped,bullet.v);
                    recordArmedKill(ped,bullet.sourceWeapon);}
                police::report(ped.police?police::Crime::AttackOfficer:
                    ped.alive?police::Crime::Assault:police::Crime::Murder,
                    {point.x,point.z},bullet.silent,ped.alive||ped.police);
                impacts.push_back({{point.x,point.z},bullet.streamType==0?1.8f:0.7f,
                    bullet.streamType==0});bullet.life=0;
                audio::playAt(audio::Effect::Hit,point.x,point.z);break;}
        }
        bullet.distance+=len(next-bullet.p);
        bullet.p=next;bullet.v.y-=bullet.gravity*dt;bullet.life-=dt;
        if(bullet.rocket&&bullet.life<=0)
            explodeAt(impactPoint,bullet.explosionRadius,bullet.explosionDamage);
    }
    bullets.erase(std::remove_if(bullets.begin(),bullets.end(),[](const Bullet& b){return b.life<=0;}),bullets.end());
    for(auto& impact:impacts)impact.life-=dt;
    impacts.erase(std::remove_if(impacts.begin(),impacts.end(),[](const Impact& i){return i.life<=0;}),impacts.end());
    if(impacts.size()>128)impacts.erase(impacts.begin(),impacts.end()-128);
    for(auto& flash:hitFlashes)flash.life-=dt;
    hitFlashes.erase(std::remove_if(hitFlashes.begin(),hitFlashes.end(),
        [](const HitFlash& flash){return flash.life<=0;}),hitFlashes.end());
    for(auto& blast:blasts)blast.life-=dt;
    blasts.erase(std::remove_if(blasts.begin(),blasts.end(),[](const Blast& b){return b.life<=0;}),blasts.end());
    fire::update(dt);
    auto propsBegin=std::chrono::steady_clock::now();
    props::update(dt);
#ifdef MINI_CITY_JOLT
    // Jolt writes the current chassis pose during props::update. Follow that pose,
    // rather than the previous tick's angle used while collecting drive input.
    if(occupied>=0&&occupied<int(vehicles.size())){
        const Vehicle& driven=vehicles[occupied];
        player=driven.p;
        float difference=std::atan2(std::sin(driven.angle-cameraYaw),
            std::cos(driven.angle-cameraYaw));
        if(vehicleLookTime<=0&&!leftMouse)
            cameraYaw+=difference*std::min(1.0f,dt*9.0f);
    }
#endif
    auto propsEnd=std::chrono::steady_clock::now();
    float elapsedPhysics=std::chrono::duration<float,std::milli>(physicsEnd-physicsBegin).count()+
        std::chrono::duration<float,std::milli>(propsEnd-propsBegin).count();
    physicsMs=physicsMs*0.9f+elapsedPhysics*0.1f;
    for(auto& part:debris){
        part.life-=dt;part.v.y-=530*dt;part.p=part.p+part.v*dt;
        if(part.p.y<part.h*0.5f){part.p.y=part.h*0.5f;part.v.y=std::abs(part.v.y)*0.25f;
            part.v.x*=0.72f;part.v.z*=0.72f;}
        part.rotation+=part.spin*dt;
    }
    debris.erase(std::remove_if(debris.begin(),debris.end(),[](const Debris& p){return p.life<=0;}),debris.end());
    if(occupied<0&&health>0&&invulnerable<=0)for(const auto& v:vehicles){
        if(std::abs(v.speed)>70&&len(v.p-player)<25){applyDamage(30);invulnerable=1;
            audio::play(audio::Effect::Hit);break;}
    }
    if(activeMission>=0){
        missionTime-=dt;
        if(missionTime<=0||health<=0){activeMission=-1;missionStep=0;announce("MISSION FAILED. Return to a marker to retry.",5);
            missionCard("MISSION FAILED",health<=0?"You were defeated":"Time ran out",4);
            audio::play(audio::Effect::Fail);}
        else{
            const auto& m=missions[activeMission];
            if(m.kind!=MissionKind::Targets&&
                !(m.kind==MissionKind::Finale&&missionStep==1)&&missionStep<int(m.goals.size())){
                bool rightVehicle=m.kind==MissionKind::Collect?occupied<0:
                    occupied>=0&&((m.kind==MissionKind::Boat||
                    (m.kind==MissionKind::Finale&&missionStep==2))?vehicles[occupied].kind==Kind::Boat:
                    m.kind==MissionKind::Bike?vehicles[occupied].kind==Kind::Bike:
                    (vehicles[occupied].kind==Kind::Car||vehicles[occupied].kind==Kind::SportCar));
                if(rightVehicle&&len(player-m.goals[missionStep])<45){
                    ++missionStep;
                    if(missionStep<int(m.goals.size()))
                        missionCard("CHECKPOINT REACHED",missionObjective(),2.5f);
                    if(missionStep==int(m.goals.size()))completeMission();
                    else if(m.kind==MissionKind::Finale)
                        announce("Exit the car and shoot the beach target, then take a boat.",5);
                    else announce("Checkpoint reached! Follow the next map marker.",3);
                }
            }
        }
    }
}


} // namespace game

