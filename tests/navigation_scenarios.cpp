#include "../src/game.h"
#include "../src/game_internal.h"
#include "../src/ped_navigation.h"
#include "../src/ai.h"
#include "../src/jolt_world.h"
#include "../src/fire.h"
#include "../src/police.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

namespace {
using namespace game;
constexpr float dt=1.0f/60;
void fixture(){
    jolt_world::shutdown();buildings.clear();vehicles.clear();peds.clear();props.clear();
    bullets.clear();fire::reset();police::reset();
    player={660,430};playerY=0;health=PLAYER_MAX_HEALTH;occupied=enteringVehicle=-1;
}
void addPed(Vec2 point){Ped ped{};ped.p=ped.target=point;ped.speed=60;
    ped.id="nav-"+std::to_string(peds.size());peds.push_back(ped);}
void moveTo(Vec2 goal){
    ped_navigation::beginFrame();
    for(std::size_t i=0;i<peds.size();++i){
        Vec2 before=peds[i].p;
        Vec2 v=ped_navigation::velocity(peds[i],goal,75,dt);
        jolt_world::movePed(i,v,dt);
        assert(len(peds[i].p-before)<3);assert(!solid(peds[i].p,8));
    }
    jolt_world::step(dt);
}
}
void navigationScenarios(){
    using namespace game;std::setvbuf(stdout,nullptr,_IONBF,0);
    // Actual hostile AI must retain its target and route around the wall.
    fixture();buildings.push_back({500,300,80,260,80,{1,1,1},"wall"});
    addPed({450,430});jolt_world::reset();ai::reactToHit(peds[0],player);
    bool detour=false;
    for(int n=0;n<600&&len(peds[0].p-player)>23;++n){
        Vec2 old=peds[0].p;ai::update(dt);jolt_world::step(dt);
        assert(!solid(peds[0].p,8)&&len(peds[0].p-old)<3);
        detour|=peds[0].p.z<290||peds[0].p.z>570;
    }
    std::printf("wall pursuit: %.1f remaining\n",len(peds[0].p-player));
    assert(detour&&len(peds[0].p-player)<25);

    // A U-shaped enclosure forces an initial move away from the destination.
    fixture();player={650,500};
    buildings={{500,300,25,300,80,{1,1,1},"left"},
        {800,300,25,300,80,{1,1,1},"right"},
        {500,575,325,25,80,{1,1,1},"bottom"}};
    addPed({650,500});jolt_world::reset();bool leftEnclosure=false;
    for(int n=0;n<1500&&len(peds[0].p-Vec2{650,700})>12;++n){
        moveTo({650,700});leftEnclosure|=peds[0].p.z<290;}
    assert(leftEnclosure&&len(peds[0].p-Vec2{650,700})<15);

    // Four pursuers share a detour without overlapping each other or the wall.
    fixture();player={700,430};buildings={{500,350,80,200,80,{1,1,1},"crowd-wall"}};
    for(int i=0;i<4;++i)addPed({430.0f-i*25,375.0f+i*28});
    jolt_world::reset();float closest=1000;
    for(int n=0;n<1500;++n){
        moveTo(player);
        for(int a=0;a<4;++a)for(int b=a+1;b<4;++b)
            closest=std::min(closest,len(peds[a].p-peds[b].p));
    }
    std::printf("crowd minimum separation %.1f; remaining",closest);
    for(const auto& ped:peds){std::printf(" %.1f",len(ped.p-player));assert(len(ped.p-player)<60);}
    std::puts("");assert(closest>=16);

    // A new prop on a previously clear path must invalidate the direct route.
    fixture();player={760,430};addPed({420,430});jolt_world::reset();
    for(int n=0;n<50;++n)moveTo(player);
    props.push_back({{550,430},{},0,0,0,0,80,false,true});
    bool wentAround=false;
    for(int n=0;n<550&&len(peds[0].p-player)>12;++n){
        moveTo(player);assert(len(peds[0].p-props[0].p)>24);
        wentAround|=std::abs(peds[0].p.z-430)>25;
    }
    assert(wentAround&&len(peds[0].p-player)>=19&&len(peds[0].p-player)<25);

    // Opposing walkers choose passing space rather than a permanent head-on jam.
    fixture();player={800,600};addPed({420,430});addPed({700,430});jolt_world::reset();
    float passingGap=1000;
    for(int n=0;n<600;++n){
        ped_navigation::beginFrame();
        for(int i=0;i<2;++i)jolt_world::movePed(i,
            ped_navigation::velocity(peds[i],i==0?Vec2{700,430}:Vec2{420,430},70,dt),dt);
        jolt_world::step(dt);passingGap=std::min(passingGap,len(peds[0].p-peds[1].p));
    }
    assert(passingGap>=16&&peds[0].p.x>670&&peds[1].p.x<450);

    // Parked cars use their oriented footprint; walkers must go around them.
    fixture();player={760,430};addPed({420,430});
    Vehicle car{};car.kind=Kind::Car;car.p={550,430};car.angle=0.65f;
    vehicles.push_back(car);jolt_world::reset();
    for(int n=0;n<550;++n){moveTo(player);assert(len(peds[0].p-car.p)>24);}
    assert(len(peds[0].p-player)<25);

    // Enclosed destinations and water never trigger straight-line tunneling.
    fixture();buildings={{500,350,100,160,80,{1,1,1},"sealed"}};
    addPed({420,430});jolt_world::reset();
    for(int n=0;n<400;++n)moveTo({550,430});
    assert(!solid(peds[0].p,8)&&peds[0].p.x<490);
    fixture();player={900,1800};addPed({900,1800});jolt_world::reset();
    for(int n=0;n<300;++n)moveTo({900,2000});
    assert(peds[0].p.z<1850);
    jolt_world::shutdown();std::puts("navigation scenarios passed");
}
