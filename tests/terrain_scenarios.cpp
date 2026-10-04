#include "../src/game.h"
#include "../src/game_internal.h"
#include "../src/terrain.h"
#include "../src/regions.h"
#include "../src/jolt_world.h"
#include "../src/physics.h"
#include "../src/savegame.h"
#include "../src/dx11_assets.h"
#include "../src/dx11_terrain.h"
#include "../src/camera.h"
#include "../src/wildlife.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>

void terrainScenarios(){
    using namespace game;
    reset();buildings.clear();trees.clear();peds.clear();props.clear();vehicles.clear();wildlife::animals.clear();
    assert(terrain::landforms().size()==20);
    assert(terrain::height({13900,1800})>500);
    assert(terrain::height({15500,15000})>450);
    assert(terrain::height({13800,4550})<-100);
    assert(terrain::height({13900,15450})<-100);
    // Every regional road and settlement remains level, including junctions.
    for(const auto& road:regions::roads())for(int i=0;i<=100;++i){
        Vec2 p=road.start+(road.end-road.start)*(i/100.0f);
        assert(std::abs(terrain::height(p))<.001f);
    }
    for(const auto& hub:regions::hubs())assert(terrain::height(hub.p)==0);
    assert(terrain::height({300,235})==0&&terrain::height({12000,8500})==0);
    // Ground aiming, including a horizontal shot that meets a mountain.
    float hit=0;
    assert(terrain::segmentHit({13900,900,1800},{13900,-200,1800},hit));
    assert(std::abs((900-1100*hit)-terrain::height({13900,1800}))<.1f);
    assert(terrain::segmentHit({12700,100,1800},{13900,100,1800},hit)&&hit<1);
    Vehicle car{};car.kind=Kind::Car;car.id="terrain-car";car.p={10000,8500};car.angle=0;
    vehicles.push_back(car);jolt_world::reset();
    for(Vec2 p:{Vec2{13900,1800},{13800,12550},{13800,4550},{13900,15450}}){
        player=previousPlayer=p;playerY=terrain::height(p)+4;occupied=-1;
        jolt_world::teleportCharacter(p,playerY);
        for(int tick=0;tick<120;++tick){jolt_world::moveCharacter({},false,1.0f/60);jolt_world::step(1.0f/60);}
        std::printf("terrain capsule %.0f %.0f: %.3f vs %.3f\n",p.x,p.z,playerY,terrain::height(player));
        assert(grounded&&std::abs(playerY-terrain::height(player))<1);
        // The camera reticle must meet the same elevated or depressed surface.
        camera::Pose pose{{p.x,playerY+100,p.z},{p.x,playerY-100,p.z}};
        auto aim=camera::traceReticle(pose,200);
        assert(std::abs(aim.y-terrain::height(p))<.1f);
    }
    // Walk down and back up the side of a dry basin without a sea-level clamp.
    player=previousPlayer={13550,15450};playerY=terrain::height(player);
    jolt_world::teleportCharacter(player,playerY);float start=playerY;
    for(int tick=0;tick<240;++tick){jolt_world::moveCharacter({50,0},false,1.0f/60);jolt_world::step(1.0f/60);}
    assert(player.x>13740&&playerY<start-10&&std::abs(playerY-terrain::height(player))<2);
    for(int tick=0;tick<240;++tick){jolt_world::moveCharacter({-50,0},false,1.0f/60);jolt_world::step(1.0f/60);}
    assert(playerY>start-5&&std::abs(playerY-terrain::height(player))<2);
    // Wheel raycasts and the rendered chassis follow terrain below and above 0.
    for(Vec2 p:{Vec2{13900,15450},{13900,1800}}){
        jolt_world::teleportVehicle(0,p,0);
        for(int tick=0;tick<180;++tick){jolt_world::driveVehicle(0,0,0,1.0f/60,true);jolt_world::step(1.0f/60);}
        assert(jolt_world::wheelContactCount(0)>=2);
        std::printf("terrain car %.0f %.0f: y %.3f vs %.3f, contacts %d\n",vehicles[0].p.x,vehicles[0].p.z,vehicles[0].rideHeight,terrain::height(vehicles[0].p),jolt_world::wheelContactCount(0));
        assert(std::abs(vehicles[0].rideHeight-terrain::height(vehicles[0].p))<12);
    }
    // Legacy ground saves and current saves resolve onto the actual surface.
    player=previousPlayer={13900,15450};playerY=terrain::height(player);
    assert(savegame::save());assert(savegame::load());
    assert(std::abs(playerY-terrain::height(player))<.01f&&playerY<-100);
    dx11::loadMeshes(L"assets/models/baked");
    std::vector<dx11::ModelInstance> instances;dx11::appendRegionalTerrain(instances,1800);
    bool below=false,above=false,rocky=false;
    for(const auto& instance:instances){
        const auto* source=instance.source;
        if(source->revision==terrain::revision())for(const auto& v:source->vertices){
            assert(std::abs(v.y-terrain::height({v.x,v.z}))<.12f);
            below|=v.y<-100;above|=v.y>300;
        }
        for(const auto& range:source->materialRanges)rocky|=!range.normalFile.empty();
    }
    assert(below&&above&&rocky);
    std::puts("terrain scenarios passed");
}
