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
#include "../src/masonry.h"
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
    cameraYaw=cameraPitch=0;
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
    // Resized machinery shares its mesh scale, rigid pivots, ground gear and
    // debris extents; explosion fragments must not shrink back to source size.
    for(Kind kind:{Kind::Combine,Kind::Airplane}){
        stage();vehicle(kind);jolt_world::reset();ticks(90);
        assert(jolt_world::wheelContactCount(0)>=2);
        std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];
        std::vector<dx11::ModelInstance> instances;dx11::buildScene(groups,instances);
        auto parts=vehicle_systems::parts(vehicles[0]);int rendered=0;
        for(const auto& part:parts){
            const auto* source=dx11::mesh(part.mesh);assert(source);
            float extent=std::max({source->maxX-source->minX,source->maxY-source->minY,source->maxZ-source->minZ});
            assert(std::abs(std::max({part.size.x,part.size.y,part.size.z})-extent*1.5f)<.01f);
            for(const auto& instance:instances)if(instance.source==source){
                assert(instance.scaleX==1.5f&&instance.scaleY==1.5f&&instance.scaleZ==1.5f);
                auto position=vehicle_systems::partPosition(vehicles[0],part);
                assert(len(position-Vec3{instance.x,instance.y,instance.z})<.01f);++rendered;
            }
        }
        assert(rendered==int(parts.size()));
        damageVehicle(0,10000);jolt_world::step(1.0f/60);
        assert(vehicles[0].exploded&&jolt_world::treeFragments().size()==parts.size());
        for(const auto& fragment:jolt_world::treeFragments())assert(fragment.meshScale==1.5f);
    }
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
    // The hull stays still while the turret follows camera azimuth and gun
    // elevation, including a rotated/tilted chassis. Shots start at the tip.
    vehicle_systems::Part barrel,turret;int joints=0;
    for(const auto& part:vehicle_systems::parts(vehicles[0])){
        if(part.spin==5){turret=part;++joints;}if(part.spin==6){barrel=part;++joints;}
    }
    assert(joints==2);
    auto rotate=[](const RagdollPart& q,Vec3 p){Vec3 a{q.qx,q.qy,q.qz};
        auto cross=[](Vec3 x,Vec3 y){return Vec3{x.y*y.z-x.z*y.y,x.z*y.x-x.x*y.z,x.x*y.y-x.y*y.x};};
        Vec3 c=cross(a,p);return p+(c*q.qw+cross(a,c))*2;};
    float hullAngle=vehicles[0].angle;auto hullPose=vehicles[0];
    for(float yaw:{0.0f,PI*.5f,-PI*.75f}){
        cameraYaw=yaw;cameraPitch=.25f;vehicle_systems::update(1.0f/60);
        assert(vehicles[0].angle==hullAngle&&vehicles[0].qy==hullPose.qy);
        Vec3 expected{std::cos(yaw)*std::cos(.25f),std::sin(.25f),std::sin(yaw)*std::cos(.25f)};
        Vec3 actual=rotate(vehicle_systems::partRotation(vehicles[0],barrel),{0,0,1});
        assert(len(actual-expected)<.001f);
        fireCooldown=0;vehicle_systems::fireTank();const auto& shot=bullets.back();
        assert(len(norm(shot.v)-actual)<.001f);
        auto source=dx11::mesh(barrel.mesh);
        Vec3 tip=vehicle_systems::partPosition(vehicles[0],barrel)+actual*(source->maxZ+2);
        assert(len(shot.p-tip)<.01f);
        assert(len(vehicle_systems::partPosition(vehicles[0],turret)-
            vehicle_systems::partPosition(hullPose,turret))<.001f);
    }
    // A rolled chassis still aims in world camera space.
    vehicles[0].qx=vehicles[0].qy=0;vehicles[0].qz=std::sin(.12f);vehicles[0].qw=std::cos(.12f);
    cameraYaw=1.2f;cameraPitch=.1f;vehicle_systems::update(1.0f/60);
    Vec3 expected{std::cos(1.2f)*std::cos(.1f),std::sin(.1f),std::sin(1.2f)*std::cos(.1f)};
    assert(len(rotate(vehicle_systems::partRotation(vehicles[0],barrel),{0,0,1})-expected)<.001f);
    cameraPitch=1.2f;vehicle_systems::update(1.0f/60);assert(vehicles[0].turretPitch<=.6f);
    float parkedYaw=vehicles[0].turretYaw;occupied=-1;cameraYaw+=1;vehicle_systems::update(1.0f/60);
    assert(vehicles[0].turretYaw==parkedYaw);
    // Driving/steering cannot recenter the tank camera or leave the turret a
    // tick behind the authoritative chassis rotation written by Jolt.
    stage();vehicle(Kind::Tank);jolt_world::reset();occupied=0;
    cameraYaw=PI*.5f;cameraPitch=.15f;vehicleLookTime=0;
    keys['W']=keys['D']=true;ticks(180);keys['W']=keys['D']=false;
    assert(cameraYaw==PI*.5f&&std::abs(vehicles[0].angle)>.02f);
    expected={0,std::sin(.15f),std::cos(.15f)};
    assert(len(rotate(vehicle_systems::partRotation(vehicles[0],barrel),{0,0,1})-expected)<.001f);
    // Building hole must agree for visual boxes, swept rays, ground movement,
    // Jolt character movement, reticle and reload reset.
    stage();buildings.push_back({4550,4450,40,100,130,rgb(160,155,140),"hole-test"});jolt_world::reset();
    float entry=0;assert(destruction::segment(buildings[0],{4500,20,4500},{4650,20,4500},entry));
    destruction::blast({4550,20,4500},120);
    assert(buildings[0].damaged&&!buildings[0].pieces.empty());
    assert(!destruction::segment(buildings[0],{4500,20,4500},{4650,20,4500},entry));
    assert(destruction::segment(buildings[0],{4500,120,4500},{4650,120,4500},entry));
    assert(!solid({4570,4500},10)&&clearLine({4500,4500},{4650,4500}));
    jolt_world::step(1.0f/60);assert(!jolt_world::treeFragments().empty());
    std::set<std::string> masonryTypes;
    for(const auto& f:jolt_world::treeFragments()){
        assert(f.category==2);assert(dx11::mesh(f.mesh));masonryTypes.insert(f.mesh);
        assert(dx11::mesh(f.mesh)->vertices.size()>100);
    }
    assert(masonryTypes.size()==masonry::Count);
    auto rubbleStart=jolt_world::treeFragments()[0].p;
    for(int i=0;i<600;++i)jolt_world::step(1.0f/60);
    assert(len(jolt_world::treeFragments()[0].p-rubbleStart)>5);
    int settled=0;for(const auto& f:jolt_world::treeFragments()){
        assert(std::isfinite(f.p.y)&&f.p.y>-2);if(f.p.y<20)++settled;
    }
    assert(settled>=int(jolt_world::treeFragments().size())/2);
    for(int i=0;i<2200;++i)jolt_world::step(1.0f/60);
    assert(jolt_world::treeFragments().empty());
    jolt_world::teleportCharacter({4520,4500},0);
    for(int i=0;i<100;++i){jolt_world::moveCharacter({90,0},false,1.0f/60);jolt_world::step(1.0f/60);}
    assert(player.x>4600);
    // Loading clears fresh session rubble as well as building damage.
    jolt_world::spawnFragment({4510,30,4500},{12,5,6},{0,20,0},rgb(170,70,40),0,masonry::Brick);
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
