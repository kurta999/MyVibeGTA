#include "../src/game.h"
#include "../src/jolt_world.h"
#include "../src/camera.h"
#include "../src/weapons.h"
#include "../src/ai.h"
#include "../src/dx11_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>

void pedScenarios(){
    using namespace game;
    constexpr float dt=1.0f/60;
    buildings.clear();vehicles.clear();props.clear();trees.clear();peds.clear();bullets.clear();
    player=previousPlayer={4500,4500};playerY=0;health=PLAYER_MAX_HEALTH;
    occupied=-1;cameraYaw=0;cameraPitch=0;cameraMode=CameraMode::ThirdNear;
    rightMouse=true;leftMouse=false;weapon=weapons::indexOf("pistol");
    magazine[weapon]=12;reloadRemaining=0;recoil=0;fireCooldown=0;
    Ped target{};target.id="ped-scenario-target";target.p={4600,4500};
    target.target=target.p;target.speed=0;target.health=100;peds.push_back(target);
    jolt_world::reset();
    for(int shot=0;shot<4&&peds[0].alive;++shot){
        cameraPitch=0;recoil=0;fireCooldown=0;
        shoot();
        for(int tick=0;tick<10;++tick)update(dt);
    }
    assert(!peds[0].alive&&peds[0].health<=0);
    // Walking into a pedestrian stops the capsule and plays a contact pose.
    bullets.clear();peds.clear();
    Ped walker{};walker.id="walker";walker.p={4550,4500};
    walker.target=walker.p;walker.speed=0;peds.push_back(walker);
    player=previousPlayer={4500,4500};playerY=0;cameraYaw=0;
    rightMouse=false;keys['W']=true;jolt_world::reset();
    for(int tick=0;tick<45;++tick)update(dt);
    keys['W']=false;
    assert(len(player-peds[0].p)>=17.8f&&len(player-peds[0].p)<21);
    assert(peds[0].contactVisualTime>0&&playerContactVisualTime>0);
    assert(peds[0].alive&&peds[0].health==100);
    // Pedestrians also stop when they walk into a stationary player.
    player=previousPlayer={4500,4500};playerY=0;
    peds[0].p={4540,4500};jolt_world::teleportCharacter(player,0);
    jolt_world::reset();
    for(int tick=0;tick<40;++tick)jolt_world::movePed(0,{-120,0},dt);
    assert(len(peds[0].p-player)>=17.8f&&peds[0].contactVisualTime>0);
    // F starts a face-to-face talk without provoking or damaging either side.
    player={4500,4500};peds[0].p={4530,4500};peds[0].contactVisualTime=0;
    jolt_world::teleportCharacter(player,0);
    assert(interactionPrompt().find("TALK")!=std::string::npos);
    interact();assert(peds[0].state==PedState::Talk&&playerTalkTime>0);
    ai::update(dt);assert(peds[0].health==100&&!peds[0].hostile);
    dx11::loadMeshes(L"assets/models/baked");
    // A saved corpse without a ragdoll snapshot must keep its settled pose.
    // Previously the death clip replayed continuously after loading a save.
    auto savedPeds=peds;float savedHealth=health,savedWorldTime=worldTime;
    Ped corpse{};corpse.id="saved-corpse";corpse.p={4530,4500};
    corpse.style=3;corpse.alive=false;corpse.corpseVisualDelay=0;
    corpse.pinAnchor={4540,4500};
    health=0;
    std::vector<dx11::Vertex> corpseFirst[dx11::MATERIAL_GROUPS];
    std::vector<dx11::Vertex> corpseLater[dx11::MATERIAL_GROUPS];
    std::vector<dx11::ModelInstance> corpseInstances;
    for(int state=0;state<3;++state){
        corpse.pinned=state==1;corpse.carried=state==2;
        peds={corpse};worldTime=0;
        dx11::buildScene(corpseFirst,corpseInstances);
        worldTime=0.2f;
        dx11::buildScene(corpseLater,corpseInstances);
        assert(!corpseFirst[5].empty()&&corpseFirst[5].size()==corpseLater[5].size());
        for(std::size_t i=0;i<corpseFirst[5].size();++i){
            const auto& a=corpseFirst[5][i];const auto& b=corpseLater[5][i];
            assert(std::abs(a.x-b.x)<0.01f&&std::abs(a.y-b.y)<0.01f&&
                   std::abs(a.z-b.z)<0.01f);
        }
    }
    peds=std::move(savedPeds);health=savedHealth;worldTime=savedWorldTime;
    std::vector<dx11::Vertex> posed[dx11::MATERIAL_GROUPS];
    std::vector<dx11::Vertex> idle[dx11::MATERIAL_GROUPS];
    std::vector<dx11::ModelInstance> instances;
    dx11::buildScene(posed,instances);
    playerTalkTime=0;peds[0].state=PedState::Wander;peds[0].socialTime=0;
    dx11::buildScene(idle,instances);
    assert(!posed[5].empty()&&posed[5].size()==idle[5].size());
    bool changed=false;
    for(std::size_t i=0;i<posed[5].size();++i)
        if(std::abs(posed[5][i].x-idle[5][i].x)>0.02f||
           std::abs(posed[5][i].y-idle[5][i].y)>0.02f||
           std::abs(posed[5][i].z-idle[5][i].z)>0.02f){changed=true;break;}
    assert(changed);
    // Ordinary scene poses collected for GPU deformation must reproduce the
    // established CPU geometry, including blended clip palettes and yaw/scale.
    playerContactVisualTime=0;peds[0].contactVisualTime=0;
    std::vector<dx11::SkinInstance> skins;
    dx11::buildScene(idle,instances,4500,50,4450);
    dx11::buildScene(posed,instances,4500,50,4450,&skins);
    assert(skins.size()==2&&posed[5].empty());
    std::vector<dx11::Vertex> reconstructed;
    for(const auto& skin:skins)dx11::deformSkinCpu(skin,reconstructed);
    assert(reconstructed.size()==idle[5].size());
    for(std::size_t i=0;i<reconstructed.size();++i){
        float actual[12],expected[12];
        std::memcpy(actual,&reconstructed[i],sizeof(actual));
        std::memcpy(expected,&idle[5][i],sizeof(expected));
        for(int field=0;field<12;++field)assert(std::abs(actual[field]-expected[field])<0.0001f);
    }
    rightMouse=true;cameraPitch=0.4f;
    dx11::buildScene(posed,instances,4500,50,4450,&skins);
    assert(skins.size()==1&&!posed[5].empty()); // Pitched player aim retains procedural CPU pose.
    rightMouse=false;cameraPitch=0;
    // Two civilians can talk or fight; fighting applies damage and death.
    peds.clear();Ped first{};first.id="first";first.p={4520,4500};
    first.target=first.p;first.speed=70;
    Ped second=first;second.id="second";second.p={4544,4500};
    second.target=second.p;peds={first,second};playerTalkTime=0;
    jolt_world::reset();
    assert(ai::startSocial(0,1,false));
    ai::update(dt);
    assert(peds[0].state==PedState::Talk&&peds[1].state==PedState::Talk);
    for(int tick=0;tick<260;++tick)ai::update(dt);
    assert(peds[0].state==PedState::Wander&&peds[1].state==PedState::Wander);
    assert(ai::startSocial(0,1,true));
    for(int tick=0;tick<450&&peds[0].alive&&peds[1].alive;++tick)ai::update(dt);
    assert(!peds[0].alive||!peds[1].alive);
    std::puts("Ped scenarios passed: camera-fired kill, contact, talking, and fighting.");
}
