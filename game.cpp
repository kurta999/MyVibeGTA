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
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
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
float cameraYaw=0,cameraPitch=0,renderAlpha=0,health=100,fireCooldown=0,invulnerable=0,walkPhase=0,stepTimer=0,muzzleFlash=0;
int weapon=0,occupied=-1,money=0,activeMission=-1,missionStep=0;
float missionTime=0,messageTime=0,worldTime=0,gameHour=16.5f;
float engineSoundTime=0,surfSoundTime=0,skidSoundTime=0,trafficSoundTime=0;
bool showMap=false;
bool debugHud=false;
float frameRate=0,frameMs=0,simulationMs=0,physicsMs=0;
int drawCalls=0,activeAi=0;
std::string message;
std::array<bool,5> unlocked{{true,false,false,false,false}};
std::array<int,5> ammo{{-1,0,0,0,0}};
std::array<int,5> magazine{{12,0,0,0,0}};
float reloadRemaining=0,recoil=0;
int reloadingWeapon=-1;
const char* weaponNames[]={"PISTOL","SMG","SHOTGUN","RIFLE","SNIPER"};
Vec2 player{300,250};
Vec2 previousPlayer{300,250};
Vec2 playerVelocity{};
float playerY=0,playerVerticalSpeed=0;
bool grounded=true;
std::vector<Building> buildings;
std::vector<Ped> peds;
std::vector<Vehicle> vehicles;
std::vector<Bullet> bullets;
std::vector<Impact> impacts;
std::vector<Debris> debris;
std::vector<RagdollPart> ragdollParts;
std::vector<Pickup> pickups;
std::vector<MissionDef> missions;
std::array<bool,6> missionDone{};

void announce(const std::string& s,float seconds=4){message=s;messageTime=seconds;}
void completeMission(){
    const auto& m=missions[activeMission];money+=m.reward;missionDone[activeMission]=true;
    health=std::min(100.0f,health+20);announce(std::string("MISSION PASSED: ")+m.name+"  +$"+std::to_string(m.reward),7);
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
    for(int i=0;i<int(missions.size());++i)if(len(missions[i].start-player)<75){
        if(i>0&&!missionDone[i-1]){
            announce(std::string("Finish ")+missions[i-1].name+" to unlock this mission.",4);
            return;
        }
        activeMission=i;missionStep=0;missionTime=missions[i].seconds;
        announce(std::string("MISSION STARTED: ")+missions[i].name,5);audio::play(audio::Effect::Pickup);return;
    }
}

