#include "ai.h"
#include "game_internal.h"
#include "audio.h"
#include "weapons.h"
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

void notifyGunshot(Vec2 origin){
    for(auto& ped:peds)if(ped.alive){
        float distance=len(ped.p-origin);
        if(distance>360)continue;
        ped.alertTime=ped.armed?7.0f:4.0f;
        ped.panic=ped.alertTime;
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
    ped.hostile=ped.armed;
    ped.panic=5;ped.alertTime=5;
    if(ped.armed)
        ped.state=findCover(ped,threat,ped.target)?PedState::TakeCover:PedState::Attack;
    else{
        ped.state=PedState::Flee;
        ped.target=ped.p+norm(ped.p-threat)*220;
    }
}

void update(float dt){
    for(auto& ped:peds){
        if(!ped.alive){ped.respawn-=dt;if(ped.respawn<=0){ped.p=randomWalkable();ped.target=ped.p;
            ped.alive=true;ped.health=100;ped.panic=0;ped.hostile=false;ped.fireCooldown=0;
            ped.state=PedState::Wander;ped.alertTime=0;ped.knockback={};}continue;}
        ped.panic=std::max(0.0f,ped.panic-dt);
        ped.alertTime=std::max(0.0f,ped.alertTime-dt);
        ped.hitFlash=std::max(0.0f,ped.hitFlash-dt);
        ped.fireCooldown=std::max(0.0f,ped.fireCooldown-dt);
        float distanceToPlayer=len(player-ped.p);
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
        if(ped.state==PedState::Defend&&distanceToPlayer<175)ped.state=PedState::Attack;
        if(ped.state==PedState::Attack&&ped.hostile&&ped.armed&&health>0){
            if(distanceToPlayer<300&&clearLine(ped.p,player)){
                ped.angle=std::atan2(player.z-ped.p.z,player.x-ped.p.x);
                ped.target=distanceToPlayer<115?ped.p:player;
                if(distanceToPlayer<255&&ped.fireCooldown<=0){
                    const auto& pistol=weapons::stats(0);
                    ped.fireCooldown=pistol.secondsBetweenShots*2.5f;
                    audio::playAt(audio::Effect::Shot,ped.p.x,ped.p.z,0);
                    Vec2 direction2=norm(player-ped.p);
                    Vec3 muzzle{ped.p.x+direction2.x*12,18,ped.p.z+direction2.z*12};
                    Vec3 target{player.x,occupied>=0?20.0f:playerY+18,player.z};
                    Vec3 direction3=norm(target-muzzle);
                    direction3=norm(Vec3{direction3.x+randf(-0.045f,0.045f),
                        direction3.y+randf(-0.025f,0.025f),
                        direction3.z+randf(-0.045f,0.045f)});
                    bullets.push_back({muzzle,direction3*pistol.projectileSpeed,
                        pistol.range/pistol.projectileSpeed,pistol.damage,pistol.gravity,
                        0,pistol.range,pistol.falloff,true});
                    announce("An armed pedestrian is firing at you!",2);
                }
            }else ped.target=ped.p;
        }
        if(ped.state==PedState::Wander&&(len(ped.target-ped.p)<7||randi(3000)==0)){
            for(int n=0;n<15;++n){Vec2 target=ped.p+Vec2{randf(-170,170),randf(-170,170)};
                if(!solid(target,12)){ped.target=target;break;}}
        }
        Vec2 step=norm(ped.target-ped.p)*ped.speed*(ped.panic>0?1.7f:1.0f)*dt+
            ped.knockback*dt;
        ped.knockback=ped.knockback*std::exp(-6.0f*dt);
        Vec2 old=ped.p;move(ped.p,step,9,Kind::Car);
        if(len(ped.p-old)<0.1f)ped.target=ped.p;
        if(len(step)>0.001f)ped.angle=std::atan2(step.z,step.x);
        if(occupied>=0&&std::abs(vehicles[occupied].speed)>55&&len(ped.p-player)<25){
            ped.alive=false;ped.respawn=6;impacts.push_back({ped.p,0.7f,true});
            spawnDebris(ped,{vehicles[occupied].velocity.x,0,vehicles[occupied].velocity.z});}
    }
}

int activeCount(){
    return int(std::count_if(peds.begin(),peds.end(),[](const Ped& ped){return ped.alive;}));
}
}
