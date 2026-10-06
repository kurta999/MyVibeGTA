#include "ai.h"
#include "regions.h"
#include "game_internal.h"
#include "audio.h"
#include "weapons.h"
#include "content.h"
#include "physics.h"
#include "police.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#include "traffic.h"
#include "ped_navigation.h"
#include "ped_navigation_surface.h"
#include "builder.h"
#endif
#include <algorithm>
#include <cmath>
#include <limits>

namespace ai {
using namespace game;
namespace {
#ifdef MINI_CITY_JOLT
void moveFar(Ped& ped,Vec2 step){
    auto before=ped.p;move(ped.p,step,9,Kind::Car);
    if(len(ped.p-before)>.001f)ped.elevationKnown=false;
}
#endif
float originHeight(Vec2 origin){
#ifdef MINI_CITY_JOLT
    if(len(origin-player)<.1f)return playerY;
    for(const auto& ped:peds)if(len(ped.p-origin)<.1f)return jolt_world::pedHeight(ped);
    for(const auto& vehicle:vehicles)if(len(vehicle.p-origin)<.1f)return vehicle.rideHeight;
#endif
    return groundHeight(origin);
}
bool actorLine(const Ped& ped,Vec2 target,float targetHeight){
#ifdef MINI_CITY_JOLT
    return clearLineAtHeight({ped.p.x,jolt_world::pedHeight(ped)+22,ped.p.z},{target.x,targetHeight+22,target.z});
#else
    return clearLine(ped.p,target);
#endif
}
bool sameFloor(const Ped& ped,float height,float tolerance=25){
#ifdef MINI_CITY_JOLT
    return std::abs(jolt_world::pedHeight(ped)-height)<tolerance;
#else
    return true;
#endif
}
bool escape(Ped& ped,Vec2 threat){
#ifdef MINI_CITY_JOLT
    if(builder::active()){
        Vec2 away=norm(ped.p-threat);if(len(away)<.01f)away=forward(ped.angle);
        for(float turn:{0.0f,.55f,-.55f,1.1f,-1.1f}){
            Vec2 direction{away.x*std::cos(turn)-away.z*std::sin(turn),away.x*std::sin(turn)+away.z*std::cos(turn)};
            Vec3 feet{};if(ped_navigation_surface::destination(ped,ped.p+direction*220,feet)&&
                (feet.x-ped.p.x)*away.x+(feet.z-ped.p.z)*away.z>8){ped.target={feet.x,feet.z};return true;}
        }
        ped.target=ped.p;ped.navigation={};return false;
    }
#endif
    ped.target=ped.p+norm(ped.p-threat)*220;return true;
}
bool findCover(Ped& ped,Vec2 threat,Vec2& destination){
#ifdef MINI_CITY_JOLT
    if(builder::active()){
        std::vector<Vec2> candidates;
        auto corners=[&](float x,float z,float w,float d){
            for(Vec2 p:{Vec2{x-21,z-21},Vec2{x+w+21,z-21},Vec2{x-21,z+d+21},Vec2{x+w+21,z+d+21},
                       Vec2{x-21,z+d/2},Vec2{x+w+21,z+d/2},Vec2{x+w/2,z-21},Vec2{x+w/2,z+d+21}})
                if(len(p-ped.p)<260)candidates.push_back(p);
        };
        for(const auto& building:buildings)corners(building.x,building.z,building.w,building.d);
        for(const auto& block:builder::blocks()){auto low=builder::cellLow(block.first);corners(low.x,low.z,40,40);}
        std::sort(candidates.begin(),candidates.end(),[&](Vec2 a,Vec2 b){return len(a-ped.p)<len(b-ped.p);});
        Vec3 eye{threat.x,originHeight(threat)+22,threat.z};
        for(Vec2 candidate:candidates){Vec3 feet{};
            if(ped_navigation_surface::destination(ped,candidate,feet,&eye)){destination={feet.x,feet.z};return true;}}
        return false;
    }
#endif
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
           !actorLine(ped,origin,originHeight(origin)))continue;
        ped.panic=6;ped.alertTime=6;ped.state=PedState::Flee;
        escape(ped,origin);
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
#ifdef MINI_CITY_JOLT
        ped.lastKnownHeight=originHeight(origin);
#endif
        if(ped.armed){
            ped.hostile=true;
            ped.state=findCover(ped,origin,ped.target)?PedState::TakeCover:PedState::Attack;
        }else if(ped.style%3==0){
            ped.state=PedState::Investigate;ped.target=origin;
        }else{
            ped.state=PedState::Flee;
            escape(ped,origin);
        }
    }
}

