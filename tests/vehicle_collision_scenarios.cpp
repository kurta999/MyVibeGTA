#include "../src/game.h"
#include "../src/jolt_world.h"
#include "../src/regions.h"
#include "../src/traffic.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

void vehicleCollisionScenarios(){
    using namespace game;
    buildings.clear();peds.clear();props.clear();
    constexpr float dt=1.0f/60;
    for(Kind kind:{Kind::Car,Kind::SportCar,Kind::Bike}){
        Vehicle car{};car.kind=kind;car.id="collision-test";car.p={12000,2000};car.angle=PI/2;
        vehicles={car};player=car.p;occupied=0;health=PLAYER_MAX_HEALTH;
        jolt_world::reset();float maximumSpeed=0;
        for(int tick=0;tick<1800;++tick){
            int phase=tick;
            float throttle=phase<1300?1.0f:phase<1550?-1.0f:0;
            float steering=phase<1000?0:phase<1150?1.0f:phase<1300?-1.0f:0;
            keys[VK_SPACE]=phase>=1150&&phase<1300;
            vehicles[0].collisionCooldown=std::max(0.0f,vehicles[0].collisionCooldown-dt);
            jolt_world::driveVehicle(0,throttle,steering,dt,phase>=1550);
            jolt_world::step(dt);
            maximumSpeed=std::max(maximumSpeed,std::abs(vehicles[0].speed));
        }
        keys[VK_SPACE]=false;
        std::printf("Empty-road vehicle %d: peak speed %.1f, damage %.3f\n",int(kind),maximumSpeed,vehicles[0].damage);
        std::fflush(stdout);
        assert(maximumSpeed>100&&vehicles[0].damage==0&&health==PLAYER_MAX_HEALTH);
        // A world-position resync stops the chassis without an impact. This
        // used to be mistaken for a crash by the velocity-difference heuristic.
        jolt_world::teleportVehicle(0,{12000,2000},PI/2);
        for(int tick=0;tick<300;++tick){
            jolt_world::driveVehicle(0,1,0,dt);jolt_world::step(dt);
        }
        assert(std::abs(vehicles[0].speed)>100);
        vehicles[0].p.x+=10;
        jolt_world::driveVehicle(0,0,0,dt);jolt_world::step(dt);
        assert(vehicles[0].damage==0&&health==PLAYER_MAX_HEALTH);
    }
    // Real wall impacts must still stop and damage a moving car.
    Vehicle car{};car.kind=Kind::Car;car.id="wall-test";car.p={5000,6000};
    vehicles={car};player=car.p;occupied=0;
    buildings.push_back({5320,5900,30,200,100,{1,1,1},"test-wall"});
    jolt_world::reset();
    for(int tick=0;tick<300;++tick){
        vehicles[0].collisionCooldown=std::max(0.0f,vehicles[0].collisionCooldown-dt);
        jolt_world::driveVehicle(0,1,0,dt);jolt_world::step(dt);
    }
    assert(vehicles[0].damage>0&&vehicles[0].p.x<5320);
    // Exit with throttle still held: momentum survives, controls do not latch,
    // and the empty chassis comes to rest without an artificial collision.
    buildings.clear();trees.clear();
    for(Kind kind:{Kind::Car,Kind::SportCar,Kind::Bike}){
        car={};car.kind=kind;car.p={12000,2000};car.id="coast-test";
        vehicles={car};player=car.p;occupied=0;health=PLAYER_MAX_HEALTH;
        jolt_world::reset();
        for(int tick=0;tick<300;++tick){jolt_world::driveVehicle(0,1,0,dt);jolt_world::step(dt);}
        float speed=len(vehicles[0].velocity);Vec2 start=vehicles[0].p;
        assert(speed>100);keys['W']=true;
        enterExit();assert(occupied<0&&len(vehicles[0].velocity)>speed*0.99f);
        traffic::update(dt);jolt_world::step(dt);assert(len(vehicles[0].velocity)>speed*0.8f);
        for(int tick=0;tick<1200;++tick){traffic::update(dt);jolt_world::step(dt);}
        keys['W']=false;
        std::printf("Coasting vehicle %d: %.1f -> %.2f, travel %.1f\n",int(kind),speed,len(vehicles[0].velocity),len(vehicles[0].p-start));
        std::fflush(stdout);
        assert(len(vehicles[0].velocity)<2&&len(vehicles[0].p-start)>20);
        assert(vehicles[0].damage==0);
    }
    for(Kind kind:{Kind::Car,Kind::SportCar,Kind::Bike,Kind::Boat}){
        car={};car.kind=kind;car.p={850,SHORE+200};vehicles={car};
        player=car.p;occupied=0;playerY=0;swimming=false;
        jolt_world::reset();enterExit();
        assert(occupied<0&&swimming&&regions::waterAt(player));
        Vec2 start=player;
        for(int tick=0;tick<30;++tick)jolt_world::moveCharacter({30,0},false,dt);
        assert(swimming&&len(player-start)>5&&std::abs(playerY)<3);
    }
    // A wall beside a submerged car still blocks that door.
    car={};car.p={850,SHORE+200};vehicles={car};occupied=0;player=car.p;
    buildings.push_back({800,car.p.z+20,100,40,100,{1,1,1},"exit-wall"});
    jolt_world::reset();enterExit();
    assert(occupied<0&&swimming&&player.z<car.p.z);
    std::puts("Vehicle collision scenarios passed: empty-road braking/turning/drifting and real wall damage.");
}
