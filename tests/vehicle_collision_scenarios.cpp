#include "../src/game.h"
#include "../src/jolt_world.h"
#include "../src/regions.h"
#include "../src/traffic.h"
#include "../src/wildlife.h"
#include "../src/dx11_assets.h"
#include "../src/physics.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

void vehicleCollisionScenarios(){
    using namespace game;
    assert(physics::vehicleScale(Kind::Car)==2&&physics::vehicleScale(Kind::SportCar)==2);
    assert(physics::vehicleScale(Kind::Helicopter)==2&&physics::vehicleScale(Kind::Bike)==1);
    assert(physics::vehicleScale(Kind::Boat)==1&&physics::vehicleRestHeight(Kind::Car)==28);
    // Hit volumes rotate with the doubled body and do not block clear lanes
    // beside it as an oversized circular approximation would.
    Vehicle hitCar{};hitCar.kind=Kind::Car;float entry=0;
    assert(!physics::vehicleSegmentHit(hitCar,{-100,20,30},{100,20,30},entry));
    assert(physics::vehicleSegmentHit(hitCar,{-100,20,24},{100,20,24},entry));
    assert(std::abs(entry-.26f)<.001f&&physics::vehicleContains(hitCar,{40,20,24}));
    hitCar.angle=PI/2;
    assert(physics::vehicleSegmentHit(hitCar,{-100,20,0},{100,20,0},entry));
    assert(std::abs(entry-.37f)<.001f);
    buildings.clear();peds.clear();props.clear();trees.clear();wildlife::animals.clear();
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
    // The doubled 48-unit front chassis extent must stop before the wall.
    assert(vehicles[0].damage>0&&vehicles[0].p.x<5320-43);
    // Trunks stop walkers and low-speed cars. High closing speed breaks the
    // tree once and replaces the trunk with falling physical fragments.
    buildings.clear();occupied=-1;vehicles.clear();
    Tree tree{};tree.p={4500,4500};tree.id="collision-tree";
    trees={tree};player={4400,4500};playerY=groundHeight(player);jolt_world::reset();
    assert(jolt_world::activeTreeColliderCount()==1);
    for(int tick=0;tick<180;++tick){
        jolt_world::moveCharacter({80,0},false,dt);
        // Slopes can steer the capsule around a trunk; its footprint must never penetrate it.
        assert(len(player-tree.p)>=15.8f);
    }
    player={4400,4500};playerY=groundHeight(player);jolt_world::teleportCharacter(player,playerY);
    trees[0].destroyed=true;jolt_world::step(dt);
    assert(jolt_world::activeTreeColliderCount()==0);
    for(int tick=0;tick<90;++tick)jolt_world::moveCharacter({80,0},false,dt);
    assert(player.x>4510);
    for(float distance:{80.0f,450.0f}){
        trees={tree};car={};car.kind=Kind::Car;car.id="tree-strike";
        car.p=tree.p-Vec2{distance,0};vehicles={car};player=car.p;occupied=0;
        health=PLAYER_MAX_HEALTH;jolt_world::reset();float peak=0;
        for(int tick=0;tick<360&&!trees[0].destroyed;++tick){
            jolt_world::driveVehicle(0,1,0,dt);jolt_world::step(dt);
            peak=std::max(peak,len(vehicles[0].velocity));
        }
        std::printf("Tree strike distance %.0f: peak %.1f, destroyed %d, fragments %zu\n",
            distance,peak,trees[0].destroyed,jolt_world::treeFragments().size());
        assert(vehicles[0].damage>0);
        if(distance<100){assert(!trees[0].destroyed&&vehicles[0].p.x<4480);continue;}
        assert(trees[0].destroyed&&trees[0].health==0&&!trees[0].burning);
        assert(jolt_world::activeTreeColliderCount()==0&&jolt_world::treeFragments().size()==11);
        Vec3 initial=jolt_world::treeFragments()[3].p;
        for(int tick=0;tick<90;++tick)jolt_world::step(dt);
        assert(len(jolt_world::treeFragments()[3].p-initial)>20);
        dx11::loadMeshes(L"assets/models/baked");
        std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];
        std::vector<dx11::ModelInstance> instances;dx11::buildScene(groups,instances);
        std::size_t wood=0,leaves=0;
        for(const auto& vertex:groups[0]){
            if(std::abs(vertex.r-132/255.0f)<0.00001f&&std::abs(vertex.g-91/255.0f)<0.00001f)++wood;
            if(std::abs(vertex.r-70/255.0f)<0.00001f&&std::abs(vertex.g-116/255.0f)<0.00001f)++leaves;
        }
        auto vertexCount=[](const dx11::Mesh* mesh){assert(mesh);return mesh->indices.empty()?mesh->vertices.size():mesh->indices.size();};
        assert(wood>=8*vertexCount(dx11::mesh("primitive/cylinder")));
        assert(leaves>=3*vertexCount(dx11::mesh("primitive/sphere")));
        for(const auto& vertex:groups[0])assert(std::isfinite(vertex.x)&&std::isfinite(vertex.y)&&std::isfinite(vertex.z));
        for(int tick=0;tick<720;++tick)jolt_world::step(dt);
        assert(jolt_world::treeFragments().empty());
    }
    trees.clear();
    // Contact damage applies to both occupied and empty moving vehicles; the
    // animal collider disappears when killed, without repeated corpse damage.
    for(bool driven:{true,false}){
        car={};car.kind=Kind::Car;car.p={4100,4500};vehicles={car};
        wildlife::Animal a{};a.species=0;a.id="car-hit-tiger";
        a.p=a.home=a.target={4500,4500};a.health=180;a.timer=100;
        wildlife::animals={a};player=car.p;occupied=driven?0:-1;playerY=groundHeight(player);
        health=PLAYER_MAX_HEALTH;jolt_world::reset();
        for(int tick=0;tick<360&&wildlife::animals[0].health>0;++tick){
            jolt_world::driveVehicle(0,1,0,dt);jolt_world::step(dt);
        }
        assert(wildlife::animals[0].health==0);
        assert(jolt_world::activeAnimalColliderCount()==0&&vehicles[0].damage>0);
    }
    wildlife::animals.clear();
    // Exit with throttle still held: momentum survives, controls do not latch,
    // and the empty chassis comes to rest without an artificial collision.
    buildings.clear();trees.clear();
    for(Kind kind:{Kind::Car,Kind::SportCar,Kind::Bike}){
        car={};car.kind=kind;car.p={10000,3000};car.id="coast-test";
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
        player=car.p;occupied=0;playerY=groundHeight(player);swimming=false;
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