void reactToHit(Ped& ped,Vec2 threat){
#ifdef MINI_CITY_JOLT
    traffic::provoke(ped,threat);
#endif
    ped.hostile=true;
    ped.socialPartner=-1;ped.socialTime=0;
    ped.panic=5;ped.alertTime=5;
    ped.lastKnown=threat;ped.sightMemory=4;
#ifdef MINI_CITY_JOLT
    ped.lastKnownHeight=originHeight(threat);
#endif
    ped.fireCooldown=std::max(ped.fireCooldown,0.28f);
    if(ped.armed)
        ped.state=findCover(ped,threat,ped.target)?PedState::TakeCover:PedState::Attack;
    else{
        ped.state=PedState::Attack;
        ped.target=threat;
    }
}

void pedestrianContact(Ped& ped,float speed,Vec2 from){
    if(!ped.alive||ped.drivingVehicle>=0||ped.knockedDown>0)return;
    if(ped.contactVisualTime>0.18f)return;
    ped.contactVisualTime=0.42f;
    ped.angle=std::atan2(from.z-ped.p.z,from.x-ped.p.x);
    if(ped.state==PedState::Wander)ped.target=ped.p;
    if(speed>190){
        ped.knockedDown=ped.impactAnimationTotal=0.8f;
        ped.knockback=norm(ped.p-from)*85.0f;
    }
    playerContactVisualTime=std::max(playerContactVisualTime,0.26f);
}
bool talkToPed(int index){
    if(index<0||index>=int(peds.size())||health<=0||occupied>=0)return false;
#ifndef MINI_CITY_JOLT
    if(playerY>game::groundHeight(player)+12)return false;
#endif
    Ped& ped=peds[index];
    if(!ped.alive||ped.drivingVehicle>=0||ped.hostile||ped.police||
       ped.knockedDown>0||len(player-ped.p)>43||!sameFloor(ped,playerY,12)||!actorLine(ped,player,playerY))return false;
    ped.state=PedState::Talk;ped.socialPartner=-1;ped.socialTime=2.6f;
    ped.socialCooldown=8;ped.target=ped.p;
    ped.angle=std::atan2(player.z-ped.p.z,player.x-ped.p.x);
    playerTalkTime=2.6f;announce("You exchange a few words.",2);
    return true;
}
bool startSocial(int first,int second,bool fight){
    if(first<0||second<0||first==second||first>=int(peds.size())||
       second>=int(peds.size()))return false;
    Ped& a=peds[first];Ped& b=peds[second];
    if(!a.alive||!b.alive||a.drivingVehicle>=0||b.drivingVehicle>=0||
       a.police||b.police||a.hostile||b.hostile||a.knockedDown>0||b.knockedDown>0||
       a.state!=PedState::Wander||b.state!=PedState::Wander||
       len(a.p-b.p)>65||!sameFloor(a,game::pedGroundHeight(b),12)||!actorLine(a,b.p,game::pedGroundHeight(b)))return false;
    for(auto* ped:{&a,&b}){
        ped->state=fight?PedState::Fight:PedState::Talk;
        ped->socialTime=fight?7.0f:4.0f;
        ped->socialCooldown=fight?18.0f:10.0f;
        ped->target=ped->p;
    }
    a.socialPartner=second;b.socialPartner=first;
    return true;
}

