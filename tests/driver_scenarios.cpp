#include "../src/game.h"
#include "../src/dx11_assets.h"
#include "../src/jolt_world.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <limits>

void driverScenarios(){
    using namespace game;
    dx11::loadMeshes(L"assets/models/baked");
    peds.clear();rightMouse=false;cameraMode=CameraMode::ThirdNear;
    // Keep the entry/acceleration lane clear of the expanded starter fleet.
    for(int i=0;i<int(vehicles.size());++i){vehicles[i].driver=-1;
        jolt_world::teleportVehicle(i,{9000+float(i)*150,9000},0);}
    std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];
    std::vector<dx11::ModelInstance> instances;
    for(Kind kind:{Kind::Bike,Kind::Car,Kind::SportCar}){
        int index=-1;
        for(int i=0;i<int(vehicles.size());++i)if(vehicles[i].kind==kind){index=i;break;}
        assert(index>=0);
        auto& vehicle=vehicles[index];vehicle.driver=-1;
        jolt_world::teleportVehicle(index,{300,250},0);
        player=previousPlayer=vehicle.p+Vec2{0,30};playerY=0;occupied=-1;
        jolt_world::teleportCharacter(player,0);
        enterExit();
        for(int tick=0;tick<65;++tick)update(1.0f/60);
        assert(occupied==index);
        Vec2 start=vehicle.p;keys['W']=true;
        for(int tick=0;tick<30;++tick)update(1.0f/60);
        keys['W']=false;assert(len(vehicle.p-start)>1);
        for(float angle:{0.0f,1.7f}){
            jolt_world::teleportVehicle(index,vehicle.p,angle);
            dx11::buildScene(groups,instances);
            int authoredParts=0;
            for(const auto& instance:instances)if(instance.material==6&&
                len(Vec2{instance.x,instance.z}-vehicle.p)<80&&instance.source!=dx11::mesh("primitive/box"))++authoredParts;
            assert(authoredParts>=4);
            // This fails if people() hides the player and vehicles() omits it.
            assert(groups[5].size()==dx11::skinMesh("characters/hoodie-man")->vertices.size());
            float top=0;
            for(const auto& vertex:groups[5]){
                assert(std::isfinite(vertex.x)&&std::isfinite(vertex.y)&&std::isfinite(vertex.z));
                assert(len(Vec2{vertex.x,vertex.z}-vehicle.p)<40);
                top=std::max(top,vertex.y-vehicle.rideHeight);
            }
            // The RX-7 cabin is lower than the Rover; the seated head must fit
            // under each imported roof rather than the old procedural body.
            assert(kind==Kind::Bike?top>34&&top<45:kind==Kind::SportCar?top>20&&top<28:top>32&&top<43);
            std::vector<dx11::SkinInstance> skins;
            dx11::buildScene(groups,instances,vehicle.p.x,60,vehicle.p.z-60,&skins);
            assert(skins.empty());
            assert(groups[5].size()==dx11::skinMesh("characters/hoodie-man")->vertices.size());
        }
        vehicle.speed=0;vehicle.velocity={};jolt_world::stopVehicle(index);
        enterExit();assert(occupied<0);
        // First person hides the on-foot player; an abandoned vehicle must not
        // retain a second character after exiting.
        cameraMode=CameraMode::FirstClose;dx11::buildScene(groups,instances);
        assert(groups[5].empty());cameraMode=CameraMode::ThirdNear;
        for(int tick=0;tick<65;++tick)update(1.0f/60);
        jolt_world::teleportVehicle(index,{1000+float(index)*100,1000},0);
    }
    std::puts("Driver scenarios passed: real entry, road acceleration, seated geometry, vehicle rotation, exit cleanup.");
}
