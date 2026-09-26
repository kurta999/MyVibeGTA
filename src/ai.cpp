#include "ai.h"
#include "regions.h"
#include "game_internal.h"
#include "audio.h"
#include "weapons.h"
#include "content.h"
#include "police.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif
#include <algorithm>
#include <cmath>

namespace ai {
using namespace game;
namespace {
bool findCover(const Ped& ped,Vec2 threat,Vec2& destination){
    float best=260;
    for(const auto& building:buildings){
        Vec2 corners[]={{building.x-21,building.z-21},{building.x+building.w+21,building.z-21},
            {building.x-21,building.z+building.d+21},{building.x+building.w+21,building.z+building.d+21}};
        for(Vec2 candidate:corners){
            float distance=len(candidate-ped.p);
            if(distance>=best||solid(candidate,12)||clearLine(threat,candidate))continue;
            best=distance;destination=candidate;
        }
    }
    return best<260;
}
}

int notifyThreat(Vec2 origin,float facing){
    int frightened=0;Vec2 aim=forward(facing);
    for(auto& ped:peds)if(ped.alive&&!ped.police){
        Vec2 toward=ped.p-origin;float distance=len(toward);
        if(distance>180||distance<1||
           aim.x*toward.x/distance+aim.z*toward.z/distance<0.45f||
           !clearLine(origin,ped.p))continue;
        ped.panic=6;ped.alertTime=6;ped.state=PedState::Flee;
        ped.target=ped.p+norm(toward)*220;
        ++frightened;
    }
    return frightened;
}
void notifyGunshot(Vec2 origin){
    for(auto& ped:peds)if(ped.alive){
        float distance=len(ped.p-origin);
        if(distance>360)continue;
        ped.alertTime=ped.armed?7.0f:4.0f;
        ped.panic=ped.alertTime;
        ped.lastKnown=origin;ped.sightMemory=ped.armed?4.0f:0;
        if(ped.armed){
            ped.hostile=true;
            ped.state=findCover(ped,origin,ped.target)?PedState::TakeCover:PedState::Attack;
        }else if(ped.style%3==0){
            ped.state=PedState::Investigate;ped.target=origin;
        }else{
            ped.state=PedState::Flee;
            ped.target=ped.p+norm(ped.p-origin)*220;
        }
    }
}

void reactToHit(Ped& ped,Vec2 threat){
    ped.hostile=true;
    ped.panic=5;ped.alertTime=5;
    ped.lastKnown=threat;ped.sightMemory=4;
    ped.fireCooldown=std::max(ped.fireCooldown,0.28f);
    if(ped.armed)
        ped.state=findCover(ped,threat,ped.target)?PedState::TakeCover:PedState::Attack;
    else{
        ped.state=PedState::Attack;
        ped.target=threat;
    }
}

bool vehicleImpact(Ped& ped,const Vehicle& vehicle){
    if(!ped.alive||vehicle.exploded||vehicle.kind==Kind::Boat||
       ped.vehicleImpactCooldown>0)return false;
    float speed=std::abs(vehicle.speed);
    float radius=vehicle.kind==Kind::Bike?17.0f:26.0f;
    if(speed<20||len(ped.p-vehicle.p)>=radius)return false;
    Vec2 travel=norm(vehicle.velocity);
    if(len(travel)<0.01f)travel=forward(vehicle.angle)*(vehicle.speed<0?-1.0f:1.0f);
    int damage=std::max(1,int((speed-20.0f)*1.2f));
    if(speed>=130)damage=std::max(damage,ped.health+ped.armor);
    int absorbed=std::min(ped.armor,damage);
    ped.armor-=absorbed;
    ped.health-=damage-absorbed;
    ped.hitFlash=0.45f;
    ped.knockback=ped.knockback+travel*std::min(550.0f,speed*3.2f);
    ped.vehicleImpactCooldown=0.8f;
    impacts.push_back({ped.p,0.7f,true});
    if(ped.health<=0){
        ped.alive=false;ped.respawn=ped.police?999999:45;
        ped.corpseVisualDelay=6;
        spawnDebris(ped,{travel.x*speed*12.0f,speed*3.0f,travel.z*speed*12.0f});
    }else{
        ped.impactAnimationTotal=speed>=40?std::clamp(speed/65.0f,0.8f,2.2f):0;
        ped.knockedDown=ped.impactAnimationTotal;
        reactToHit(ped,vehicle.p);
    }
    police::report(ped.police?police::Crime::AttackOfficer:
        ped.alive?police::Crime::Assault:police::Crime::Murder,ped.p,false,ped.police);
    return true;
}

void update(float dt){
    for(auto& ped:peds){
        ped.vehicleImpactCooldown=std::max(0.0f,ped.vehicleImpactCooldown-dt);
        if(ped.alive&&occupied>=0&&occupied<int(vehicles.size())&&
           vehicleImpact(ped,vehicles[occupied]))continue;
        if(!ped.alive){if(!ped.carried)ped.respawn-=dt;
            ped.corpseVisualDelay=std::max(0.0f,ped.corpseVisualDelay-dt);
            if(ped.respawn<=0){
#ifdef MINI_CITY_JOLT
            jolt_world::removeRagdoll(ped.id);
#endif
            if(!regions::homeForPed(ped.id,ped.p))ped.p=randomWalkable();
            ped.target=ped.p;
            ped.alive=true;ped.health=100;ped.panic=0;ped.hostile=false;ped.fireCooldown=0;
            ped.burnTime=0;
            ped.state=PedState::Wander;ped.alertTime=0;ped.knockback={};
            ped.sightMemory=0;ped.tacticTimer=0;ped.burstShots=0;
            ped.attackVisualTime=0;ped.hitFlash=0;
            ped.armor=ped.maxArmor;ped.cash=content::rollPedCash(ped.armed);
            ped.looted=false;ped.carried=false;ped.knockedDown=0;
            ped.impactAnimationTotal=0;ped.vehicleImpactCooldown=0;
            ped.pinned=false;ped.pinAnchor={};}continue;}
        ped.hitFlash=std::max(0.0f,ped.hitFlash-dt);
        ped.attackVisualTime=std::max(0.0f,ped.attackVisualTime-dt);
        if(!ped.police&&!ped.hostile&&ped.burnTime<=0&&ped.state==PedState::Wander&&
           len(ped.p-player)>1200)continue;
        if(ped.knockedDown>0){
            ped.knockedDown=std::max(0.0f,ped.knockedDown-dt);
            Vec2 push=ped.knockback*dt;
            ped.knockback=ped.knockback*std::exp(-4.0f*dt);
#ifdef MINI_CITY_JOLT
            if(len(ped.p-player)<420)
                jolt_world::movePed(std::size_t(&ped-peds.data()),push*(1.0f/dt),dt);
            else move(ped.p,push,9,Kind::Car);
#else
            move(ped.p,push,9,Kind::Car);
#endif
            continue;
        }
        ped.panic=std::max(0.0f,ped.panic-dt);
        ped.alertTime=std::max(0.0f,ped.alertTime-dt);
        ped.fireCooldown=std::max(0.0f,ped.fireCooldown-dt);
        ped.tacticTimer=std::max(0.0f,ped.tacticTimer-dt);
        ped.sightMemory=std::max(0.0f,ped.sightMemory-dt);
        if(ped.burnTime>0){
            ped.state=PedState::Flee;ped.hostile=false;
            ped.alertTime=std::max(ped.alertTime,1.0f);
            ped.panic=std::max(ped.panic,1.0f);
        }
        float distanceToPlayer=len(player-ped.p);
        bool canSee=ped.armed&&distanceToPlayer<360&&clearLine(ped.p,player);
        if(canSee){ped.lastKnown=player;ped.sightMemory=3.5f;}
        if(ped.state==PedState::Attack&&ped.hostile&&!ped.armed){
            if(ped.alertTime<=0){ped.hostile=false;ped.state=PedState::Wander;}
            else{
                ped.target=player;
                ped.angle=std::atan2(player.z-ped.p.z,player.x-ped.p.x);
                if(distanceToPlayer<23&&ped.fireCooldown<=0&&health>0){
                    applyDamage(8);
                    ped.fireCooldown=0.9f;
                    ped.attackVisualTime=0.42f;
                    audio::playAt(audio::Effect::Hit,player.x,player.z);
                }
            }
        }
        if(ped.state==PedState::Investigate&&
            (ped.alertTime<=0||len(ped.target-ped.p)<20))ped.state=PedState::Wander;
        if(ped.state==PedState::Flee){
            if(ped.alertTime<=0)ped.state=PedState::Wander;
            else if(len(ped.target-ped.p)<25||solid(ped.target,12))
                ped.target=ped.p+norm(ped.p-player)*170;
        }
        if(ped.state==PedState::TakeCover){
            if(len(ped.target-ped.p)<18)ped.state=PedState::Defend;
            if(distanceToPlayer<90)ped.state=PedState::Attack;
        }
        if(ped.state==PedState::Defend&&canSee&&distanceToPlayer<175)ped.state=PedState::Attack;
        if((ped.state==PedState::Attack||ped.state==PedState::Defend)&&
            ped.hostile&&ped.armed&&health>0){
            if(canSee){
                ped.angle=std::atan2(player.z-ped.p.z,player.x-ped.p.x);
                if(ped.state==PedState::Attack&&ped.tacticTimer<=0){
                    Vec2 toward=norm(player-ped.p),side{-toward.z,toward.x};
                    Vec2 destination=distanceToPlayer<105?ped.p-toward*95:
                        distanceToPlayer>235?player-toward*155:
                        ped.p+side*(ped.strafeRight?55.0f:-55.0f);
                    if(!solid(destination,12))ped.target=destination;
                    else ped.target=ped.lastKnown;
                    ped.strafeRight=!ped.strafeRight;
                    ped.tacticTimer=1.2f+float(ped.style%3)*0.25f;
                }
                if(distanceToPlayer<255&&ped.fireCooldown<=0){
                    const auto& pistol=weapons::stats(ped.weaponIndex);
                    ped.attackVisualTime=0.32f;
                    ped.fireCooldown=++ped.burstShots>=3?
                        std::max(1.35f,pistol.secondsBetweenShots*1.5f):
                        pistol.secondsBetweenShots*1.5f;
                    if(ped.burstShots>=3)ped.burstShots=0;
                    audio::playAt(audio::Effect::Shot,ped.p.x,ped.p.z,ped.weaponIndex);
                    Vec2 direction2=norm(player-ped.p);
                    Vec3 muzzle{ped.p.x+direction2.x*12,18,ped.p.z+direction2.z*12};
                    Vec3 target{player.x,occupied>=0?20.0f:playerY+18,player.z};
                    Vec3 direction3=norm(target-muzzle);
                    direction3=norm(Vec3{direction3.x+randf(-ped.accuracy,ped.accuracy),
                        direction3.y+randf(-ped.accuracy*0.55f,ped.accuracy*0.55f),
                        direction3.z+randf(-ped.accuracy,ped.accuracy)});
                    bullets.push_back({muzzle,direction3*pistol.projectileSpeed,
                        pistol.range/pistol.projectileSpeed,pistol.damage,pistol.gravity,
                        0,pistol.range,pistol.falloff,true});
                    announce("An armed pedestrian is firing at you!",2);
                }
            }else if(ped.sightMemory>0){
                ped.target=ped.lastKnown;
                if(ped.state==PedState::Defend&&ped.tacticTimer<=0){
                    ped.state=PedState::Investigate;ped.alertTime=3;
                }
            }else{ped.state=PedState::Wander;ped.hostile=false;}
        }
        if(ped.state==PedState::Wander&&(len(ped.target-ped.p)<7||randi(3000)==0)){
            for(int n=0;n<15;++n){Vec2 target=ped.p+Vec2{randf(-170,170),randf(-170,170)};
                if(!solid(target,12)){ped.target=target;break;}}
        }
        Vec2 step=norm(ped.target-ped.p)*ped.speed*(ped.panic>0?1.7f:1.0f)*dt+
            ped.knockback*dt;
        ped.knockback=ped.knockback*std::exp(-6.0f*dt);
        Vec2 old=ped.p;
#ifdef MINI_CITY_JOLT
        if(distanceToPlayer<420)
            jolt_world::movePed(std::size_t(&ped-peds.data()),step*(1.0f/dt),dt);
        else move(ped.p,step,9,Kind::Car);
#else
        move(ped.p,step,9,Kind::Car);
#endif
        if(len(ped.p-old)<0.1f)ped.target=ped.p;
        if(len(step)>0.001f)ped.angle=std::atan2(step.z,step.x);
    }
}

int activeCount(){
    return int(std::count_if(peds.begin(),peds.end(),[](const Ped& ped){
        return ped.alive&&(ped.police||ped.hostile||
            len(ped.p-player)<=1200);
    }));
}
}
