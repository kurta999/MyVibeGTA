#include "../src/game.h"
#include "../src/game_internal.h"
#include "../src/jolt_world.h"
#include "../src/destruction.h"
#include "../src/vehicle_systems.h"
#include "../src/physics.h"
#include "../src/radio.h"
#include "../src/input.h"
#include "../src/ui.h"
#include "../src/savegame.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#include "../src/dx11_assets.h"
#include "../src/camera.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <fstream>
#include <set>
namespace {
using namespace game;
void stage(){
    reset();buildings.clear();trees.clear();peds.clear();props.clear();vehicles.clear();pickups.clear();
    wildlife::animals.clear();birds::flock.clear();
    player=previousPlayer={4500,4500};playerY=0;health=PLAYER_MAX_HEALTH;occupied=-1;
    ui::page=ui::Page::Closed;std::fill(std::begin(keys),std::end(keys),false);leftMouse=false;
}
void vehicle(Kind kind,Vec2 p={4500,4500}){Vehicle v{};v.kind=kind;v.p=p;v.id="expansion-test";v.c=rgb(140,170,80);vehicles.push_back(v);}
void ticks(int n,float dt=1.0f/60){for(int i=0;i<n;++i)update(dt);}
}
void expansionScenarios(){
    using namespace game;
    dx11::loadMeshes(L"assets\\models\\baked");
    for(int kind=0;kind<13;++kind){Vehicle v{};v.kind=Kind(kind);auto parts=vehicle_systems::parts(v);
        assert(parts.size()>=4);for(const auto& part:parts)assert(dx11::mesh(part.mesh));}
    Vehicle combine{};combine.kind=Kind::Combine;bool cuttingAuger=false;
    for(const auto& part:vehicle_systems::parts(combine))cuttingAuger|=part.spin==4;
    assert(cuttingAuger);
    reset();std::set<Kind> kinds;for(auto& v:vehicles){kinds.insert(v.kind);
        if(v.id.rfind("expansion-",0)==0)assert(!solid(v.p,physics::chassisHalf(v.kind).x));}
    assert(kinds.size()==13);assert(radio::load());assert(radio::stations().size()>=10);
    radio::select("off");radio::cycle(1);assert(radio::selectedId()==radio::stations()[0].id);
    radio::cycle(-1);assert(radio::selectedId()=="off");radio::cycle(-1);assert(radio::selectedId()==radio::stations().back().id);
    radio::select("off");
    input::windowProc(nullptr,WM_MOUSEWHEEL,WPARAM(60<<16),0); // fractional detents on foot do not change radio
    stage();vehicle(Kind::Car);occupied=0;
    input::windowProc(nullptr,WM_MOUSEWHEEL,WPARAM(60<<16),0);assert(radio::selectedId()=="off");
    input::windowProc(nullptr,WM_MOUSEWHEEL,WPARAM(60<<16),0);assert(radio::selectedId()==radio::stations()[0].id);
    radio::select("off");
    for(Kind kind:{Kind::Skateboard,Kind::Bicycle,Kind::Tractor,Kind::Combine,Kind::Tank,Kind::Truck}){
        for(float dt:{1.0f/60,1.0f/30}){
            stage();vehicle(kind);jolt_world::reset();occupied=0;keys['W']=true;
            ticks(int(3/dt),dt);keys['W']=false;
            std::printf("%s travel %.1f speed %.1f height %.1f\n",vehicle_systems::name(kind),len(vehicles[0].p-Vec2{4500,4500}),vehicles[0].speed,vehicles[0].rideHeight);
            assert(len(vehicles[0].p-Vec2{4500,4500})>15);assert(std::isfinite(vehicles[0].angle));
            assert(vehicles[0].damage<10);assert(jolt_world::wheelContactCount(0)>=2);
        }
    }
    // A physical hitch preserves relative spacing while accelerating, detaches,
    // and is cleaned up when either connected vehicle explodes.
    stage();vehicle(Kind::Truck);vehicle(Kind::Trailer,{4362,4500});jolt_world::reset();occupied=0;
    ticks(60);assert(jolt_world::toggleTrailer(0));assert(vehicles[0].trailer==1&&vehicles[1].towVehicle==0);
    keys['W']=true;ticks(180);keys['W']=false;
    assert(vehicles[1].p.x>4380);assert(len(vehicles[0].p-vehicles[1].p)<170);
    jolt_world::stopVehicle(0);jolt_world::stopVehicle(1);assert(jolt_world::toggleTrailer(0));
    assert(vehicles[0].trailer<0&&vehicles[1].towVehicle<0);
    // One tank shot, no inventory consumption, bounded repeat rate.
    stage();vehicle(Kind::Tank);jolt_world::reset();occupied=0;cameraPitch=0;fireCooldown=0;
    auto inventory=magazine;vehicle_systems::fireTank();assert(bullets.size()==1&&bullets[0].rocket&&bullets[0].explosionDamage==450);
    vehicle_systems::fireTank();assert(bullets.size()==1&&magazine==inventory);
    // Building hole must agree for visual boxes, swept rays, ground movement,
    // Jolt character movement, reticle and reload reset.
    stage();buildings.push_back({4550,4450,40,100,130,rgb(160,155,140),"hole-test"});jolt_world::reset();
    float entry=0;assert(destruction::segment(buildings[0],{4500,20,4500},{4650,20,4500},entry));
    destruction::blast({4550,20,4500},120);
    assert(buildings[0].damaged&&!buildings[0].pieces.empty());
    assert(!destruction::segment(buildings[0],{4500,20,4500},{4650,20,4500},entry));
    assert(destruction::segment(buildings[0],{4500,120,4500},{4650,120,4500},entry));
    assert(!solid({4570,4500},10)&&clearLine({4500,4500},{4650,4500}));
    jolt_world::teleportCharacter({4520,4500},0);
    for(int i=0;i<100;++i){jolt_world::moveCharacter({90,0},false,1.0f/60);jolt_world::step(1.0f/60);}
    assert(player.x>4600);
    jolt_world::step(1.0f/60);assert(!jolt_world::treeFragments().empty());
    // Session-only holes never appear in the saved document. Display and radio do.
    ui::windowMode=2;ui::windowChoice=0;radio::select(radio::stations()[2].id);
    assert(savegame::save());ui::windowMode=0;ui::windowChoice=2;radio::select("off");
    assert(savegame::load());assert(ui::windowMode==2&&ui::windowChoice==0&&radio::selectedId()==radio::stations()[2].id);
    for(auto& b:buildings)assert(!b.damaged&&b.pieces.empty()&&b.cuts.empty());assert(jolt_world::treeFragments().empty());radio::select("off");ui::windowMode=0;
    for(Kind k:{Kind::Car,Kind::Helicopter,Kind::Airplane}){
        stage();vehicle(k);jolt_world::reset();damageVehicle(0,10000);jolt_world::step(1.0f/60);
        assert(vehicles[0].exploded&&jolt_world::treeFragments().size()>=4);
        for(const auto& fragment:jolt_world::treeFragments())assert(!fragment.mesh.empty());
        auto before=jolt_world::treeFragments()[0].p;ticks(20);
        assert(len(jolt_world::treeFragments()[0].p-before)>5);
        ticks(1200);assert(jolt_world::treeFragments().empty());
    }
    // Fixed-wing takeoff at two tick rates; unpowered flight loses energy.
    for(float dt:{1.0f/60,1.0f/30}){
        stage();vehicle(Kind::Airplane);jolt_world::reset();occupied=0;keys['W']=true;
        ticks(int(2/dt),dt);keys[VK_SPACE]=true;ticks(int(1/dt),dt);keys[VK_SPACE]=false;
        ticks(int(2/dt),dt);keys['W']=false;
        std::printf("PLANE dt %.4f x %.1f alt %.1f speed %.1f pitch %.2f stall %d damage %.1f\n",dt,vehicles[0].p.x,vehicles[0].rideHeight,vehicles[0].speed,vehicles[0].flightPitch,vehicles[0].stalled,vehicles[0].damage);
        assert(vehicles[0].rideHeight>30);assert(vehicles[0].p.x>4750);assert(!vehicles[0].exploded);
        keys['D']=true;ticks(int(.5f/dt),dt);keys['D']=false;
        assert(std::abs(vehicles[0].flightRoll)>.08f);
        float alt=vehicles[0].rideHeight;keys['S']=true;ticks(int(2/dt),dt);keys['S']=false;
        assert(vehicles[0].flightThrottle<.01f);assert(std::isfinite(vehicles[0].rideHeight));
        occupied=-1;ticks(int(8/dt),dt);assert(vehicles[0].rideHeight<alt+100);
    }
    // With no forward airflow, an airborne wing stalls and loses altitude.
    stage();vehicle(Kind::Airplane);jolt_world::reset();occupied=0;
    jolt_world::teleportVehicle(0,{4500,4500},0,180);ticks(30);
    assert(vehicles[0].stalled&&vehicles[0].rideHeight<170&&!vehicles[0].exploded);
    // A low, unpowered landing settles on its landing gear without an explosion.
    stage();vehicle(Kind::Airplane);jolt_world::reset();occupied=0;
    jolt_world::teleportVehicle(0,{4500,4500},0,5);ticks(180);
    assert(jolt_world::wheelContactCount(0)>=2&&vehicles[0].rideHeight<8&&!vehicles[0].exploded);
    radio::shutdown();jolt_world::shutdown();std::puts("Vehicle expansion, trailer, tank, building holes, fragments, radio and saved display checks passed");
}