bool vehicleImpact(Ped& ped,const Vehicle& vehicle){
    if(!ped.alive||ped.drivingVehicle>=0||vehicle.exploded||vehicle.kind==Kind::Boat||
       (vehicle.kind==Kind::Helicopter&&vehicle.rideHeight>37)||
       ped.vehicleImpactCooldown>0)return false;
#ifdef MINI_CITY_JOLT
    float floor=jolt_world::pedHeight(ped);
    auto half=physics::chassisHalf(vehicle.kind);float x=vehicle.qx,y=vehicle.qy,z=vehicle.qz,w=vehicle.qw;
    float extent=half.x*std::abs(2*(x*y+z*w))+half.y*std::abs(1-2*(x*x+z*z))+half.z*std::abs(2*(y*z-x*w));
    float center=vehicle.rideHeight+physics::vehicleRestHeight(vehicle.kind);
    if(floor+36<center-extent-2||floor>center+extent+2)return false;
#endif
    float speed=std::abs(vehicle.speed);
    float radius=vehicle.kind==Kind::Bike?17.0f:physics::vehicleRadius(vehicle.kind);
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

void update(float dt,bool beginNavigationFrame){
    if(dt<=0)return;
#ifdef MINI_CITY_JOLT
    if(beginNavigationFrame)ped_navigation::beginFrame();
#endif
    for(auto& ped:peds){
#ifdef MINI_CITY_JOLT
        if(!ped.alive)traffic::release(ped);
#endif
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
            ped.burnTime=0;ped.stunRemaining=0;
            ped.state=PedState::Wander;ped.alertTime=0;ped.knockback={};
            ped.sightMemory=0;ped.tacticTimer=0;ped.burstShots=0;
            ped.attackVisualTime=0;ped.hitFlash=0;
            ped.contactVisualTime=ped.socialTime=ped.socialCooldown=0;
            ped.socialPartner=-1;
            ped.armor=ped.maxArmor;ped.cash=content::rollPedCash(ped.armed);
            ped.looted=false;ped.carried=false;ped.knockedDown=0;
            ped.impactAnimationTotal=0;ped.vehicleImpactCooldown=0;
            ped.pinned=false;ped.pinAnchor={};
#ifdef MINI_CITY_JOLT
            ped.elevationKnown=false;ped.navigation={};
            jolt_world::teleportPed(std::size_t(&ped-peds.data()),ped.p,groundHeight(ped.p));
#endif
            }continue;}
        ped.hitFlash=std::max(0.0f,ped.hitFlash-dt);
        if(ped.stunRemaining>0){ped.stunRemaining=std::max(0.0f,ped.stunRemaining-dt);
            ped.fireCooldown=std::max(ped.fireCooldown,.3f);continue;}
        ped.attackVisualTime=std::max(0.0f,ped.attackVisualTime-dt);
        ped.contactVisualTime=std::max(0.0f,ped.contactVisualTime-dt);
        ped.socialCooldown=std::max(0.0f,ped.socialCooldown-dt);
        if(!ped.police&&!ped.hostile&&ped.grievance<=0&&ped.drivingVehicle<0&&
           ped.burnTime<=0&&ped.state==PedState::Wander&&
           len(ped.p-player)>1200)continue;
        if(ped.knockedDown>0){
            ped.knockedDown=std::max(0.0f,ped.knockedDown-dt);
            Vec2 push=ped.knockback*dt;
            ped.knockback=ped.knockback*std::exp(-4.0f*dt);
#ifdef MINI_CITY_JOLT
            if(builder::active()||len(ped.p-player)<420)
                jolt_world::movePed(std::size_t(&ped-peds.data()),push*(1.0f/dt),dt);
            else moveFar(ped,push);
#else
            move(ped.p,push,9,Kind::Car);
#endif
            continue;
        }
        if(ped.state==PedState::Talk||ped.state==PedState::Fight){
            ped.socialTime-=dt;
            bool withPlayer=ped.state==PedState::Talk&&ped.socialPartner<0;
            bool valid=withPlayer?health>0&&occupied<0&&
                len(player-ped.p)<75&&playerTalkTime>0:
                ped.socialPartner>=0&&ped.socialPartner<int(peds.size())&&
                peds[ped.socialPartner].alive&&
                peds[ped.socialPartner].socialPartner==int(&ped-peds.data())&&
                len(peds[ped.socialPartner].p-ped.p)<100;
#ifdef MINI_CITY_JOLT
            valid=valid&&(withPlayer?sameFloor(ped,playerY):sameFloor(ped,jolt_world::pedHeight(peds[ped.socialPartner])));
#endif
            if(ped.socialTime<=0||!valid||ped.burnTime>0){
                ped.state=PedState::Wander;ped.socialPartner=-1;
                ped.target=ped.p;ped.socialTime=0;continue;
            }
            Ped* partner=withPlayer?nullptr:&peds[ped.socialPartner];
            Vec2 point=withPlayer?player:partner->p;
            ped.angle=std::atan2(point.z-ped.p.z,point.x-ped.p.x);
            float distance=len(point-ped.p);
            if(ped.state==PedState::Fight&&distance>24){
#ifdef MINI_CITY_JOLT
                Vec2 velocity=ped_navigation::velocityAtHeight(ped,{point.x,withPlayer?playerY:jolt_world::pedHeight(*partner),point.z},ped.speed,dt);
                if(builder::active()||distance<420)jolt_world::movePed(std::size_t(&ped-peds.data()),velocity,dt);
                else moveFar(ped,velocity*dt);
#else
                move(ped.p,norm(point-ped.p)*ped.speed*dt,9,Kind::Car);
#endif
            }else if(ped.state==PedState::Fight&&ped.fireCooldown<=0){
                ped.attackVisualTime=0.42f;ped.fireCooldown=0.95f;
                partner->health-=18;partner->hitFlash=0.3f;
                partner->knockback=norm(partner->p-ped.p)*75;
                audio::playAt(audio::Effect::Hit,partner->p.x,partner->p.z);
                if(partner->health<=0){
                    partner->alive=false;partner->respawn=45;
                    partner->corpseVisualDelay=6;
                    spawnDebris(*partner,{0,105,0});
                }
            }
            ped.fireCooldown=std::max(0.0f,ped.fireCooldown-dt);
            continue;
        }
#ifdef MINI_CITY_JOLT
        if(traffic::updatePed(ped,dt))continue;
#endif
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
#ifdef MINI_CITY_JOLT
        float targetHeight=occupied>=0&&occupied<int(vehicles.size())?vehicles[occupied].rideHeight:playerY;
        float combatDistance=len(Vec3{player.x-ped.p.x,targetHeight-jolt_world::pedHeight(ped),player.z-ped.p.z});
#else
        float targetHeight=playerY,combatDistance=distanceToPlayer;
#endif
        bool canSee=ped.armed&&combatDistance<360&&actorLine(ped,player,targetHeight);
        if(canSee){ped.lastKnown=player;ped.sightMemory=3.5f;
#ifdef MINI_CITY_JOLT
            ped.lastKnownHeight=playerY;
#endif
        }
        if(ped.state==PedState::Attack&&ped.hostile&&!ped.armed){
            if(ped.alertTime<=0){ped.hostile=false;ped.state=PedState::Wander;}
            else{
                ped.target=ped.grievance>0?ped.lastKnown:player;
                ped.angle=std::atan2(ped.target.z-ped.p.z,ped.target.x-ped.p.x);
                if(occupied<0&&distanceToPlayer<23&&sameFloor(ped,playerY)&&actorLine(ped,player,playerY)&&ped.fireCooldown<=0&&health>0){
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
#ifdef MINI_CITY_JOLT
            else if(builder::active()){
                if(len(ped.target-ped.p)<8||!ped_navigation_surface::hasDestination(ped))escape(ped,player);
            }
#endif
            else if(len(ped.target-ped.p)<25||solid(ped.target,12))
                ped.target=ped.p+norm(ped.p-player)*170;
        }
        if(ped.state==PedState::TakeCover
#ifdef MINI_CITY_JOLT
           ||(builder::active()&&ped.state==PedState::Defend)
#endif
           ){
#ifdef MINI_CITY_JOLT
            if(builder::active()&&!ped_navigation_surface::hasDestination(ped))
                ped.state=findCover(ped,ped.lastKnown,ped.target)?PedState::TakeCover:PedState::Attack;
            bool atCover=!builder::active()||sameFloor(ped,ped.navigation.goalHeight,2.5f);
            bool closeThreat=builder::active()?combatDistance<90&&actorLine(ped,player,targetHeight):distanceToPlayer<90;
#else
            bool atCover=true,closeThreat=distanceToPlayer<90;
#endif
            if(ped.state==PedState::TakeCover&&len(ped.target-ped.p)<18&&atCover){
                ped.state=PedState::Defend;
#ifdef MINI_CITY_JOLT
                if(builder::active())ped.tacticTimer=std::max(ped.tacticTimer,2.5f);
#endif
            }
            if(closeThreat)ped.state=PedState::Attack;
        }
        if(ped.state==PedState::Defend&&canSee&&distanceToPlayer<175)ped.state=PedState::Attack;
        if((ped.state==PedState::Attack||ped.state==PedState::Defend)&&
            ped.hostile&&ped.armed&&health>0){
            if(canSee){
                ped.angle=std::atan2(player.z-ped.p.z,player.x-ped.p.x);
                if(ped.state==PedState::Attack&&(ped.tacticTimer<=0
#ifdef MINI_CITY_JOLT
                    ||(builder::active()&&ped.navigation.destinationSelected&&!ped_navigation_surface::hasDestination(ped))
#endif
                    )){
                    Vec2 toward=norm(player-ped.p),side{-toward.z,toward.x};
                    Vec2 destination=distanceToPlayer<105?ped.p-toward*95:
                        distanceToPlayer>235?player-toward*155:
                        ped.p+side*(ped.strafeRight?55.0f:-55.0f);
                    bool budgetDeferred=false;
#ifdef MINI_CITY_JOLT
                    if(builder::active()){
                        Vec3 feet{};
                        bool selected=ped_navigation_surface::destination(ped,destination,feet);
                        if(!selected&&distanceToPlayer>=105&&distanceToPlayer<=235)
                            selected=ped_navigation_surface::destination(ped,ped.p-side*(ped.strafeRight?55.0f:-55.0f),feet);
                        if(selected)ped.target={feet.x,feet.z};
                        else {ped.target=ped.p;ped.navigation={};
                            const auto& stats=ped_navigation_surface::stats();
                            budgetDeferred=stats.plans>=4||stats.physicsQueries>=18000;}
                    }else
#endif
                    if(!solid(destination,12))ped.target=destination;
                    else ped.target=ped.lastKnown;
                    // A busy frame is not a completed tactical choice. Let
                    // later actors retry while successful actors hold theirs.
                    if(!budgetDeferred)ped.strafeRight=!ped.strafeRight;
                    ped.tacticTimer=budgetDeferred?.1f:1.2f+float(ped.style%3)*0.25f;
                }
                if(combatDistance<255&&ped.fireCooldown<=0){
                    const auto& pistol=weapons::stats(ped.weaponIndex);
                    ped.attackVisualTime=0.32f;
                    ped.fireCooldown=++ped.burstShots>=3?
                        std::max(1.35f,pistol.secondsBetweenShots*1.5f):
                        pistol.secondsBetweenShots*1.5f;
                    if(ped.burstShots>=3)ped.burstShots=0;
                    audio::playAt(audio::Effect::Shot,ped.p.x,ped.p.z,ped.weaponIndex);
                    Vec2 direction2=norm(player-ped.p);
                    Vec3 muzzle{ped.p.x+direction2.x*12,game::pedGroundHeight(ped)+18,ped.p.z+direction2.z*12};
                    Vec3 target{player.x,occupied>=0?20.0f:playerY+18,player.z};
#ifdef MINI_CITY_JOLT
                    target.y=targetHeight+(occupied>=0?20.0f:18.0f);
#endif
                    Vec3 direction3=norm(target-muzzle);
                    direction3=norm(Vec3{direction3.x+randf(-ped.accuracy,ped.accuracy),
                        direction3.y+randf(-ped.accuracy*0.55f,ped.accuracy*0.55f),
                        direction3.z+randf(-ped.accuracy,ped.accuracy)});
                    bullets.push_back({muzzle,direction3*pistol.projectileSpeed,
                        pistol.range/pistol.projectileSpeed,pistol.damage,pistol.gravity,
                        0,pistol.range,pistol.falloff,true});
                    announce("An armed pedestrian is firing at you!",2);
                }
            }
#ifdef MINI_CITY_JOLT
            else if(builder::active()&&ped.state==PedState::Defend&&ped_navigation_surface::hasDestination(ped)){
                if(ped.tacticTimer<=0){ped.state=PedState::Investigate;ped.alertTime=3;
                    ped.target=ped.lastKnown;ped.navigation.destinationSelected=false;}
            }
#endif
            else if(ped.sightMemory>0){
                ped.target=ped.lastKnown;
                if(ped.state==PedState::Defend&&ped.tacticTimer<=0){
                    ped.state=PedState::Investigate;ped.alertTime=3;
                }
            }else if(ped.grievance>0){ped.target=ped.lastKnown;}
            else{ped.state=PedState::Wander;ped.hostile=false;}
        }
#ifdef MINI_CITY_JOLT
        if(builder::active()&&ped.state==PedState::Wander&&
           (len(ped.target-ped.p)<7||!ped_navigation_surface::hasDestination(ped)||randi(3000)==0)){
            bool selected=false;
            for(int n=0;n<15&&!selected;++n){Vec2 target=ped.p+Vec2{randf(-170,170),randf(-170,170)};Vec3 feet{};
                if(ped_navigation_surface::destination(ped,target,feet)){ped.target={feet.x,feet.z};selected=true;}}
            if(!selected){ped.target=ped.p;ped.navigation={};}
        }else
#endif
        if(ped.state==PedState::Wander&&(len(ped.target-ped.p)<7||randi(3000)==0)){
            for(int n=0;n<15;++n){Vec2 target=ped.p+Vec2{randf(-170,170),randf(-170,170)};
                if(!solid(target,12)){ped.target=target;break;}}
        }
        if(ped.state==PedState::Wander&&ped.socialCooldown<=0&&
           distanceToPlayer<350&&randi(900)==0){
            for(int j=0;j<int(peds.size());++j){
                auto& other=peds[j];
                if(&other==&ped||other.socialCooldown>0||
                   len(other.p-ped.p)>65)continue;
                if(startSocial(int(&ped-peds.data()),j,randi(14)==0))break;
            }
            if(ped.state!=PedState::Wander)continue;
        }
        float moveSpeed=ped.speed*(ped.panic>0?1.7f:1.0f);
#ifdef MINI_CITY_JOLT
        float destinationHeight=std::numeric_limits<float>::quiet_NaN();
        if((ped.state==PedState::Attack||ped.state==PedState::Defend||ped.state==PedState::Investigate)&&ped.hostile)
            destinationHeight=canSee||(ped.state==PedState::Attack&&!ped.armed&&ped.grievance<=0)?playerY:ped.lastKnownHeight;
        if(builder::active()){
            if(ped_navigation_surface::hasDestination(ped))destinationHeight=ped.navigation.goalHeight;
            else if(len(ped.target-ped.p)<.01f)destinationHeight=jolt_world::pedHeight(ped);
        }
        Vec2 step=ped_navigation::velocityAtHeight(ped,{ped.target.x,destinationHeight,ped.target.z},moveSpeed,dt)*dt+ped.knockback*dt;
#else
        Vec2 step=norm(ped.target-ped.p)*moveSpeed*dt+ped.knockback*dt;
#endif
        ped.knockback=ped.knockback*std::exp(-6.0f*dt);
        Vec2 old=ped.p;
#ifdef MINI_CITY_JOLT
        if(builder::active()||distanceToPlayer<420)
            jolt_world::movePed(std::size_t(&ped-peds.data()),step*(1.0f/dt),dt);
        else moveFar(ped,step);
#else
        move(ped.p,step,9,Kind::Car);
#endif
        if(len(ped.p-old)<0.1f&&ped.state==PedState::Wander)ped.target=ped.p;
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
