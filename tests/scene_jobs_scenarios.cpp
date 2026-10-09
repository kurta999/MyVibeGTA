#include "../src/game.h"
#include "../src/dx11_assets.h"
#include "../src/cpu_jobs.h"
#include "../src/ui.h"
#include "../src/jolt_world.h"
#include "../src/weather.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace {
struct Frame {
    std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];
    std::vector<dx11::ModelInstance> models;
    std::vector<dx11::SkinInstance> skins;
};
void capture(Frame& frame,cpu::Pool* pool){
    dx11::buildScene(frame.groups,frame.models,game::player.x,70,game::player.z-100,&frame.skins,false,pool);
}
void compare(const Frame& serial,const Frame& parallel){
    for(int i=0;i<dx11::MATERIAL_GROUPS;++i){
        assert(serial.groups[i].size()==parallel.groups[i].size());
        if(!serial.groups[i].empty())assert(std::memcmp(serial.groups[i].data(),parallel.groups[i].data(),
            serial.groups[i].size()*sizeof(dx11::Vertex))==0);
    }
    assert(serial.models.size()==parallel.models.size()&&serial.skins.size()==parallel.skins.size());
    for(size_t i=0;i<serial.models.size();++i){
        const auto& a=serial.models[i];const auto& b=parallel.models[i];
        assert(a.source==b.source&&a.material==b.material);
        assert(a.x==b.x&&a.y==b.y&&a.z==b.z&&a.scaleX==b.scaleX&&a.scaleY==b.scaleY&&a.scaleZ==b.scaleZ&&
            a.cosYaw==b.cosYaw&&a.sinYaw==b.sinYaw&&a.centerX==b.centerX&&a.minY==b.minY&&a.centerZ==b.centerZ&&
            a.r==b.r&&a.g==b.g&&a.b==b.b&&a.sinPitch==b.sinPitch&&a.cosPitch==b.cosPitch);
    }
    for(size_t i=0;i<serial.skins.size();++i){
        const auto& a=serial.skins[i];const auto& b=parallel.skins[i];
        assert(a.source==b.source&&a.identity==b.identity&&a.palette==b.palette&&a.scale==b.scale&&
            a.origin==b.origin&&a.transform==b.transform&&a.yaw==b.yaw);
    }
}
void matchingFrame(cpu::Pool& pool){
    Frame serial,parallel,scratch;
    // Force both paths to rebuild the same grass cache, including ordered row
    // merging; a warm parallel cache alone would not exercise those jobs.
    int distance=ui::grassDistance;
    ui::grassDistance=distance+1;capture(scratch,nullptr);ui::grassDistance=distance;
    capture(serial,nullptr);
    ui::grassDistance=distance+1;capture(scratch,nullptr);ui::grassDistance=distance;
    capture(parallel,&pool);
    assert(dx11::sceneWorkStats().jobBatches>0);
    if(game::rightMouse)assert(dx11::sceneWorkStats().skinVertices>4096);
    compare(serial,parallel);
    // The opt-in DX12 path must remove the fallback vertices without changing
    // character geometry. Compare against the original scene deformation.
    Frame extended,reference;
    dx11::buildScene(reference.groups,reference.models,game::player.x,70,game::player.z-100,nullptr,false,&pool);
    dx11::buildScene(extended.groups,extended.models,game::player.x,70,game::player.z-100,&extended.skins,false,&pool,true);
    assert(dx11::sceneWorkStats().skinVertices==0);
    assert(extended.groups[5].empty());
    for(const auto& skin:extended.skins)dx11::deformSkinCpu(skin,extended.groups[5]);
    auto& expected=reference.groups[5];auto& actual=extended.groups[5];
    assert(expected.size()==actual.size());
    for(size_t i=0;i<actual.size();++i){
        float a[12],b[12];std::memcpy(a,&actual[i],sizeof(a));std::memcpy(b,&expected[i],sizeof(b));
        for(int field=0;field<12;++field)assert(std::abs(a[field]-b[field])<(field<3?.003f:.00003f));
    }
}
}
void sceneJobScenarios(){
    using namespace game;
    dx11::loadMeshes(L"assets/models/baked");
    assert(dx11::skinMesh("characters/hoodie-man"));
    cpu::Pool pool(4);
    ui::vegetationDensity=2;ui::grassDistance=65;ui::drawDistance=60;
    ui::lodDistance=75;weather::set("clear");
    player=previousPlayer={180,100};worldTime=1.25f;gameHour=12;
    // Exercise all four character skins in car, sport-car, bike, and boat seats.
    assert(peds.size()>=4&&vehicles.size()>=4);
    for(int i=0;i<4;++i){
        peds[i].alive=true;peds[i].style=i;
        auto& v=vehicles[i];v.p={200+float(i)*45,125};v.driver=i;
        v.kind=Kind(i);v.angle=0.3f*i;v.exploded=false;
    }
    rightMouse=true;cameraPitch=0.55f; // CPU pitched aiming plus ordinary GPU poses.
    matchingFrame(pool);
    auto& corpse=peds[4];corpse.p={200,170};corpse.alive=false;
    jolt_world::spawnRagdoll(corpse,{30,10,0});jolt_world::step(1.0f/60);
    assert(!ragdollParts.empty());rightMouse=false;cameraPitch=0;gameHour=22;
    matchingFrame(pool);
    // A changed frame/world must join old work and never reuse old input/output.
    player=previousPlayer={4400,9000};gameHour=12;worldTime=9.5f;
    matchingFrame(pool);
    player=previousPlayer={8125,9210};worldTime=20;
    matchingFrame(pool);
    // Repeated warm frames still deform actors through independent slots.
    player=previousPlayer={180,100};matchingFrame(pool);
    std::puts("Parallel scene checks passed: exact vertices, ordered grass/models, palettes, seats, aiming, ragdolls, region changes");
}