bool inside(const Building& b,Vec2 p,float pad){
    return p.x>b.x-pad&&p.x<b.x+b.w+pad&&p.z>b.z-pad&&p.z<b.z+b.d+pad;
}
bool solid(Vec2 p,float radius){
    if(p.x<radius||p.x>WORLD_W-radius||p.z<radius||p.z>SHORE-radius)return true;
    for(const auto& b:buildings)if(inside(b,p,radius))return true;
    return false;
}
bool bulletSolid(Vec3 p){
    if(p.x<0||p.x>WORLD_W||p.z<0||p.z>WORLD_D||p.y<0)return true;
    for(const auto& b:buildings)if(p.y<b.h&&inside(b,{p.x,p.z},1))return true;
    return false;
}
bool valid(Vec2 p,Kind k,float radius){
    if(k==Kind::Boat)return p.x>=radius&&p.x<=WORLD_W-radius&&p.z>=SHORE+radius&&p.z<=WORLD_D-radius;
    return !solid(p,radius);
}
Vec2 randomWalkable(){
    for(int i=0;i<1000;++i){Vec2 p{randf(25,WORLD_W-25),randf(25,SHORE-25)};if(!solid(p,13))return p;}
    return {300,250};
}
void reset(){
    buildings.clear();peds.clear();vehicles.clear();bullets.clear();impacts.clear();debris.clear();ragdollParts.clear();pickups.clear();missions.clear();
    player={300,250};previousPlayer=player;playerVelocity={};playerY=0;playerVerticalSpeed=0;grounded=true;
    health=100;weapon=0;occupied=-1;fireCooldown=0;reloadRemaining=0;reloadingWeapon=-1;recoil=0;
    cameraYaw=0;cameraPitch=0;invulnerable=0;
    money=0;activeMission=-1;missionStep=0;missionTime=0;showMap=false;worldTime=0;gameHour=16.5f;
    walkPhase=0;stepTimer=0;muzzleFlash=0;engineSoundTime=0;surfSoundTime=0;
    skidSoundTime=0;trafficSoundTime=0;
    unlocked={{true,false,false,false,false}};ammo={{-1,0,0,0,0}};
    magazine={{weapons::stats(0).magazine,0,0,0,0}};missionDone.fill(false);
    announce("Find the colored mission markers. Press F to start; M opens the map.",8);
    content::populate();
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
    if(occupied>=0){
        Vehicle& v=vehicles[occupied];Vec2 side{-std::sin(v.angle),std::cos(v.angle)};
        if(v.kind==Kind::Boat){
            if(v.p.z<SHORE+105){Vec2 dock{v.p.x,SHORE-22};if(!solid(dock,12)){player=dock;occupied=-1;audio::play(audio::Effect::Splash);}}
            return;
        }
        for(int sign:{1,-1}){Vec2 out=v.p+side*(sign*(v.kind==Kind::Bike?27.0f:38.0f));
            if(!solid(out,12)){player=out;occupied=-1;return;}}
        return;
    }
    float best=85;int index=-1;
    for(int i=0;i<int(vehicles.size());++i){float d=len(vehicles[i].p-player);if(d<best){best=d;index=i;}}
    if(index>=0){occupied=index;player=vehicles[index].p;cameraYaw=vehicles[index].angle;
        playerY=0;playerVerticalSpeed=0;playerVelocity={};grounded=true;
        audio::play(vehicles[index].kind==Kind::Boat?audio::Effect::Splash:audio::Effect::Engine);}
}
void startReload(){
    if(occupied>=0||health<=0||reloadRemaining>0)return;
    const auto& stats=weapons::stats(weapon);
    if(magazine[weapon]>=stats.magazine||ammo[weapon]==0)return;
    reloadingWeapon=weapon;reloadRemaining=stats.reloadSeconds;
    audio::play(audio::Effect::Reload);
}
void shoot(){
    if(occupied>=0||health<=0||fireCooldown>0)return;
    if(reloadRemaining>0&&reloadingWeapon==weapon)return;
    if(magazine[weapon]==0){
        if(ammo[weapon]!=0)startReload();
        else announce("Out of ammo. Find a weapon pickup or switch guns.",2);
        fireCooldown=0.35f;return;
    }
    --magazine[weapon];
    const auto& stats=weapons::stats(weapon);
    audio::play(audio::Effect::Shot,weapon);
    ai::notifyGunshot(player);
    muzzleFlash=0.12f;
    recoil=std::min(1.0f,recoil+0.25f+(weapon==2?0.25f:0));
    Vec2 f=forward(cameraYaw),r{-f.z,f.x};
    camera::Pose pose=camera::compute(player,playerY,true,occupied);
    Vec3 target=camera::traceReticle(pose,stats.range);
    Vec3 muzzle{player.x+f.x*16+r.x*7,17+playerY,player.z+f.z*16+r.z*7};
    float flightTime=len(target-muzzle)/stats.projectileSpeed;
    Vec3 ballisticTarget=target;
    ballisticTarget.y+=0.5f*stats.gravity*flightTime*flightTime;
    for(int i=0;i<stats.pellets;++i){
        float spread=randf(-stats.spread,stats.spread)*(1.0f+recoil*0.3f)+
            (weapon==2?(i-3)*0.025f:0);
        Vec3 direction=norm(ballisticTarget-muzzle);
        direction=norm(Vec3{direction.x+std::cos(cameraYaw+PI/2)*spread,
            direction.y+randf(-stats.spread*0.5f,stats.spread*0.5f),
            direction.z+std::sin(cameraYaw+PI/2)*spread});
        bullets.push_back({muzzle,direction*stats.projectileSpeed,
            stats.range/stats.projectileSpeed,stats.damage,stats.gravity,0,stats.range,stats.falloff});
    }
    cameraPitch=std::clamp(cameraPitch+stats.recoilKick,-0.85f,0.8f);
    fireCooldown=stats.secondsBetweenShots;
}
void update(float dt){
    previousPlayer=player;
    audio::setListener(player.x,player.z,cameraYaw);
    dt=std::min(dt,0.05f);fireCooldown=std::max(0.0f,fireCooldown-dt);
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
    muzzleFlash=std::max(0.0f,muzzleFlash-dt);worldTime+=dt;
    surfSoundTime-=dt;skidSoundTime-=dt;trafficSoundTime-=dt;
    if(player.z>BEACH_START-110&&surfSoundTime<=0){audio::play(audio::Effect::Surf);surfSoundTime=2.0f;}
    if(trafficSoundTime<=0){
        trafficSoundTime=randf(3.0f,6.0f);
        for(int index=0;index<int(vehicles.size());++index){
            const auto& vehicle=vehicles[index];
            if(index==occupied||vehicle.kind==Kind::Boat||len(vehicle.p-player)>=450)continue;
            audio::playAt(audio::Effect::Traffic,vehicle.p.x,vehicle.p.z);break;
        }
    }
    gameHour=std::fmod(gameHour+dt*0.075f,24.0f);
    for(int i=0;i<5;++i)if(keys['1'+i]&&unlocked[i])weapon=i;
    if(keys[VK_LEFT])cameraYaw-=dt*1.8f;
    if(keys[VK_RIGHT])cameraYaw+=dt*1.8f;
    auto physicsBegin=std::chrono::steady_clock::now();
    if(health>0){
        if(occupied<0){
            Vec2 f=forward(cameraYaw),r{-f.z,f.x};
            Vec2 input=f*(float(keys[ui::bindings[int(ui::Action::Forward)]])-float(keys[ui::bindings[int(ui::Action::Backward)]]))+
                r*(float(keys[ui::bindings[int(ui::Action::Right)]])-float(keys[ui::bindings[int(ui::Action::Left)]]));
            Vec2 desired=norm(input)*(keys[ui::bindings[int(ui::Action::Sprint)]]?250.0f:160.0f);
            float response=grounded?10.0f:3.0f;
            playerVelocity=playerVelocity+(desired-playerVelocity)*std::min(1.0f,response*dt);
            Vec2 oldPlayer=player;move(player,playerVelocity*dt,11,Kind::Car);
            if(std::abs(player.x-oldPlayer.x)<0.001f)playerVelocity.x=0;
            if(std::abs(player.z-oldPlayer.z)<0.001f)playerVelocity.z=0;
            physics::stepCharacterVertical(dt,keys[VK_SPACE]);
            if(len(input)>0.01f){
                bool running=keys[ui::bindings[int(ui::Action::Sprint)]];walkPhase+=dt*(running?13.0f:8.0f);
                stepTimer-=dt;
                if(stepTimer<=0){audio::play(audio::Effect::Step,player.z>=BEACH_START?1:0);
                    stepTimer=running?0.29f:0.43f;}
            }else stepTimer=0;
            if(leftMouse&&rightMouse)shoot();
        }else{
            Vehicle& v=vehicles[occupied];
            engineSoundTime-=dt;
            if(engineSoundTime<=0){audio::play(v.kind==Kind::Boat?audio::Effect::Splash:audio::Effect::Engine,
                int(std::abs(v.speed)/75));engineSoundTime=v.kind==Kind::Boat?0.65f:0.32f;}
            float throttle=float(keys[ui::bindings[int(ui::Action::Forward)]])-
                float(keys[ui::bindings[int(ui::Action::Backward)]]);
            float turn=float(keys[ui::bindings[int(ui::Action::Right)]])-float(keys[ui::bindings[int(ui::Action::Left)]]);
            if(v.kind!=Kind::Boat&&std::abs(turn)>0.5f&&std::abs(v.speed)>145&&skidSoundTime<=0){
                audio::play(audio::Effect::Skid);skidSoundTime=0.45f;
            }
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
                    v.damage=std::min(100.0f,v.damage+severity*0.08f);
                    target.damage=std::min(100.0f,target.damage+severity*0.035f);
                    if(severity>115){health=std::max(0.0f,health-(severity-100)*0.045f);
                        audio::play(audio::Effect::Hit);}
                    break;
                }
            }
            if(vehicleHit||len(v.p-before)<len(delta)*0.35f){
                float impact=std::abs(v.speed);
                v.damage=std::min(100.0f,v.damage+std::max(0.0f,impact-55)*0.045f);
                if(impact>135){health=std::max(0.0f,health-(impact-100)*0.06f);
                    audio::play(audio::Effect::Hit);}
                v.velocity=v.velocity*0.12f;v.speed*=0.12f;
            }
            player=v.p;
            // The chase camera follows the vehicle smoothly around corners.
            float difference=std::atan2(std::sin(v.angle-cameraYaw),std::cos(v.angle-cameraYaw));
            cameraYaw+=difference*std::min(1.0f,dt*5.0f);
        }
    }
    auto physicsEnd=std::chrono::steady_clock::now();
    ai::update(dt);
    activeAi=ai::activeCount();
    for(auto& pickup:pickups){
        if(!pickup.available){pickup.respawn-=dt;if(pickup.respawn<=0)pickup.available=true;continue;}
        if(occupied<0&&len(player-pickup.p)<24){
            pickup.available=false;pickup.respawn=45;
            unlocked[pickup.weapon]=true;
            ammo[pickup.weapon]+=weapons::stats(pickup.weapon).reservePickup;
            if(magazine[pickup.weapon]==0){int loaded=std::min(ammo[pickup.weapon],weapons::stats(pickup.weapon).magazine);
                magazine[pickup.weapon]=loaded;ammo[pickup.weapon]-=loaded;}
            weapon=pickup.weapon;announce(std::string("PICKED UP ")+weaponNames[weapon]+" + AMMO",4);
            audio::play(audio::Effect::Pickup);
            savegame::save();
        }
    }
    for(auto& bullet:bullets){
        if(bullet.life<=0)continue;
        Vec3 next=bullet.p+bullet.v*dt;
        for(int n=1;n<=5&&bullet.life>0;++n){
            Vec3 point=bullet.p+(next-bullet.p)*(n/5.0f);
            if(bulletSolid(point)){bullet.life=0;break;}
            int damage=std::max(1,int(bullet.damage*(1.0f-bullet.falloff*
                std::clamp((bullet.distance+len(point-bullet.p))/bullet.range,0.0f,1.0f))));
            if(props::hit(point,bullet.v,damage)){
                bullet.life=0;audio::playAt(audio::Effect::Hit,point.x,point.z);break;}
            if(bullet.hostile){
                float radius=occupied>=0?19.0f:11.0f;
                float height=occupied>=0?43.0f:playerY+37;
                if(health>0&&len(Vec2{player.x-point.x,player.z-point.z})<radius&&
                    point.y>=2&&point.y<=height){
                    int zoneDamage=point.y>playerY+25?damage*2:point.y<playerY+11?damage/2:damage;
                    health=std::max(0.0f,health-zoneDamage);
                    if(occupied>=0)vehicles[occupied].damage=std::min(100.0f,vehicles[occupied].damage+5);
                    impacts.push_back({{point.x,point.z},0.7f,true});bullet.life=0;
                    audio::playAt(audio::Effect::Hit,point.x,point.z);break;
                }
                continue;
            }
            if(activeMission>=0&&(missions[activeMission].kind==MissionKind::Targets||
                (missions[activeMission].kind==MissionKind::Finale&&missionStep==1))&&
               missionStep<int(missions[activeMission].goals.size())&&
               len(Vec2{point.x,point.z}-missions[activeMission].goals[missionStep])<18&&
               point.y>0&&point.y<45){
                bullet.life=0;++missionStep;
                audio::playAt(audio::Effect::Hit,point.x,point.z);
                if(missionStep==int(missions[activeMission].goals.size()))completeMission();
                else if(missions[activeMission].kind==MissionKind::Finale)
                    announce("Target hit! Board a boat and follow the harbor marker.",5);
                else announce("Target hit! Find the next orange target.",3);
                break;
            }
            for(auto& ped:peds)if(ped.alive&&len(Vec2{ped.p.x-point.x,ped.p.z-point.z})<10&&
                point.y>=2&&point.y<=37){
                int zoneDamage=point.y>25?damage*2:point.y<11?damage/2:damage;
                int absorbed=std::min(ped.armor,zoneDamage/2);
                ped.armor-=absorbed;zoneDamage-=absorbed;
                ped.health-=zoneDamage;ped.hitFlash=0.24f;
                ped.knockback=ped.knockback+norm(Vec2{bullet.v.x,bullet.v.z})*70;
                ai::reactToHit(ped,player);
                if(solid(ped.target,12))ped.target=ped.p;
                if(ped.health<=0){ped.alive=false;ped.respawn=6;spawnDebris(ped,bullet.v);}
                impacts.push_back({{point.x,point.z},0.7f,true});bullet.life=0;
                audio::playAt(audio::Effect::Hit,point.x,point.z);break;}
        }
        bullet.distance+=len(next-bullet.p);
        bullet.p=next;bullet.v.y-=bullet.gravity*dt;bullet.life-=dt;
    }
    bullets.erase(std::remove_if(bullets.begin(),bullets.end(),[](const Bullet& b){return b.life<=0;}),bullets.end());
    for(auto& impact:impacts)impact.life-=dt;
    impacts.erase(std::remove_if(impacts.begin(),impacts.end(),[](const Impact& i){return i.life<=0;}),impacts.end());
    auto propsBegin=std::chrono::steady_clock::now();
    props::update(dt);
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
        if(std::abs(v.speed)>70&&len(v.p-player)<25){health=std::max(0.0f,health-30);invulnerable=1;
            audio::play(audio::Effect::Hit);break;}
    }
    if(activeMission>=0){
        missionTime-=dt;
        if(missionTime<=0||health<=0){activeMission=-1;missionStep=0;announce("MISSION FAILED. Return to a marker to retry.",5);
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

