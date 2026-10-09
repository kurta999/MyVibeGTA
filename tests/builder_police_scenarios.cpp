#include "builder_test_support.h"
#include "../src/game_internal.h"
#include "../src/jolt_world.h"
#include "../src/police.h"
#include "../src/ai.h"
#include "../src/birds.h"
#include "../src/wildlife.h"
#include "../src/ped_navigation.h"
#include "../src/ped_navigation_surface.h"
#include "../src/savegame.h"
#include "../src/weapons.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
namespace {
using namespace game;
constexpr float dt=1.0f/60;
void toggle(){bool next=!builder::active();assert(builder::requestToggle()&&finishBuilderTransition()&&builder::active()==next);}
void fixture(bool wide=false){reset();assert(builder::loadCatalog());buildings.clear();trees.clear();peds.clear();vehicles.clear();props.clear();pickups.clear();
    wildlife::animals.clear();birds::flock.clear();bullets.clear();police::reset();player=previousPlayer={400,700};playerY=0;health=PLAYER_MAX_HEALTH;
    jolt_world::reset();toggle();for(int x=2;x<=(wide?32:12);++x)for(int z=wide?4:2;z<=(wide?12:6);++z)assert(mineTerrainVolume({x,-2,z}));
    weapon=weapons::indexOf("knife");assert(weapon>=0);unlocked[weapon]=true;fireCooldown=0;
}
void playerAt(Vec2 p,float y){player=previousPlayer=p;playerY=y;jolt_world::teleportCharacter(p,y);}
void actor(Vec2 p,float y,float angle=0){Ped ped{};ped.id="builder-police-"+std::to_string(peds.size());ped.p=ped.target=p;ped.angle=angle;ped.speed=57;
    peds.push_back(ped);jolt_world::addPed();jolt_world::teleportPed(peds.size()-1,p,y);}
void policeTicks(int count){for(int n=0;n<count;++n){ped_navigation::beginFrame();police::update(dt);}}
void budget(){auto s=ped_navigation_surface::stats();assert(s.plans<=4&&s.physicsQueries<=18000&&s.expanded<=12000);}
}
void builderPoliceScenarios(bool before){using namespace game;std::setvbuf(stdout,nullptr,_IONBF,0);
    fixture();actor({220,140},0);playerAt({195,140},-80);int target=stealthTarget();stealthKill();
    std::printf("Stealth roof reproduction: target %d, surface victim alive %d\n",target,int(peds[0].alive));
    if(before)assert(target==0&&!peds[0].alive);else assert(target<0&&peds[0].alive);
    fixture();actor({220,140},-80);actor({220,200},0,-PI/2);playerAt({195,140},-80);stealthKill();policeTicks(100);
    std::printf("Witness roof reproduction: underground victim alive %d, wanted %d\n",int(peds[0].alive),police::wantedLevel());
    if(before)assert(!peds[0].alive&&police::wantedLevel()>0);else assert(!peds[0].alive&&police::wantedLevel()==0);
    fixture(true);buildings.push_back({80,160,1240,440,120,{1,1,1},"dispatch-surface-building"});jolt_world::refreshScenery();
    playerAt({300,300},-80);cameraYaw=PI;police::setWantedLevel(1);ped_navigation::beginFrame();police::update(dt);
    auto dispatchStats=ped_navigation_surface::stats();std::printf("Dispatch plans %u queries %u expanded %u actors %zu\n",dispatchStats.plans,dispatchStats.physicsQueries,dispatchStats.expanded,peds.size());
    assert(peds.size()==1&&peds[0].police);std::printf("Dispatch roof reproduction: spawn %.1f %.1f %.1f, report floor %.1f\n",
        peds[0].p.x,jolt_world::pedHeight(0),peds[0].p.z,peds[0].lastKnownHeight);
    if(before)assert(jolt_world::pedHeight(0)>-2);else assert(std::abs(jolt_world::pedHeight(0)+80)<2);
    if(!before){
        Vec2 start=peds[0].p;bool fired=false;
        for(int n=0;n<180;++n){ped_navigation::beginFrame();police::update(dt);ai::update(dt,false);jolt_world::step(dt);budget();
            fired|=!bullets.empty();for(const auto& bullet:bullets)assert(bullet.p.y<-40);bullets.clear();}
        assert(len(peds[0].p-start)>30&&fired&&std::abs(jolt_world::pedHeight(0)+80)<2);
        toggle();assert(!builder::active()&&jolt_world::pedHeight(0)>-2);toggle();
        playerAt({300,300},-80);jolt_world::teleportPed(0,peds[0].p,-80);policeTicks(5);assert(std::abs(jolt_world::pedHeight(0)+80)<2);
        std::puts("dispatch: actual underground pursuit/fire, shared frame caps and F5 restoration/re-entry");

        fixture();actor({220,140},-80);actor({220,220},-80,-PI/2);playerAt({195,140},-80);
        assert(stealthTarget()==0);stealthKill();policeTicks(100);assert(!peds[0].alive&&police::wantedLevel()==2);
        fixture();actor({220,140},-80);actor({220,220},-80,PI/2);playerAt({195,140},-80);
        stealthKill();policeTicks(100);assert(!peds[0].alive&&police::wantedLevel()==0);
        fixture();actor({220,140},-80);actor({220,220},-80,-PI/2);playerAt({195,140},-80);
        assert(placeEditVolume({5,-2,4},builder::itemIndex("granite"),false));stealthKill();policeTicks(100);
        assert(!peds[0].alive&&police::wantedLevel()==0);
        std::puts("stealth: same-floor facing witness reports; rear-facing and block-occluded witnesses remain silent");

        fixture();actor({220,140},-80);playerAt({195,140},-80);
        Building wall{200,120,5,40,40,{1,1,1},"knife-wall"};wall.damaged=true;wall.pieces={{{200,-80,120},{205,-40,160}}};
        buildings.push_back(wall);jolt_world::refreshScenery();assert(stealthTarget()<0);stealthKill();assert(peds[0].alive);
        buildings.clear();jolt_world::refreshScenery();playerAt({170,140},-80);assert(stealthTarget()<0);
        playerAt({245,140},-80);assert(stealthTarget()<0);playerAt({195,140},-60);assert(stealthTarget()<0);
        fixture();for(int x=4;x<=5;++x)assert(placeEditVolume({x,0,3},builder::itemIndex("granite"),false));
        actor({220,140},40);playerAt({195,140},0);assert(stealthTarget()<0);playerAt({195,140},40);assert(stealthTarget()==0);
        stealthKill();assert(!peds[0].alive);std::puts("stealth: wall, range, behind cone and vertical checks; takedown allowed on placed elevated support");

        fixture();actor({220,140},-80);actor({180,140},-80);peds[1].alive=false;peds[1].health=0;peds[1].respawn=45;peds[1].corpseVisualDelay=6;
        assert(jolt_world::spawnRagdoll(peds[1],{}));for(int n=0;n<100;++n)jolt_world::step(dt);playerAt({195,140},-80);
        assert(stealthTarget()==0);carryDrop();assert(carryingBody()&&peds[1].carried&&stealthTarget()<0);stealthKill();assert(peds[0].alive);clearCarry();
        std::puts("stealth: actual corpse carrying occupies the hands and prevents a knife takedown");

        fixture(true);playerAt({300,300},-80);ped_navigation::beginFrame();Vec3 feet{};
        assert(ped_navigation_surface::supported({500,300},-80,feet)&&std::abs(feet.y+80)<2);
        for(float x:{439.9f,440.0f,440.1f}){auto floors=jolt_world::pedestrianFloors({x,300});bool found=false;
            for(float y:floors)found|=std::abs(y+80)<2;assert(found);}
        Prop crate{};crate.p={500,300};crate.y=-80;props.push_back(crate);jolt_world::reset();playerAt({300,300},-80);
        assert(jolt_world::pedestrianClear({500,-79.95f,300})&&!jolt_world::pedestrianClear({500,-79.95f,300},true));
        assert(!ped_navigation_surface::supported({500,300},-80,feet,2.5f));jolt_world::remove(0);props.clear();jolt_world::refreshScenery();
        Building ceiling{480,280,40,40,40,{1,1,1},"dispatch-low-ceiling"};ceiling.damaged=true;ceiling.pieces={{{480,-60,280},{520,-40,320}}};
        buildings.push_back(ceiling);jolt_world::refreshScenery();assert(!ped_navigation_surface::supported({500,300},-80,feet,2.5f));
        buildings.clear();jolt_world::refreshScenery();ped_navigation::beginFrame();
        assert(ped_navigation_surface::supported({500,300},-80,feet)&&std::abs(feet.y+80)<2);budget();
        std::puts("dispatch support: actual floor, dynamic crate and insufficient capsule headroom");

        // Reachability must not accept the planner's partial approach to a
        // disconnected destination, nor support an edge over a deeper void.
        fixture(true);playerAt({300,300},-80);
        Building divider{480,160,40,360,40,{1,1,1},"dispatch-divider"};divider.damaged=true;divider.pieces={{{480,-80,160},{520,-40,520}}};
        buildings.push_back(divider);jolt_world::refreshScenery();ped_navigation::beginFrame();
        assert(!ped_navigation_surface::reachable({300,-80,300},{640,-80,300}));budget();
        buildings.clear();jolt_world::refreshScenery();ped_navigation::beginFrame();assert(ped_navigation_surface::reachable({300,-80,300},{640,-80,300}));
        assert(mineTerrainVolume({12,-3,7}));assert(!ped_navigation_surface::supported({479,300},-80,feet,2.5f));budget();
        std::puts("dispatch: full endpoint required across sealed divider; removing divider enables route; edited floor edge rejects support");

        fixture();playerAt({300,180},-80);cameraYaw=PI;police::setWantedLevel(1);ped_navigation::beginFrame();police::update(dt);budget();
        assert(peds.empty());for(int x=13;x<=25;++x)for(int z=3;z<=5;++z)assert(mineTerrainVolume({x,-2,z}));
        policeTicks(130);assert(peds.size()==1&&peds[0].police&&std::abs(jolt_world::pedHeight(0)+80)<2);
        std::puts("dispatch: sealed small room defers; newly excavated reachable corridor enables cooldown retry");

        fixture(true);playerAt({300,300},-80);cameraYaw=PI;ped_navigation::beginFrame();
        for(int n=0;n<4;++n)assert(ped_navigation_surface::reachable({300,-80,300},{350+float(n)*20,-80,300}));
        assert(ped_navigation_surface::stats().plans==4);police::setWantedLevel(1);police::update(dt);assert(peds.empty());
        ai::update(dt,false);assert(ped_navigation_surface::stats().plans==4);budget();policeTicks(130);assert(peds.size()==1);
        std::puts("dispatch: exhausted shared route budget defers and retries instead of resetting the AI frame");

        fixture(true);playerAt({300,300},-80);cameraYaw=PI;
        for(int n=0;n<8;++n){actor({1100+float(n)*10,440},-80);peds.back().alive=false;peds.back().health=0;peds.back().police=true;peds.back().carried=true;}
        std::string held=peds[0].id;peds[1].carried=false;police::setWantedLevel(1);ped_navigation::beginFrame();police::update(dt);
        assert(peds.size()==8&&peds[0].id==held&&!peds[0].alive&&peds[0].carried&&peds[1].alive&&peds[1].police);
        assert(std::abs(jolt_world::pedHeight(1)+80)<2);std::puts("dispatch: capped officer pool reuses an uncarried corpse without overwriting carried ownership");

        fixture(true);playerAt({300,300},-80);std::vector<Vec2> starts;
        for(int n=0;n<12;++n){actor({480+float(n%4)*40,220+float(n/4)*45},-80);
            auto& ped=peds.back();ped.armed=ped.hostile=true;ped.state=PedState::Attack;ped.alertTime=100;ped.socialCooldown=999;
            ped.weaponIndex=weapons::indexOf("pistol");ped.style=0;starts.push_back(ped.p);}
        unsigned peak=0;for(int n=0;n<180;++n){ai::update(dt);jolt_world::step(dt);budget();bullets.clear();
            peak=std::max(peak,ped_navigation_surface::stats().physicsQueries);}
        int progressed=0;for(int n=0;n<12;++n){float distance=len(peds[n].p-starts[n]);std::printf("Combat crowd %d distance %.1f\n",n,distance);progressed+=distance>5;}
        std::printf("Combat crowd: %d/12 progressed, peak queries %u\n",progressed,peak);assert(progressed==12);
    }
    reset();assert(savegame::save());jolt_world::shutdown();
}
