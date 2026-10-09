#include "builder_test_support.h"
#include "../src/game_internal.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#include "../src/jolt_world.h"
#include "../src/terrain.h"
#include "../src/savegame.h"
#include "../src/weapons.h"
#include "../src/dx11_assets.h"
#include "../src/debug_menu.h"
#include "../src/camera.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <chrono>

namespace {
using namespace game;
constexpr float dt=1.0f/60;
void toggle(){bool next=!builder::active();assert(builder::requestToggle()&&finishBuilderTransition()&&builder::active()==next);}
void fixture(){
    reset();assert(builder::loadCatalog());buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();bullets.clear();
    wildlife::clearCarry();wildlife::animals.clear();birds::flock.clear();player=previousPlayer={400,300};playerY=0;
    occupied=enteringVehicle=-1;health=PLAYER_MAX_HEALTH;debug_menu::flyMode=false;swimming=false;grounded=true;
    jolt_world::reset();toggle();
}
void room(bool roof=true){for(int x=2;x<=12;++x)for(int z=2;z<=6;++z){assert(mineTerrainVolume({x,-2,z}));if(!roof)assert(mineTerrainVolume({x,-1,z}));}}
wildlife::Animal animal(int kind,Vec2 p,float y){wildlife::Animal a;a.id="builder-wildlife-"+std::to_string(kind);
    a.species=kind;a.p=a.home=a.target=p;a.health=wildlife::species()[kind].health;a.timer=100;
    a.elevation=y;a.elevationAt=p;a.elevationKnown=a.elevationMode=true;return a;}
void tick(int count){for(int n=0;n<count;++n){wildlife::update(dt);jolt_world::step(dt);}}
void playerAt(Vec2 p,float y){player=previousPlayer=p;playerY=y;jolt_world::teleportCharacter(player,y);}
void rendered(const wildlife::Animal& a){
    std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];std::vector<dx11::ModelInstance> instances;dx11::buildScene(groups,instances);
    std::vector<const dx11::Mesh*> family;std::string name=std::string("animals/")+wildlife::species()[a.species].id;
    family.push_back(dx11::mesh(name));family.push_back(dx11::mesh(name+"-dead"));
    for(int frame=0;frame<8;++frame)family.push_back(dx11::mesh(name+"-walk-"+std::to_string(frame)));
    bool found=false;for(const auto& instance:instances)if(std::find(family.begin(),family.end(),instance.source)!=family.end()&&
            std::abs(instance.x-a.p.x)<.1f&&std::abs(instance.z-a.p.z)<.1f){
        assert(std::abs(instance.y-wildlife::originHeight(a))<.1f);found=true;}
    assert(found);
}
}
void builderWildlifeScenarios(){
    using namespace game;std::setvbuf(stdout,nullptr,_IONBF,0);
    dx11::loadMeshes(L"assets/models/baked");
    fixture();wildlife::reset();auto promptBegin=std::chrono::steady_clock::now();
    for(int frame=0;frame<200;++frame)assert(wildlife::nearbyCorpse(true)<0&&wildlife::nearbyCorpse(false)<0&&wildlife::nearbyMount()<0);
    float promptMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-promptBegin).count();
    std::printf("HUD wildlife probes: 600 loot/carry/mount calls, %zu generated living animals, %.3f ms total\n",wildlife::animals.size(),promptMs);
    // Generous gross-regression bound: the former all-world sight scans took
    // seconds per HUD frame. Actual DX11 benchmarks retain the FPS evidence.
    assert(promptMs<1000);
    fixture();room();wildlife::animals={animal(0,{220,140},0)};playerAt({195,140},-80);cameraYaw=0;
    assert(wildlife::meleeTarget(50)<0&&wildlife::nearbyMount()<0&&!wildlife::mount(0));
    int original=wildlife::animals[0].health;assert(!wildlife::hit({220,-65,140},10,player,true));assert(wildlife::animals[0].health==original);
    wildlife::hurt(0,1,player,true);float hp=health;tick(90);assert(health==hp&&wildlife::animals[0].state!=wildlife::State::Attack);
    std::puts("wildlife: retained roof prevents surface melee, mounting, projectile hits and predator damage against underground player");
    wildlife::animals={animal(0,{220,140},-80)};tick(2);playerAt({195,140},-80);
    assert(wildlife::meleeTarget(50)==0&&wildlife::hit({220,-65,140},1,player,true));
    tick(2);assert(health<hp);wildlife::animals[0].state=wildlife::State::Idle;health=PLAYER_MAX_HEALTH;
    assert(!wildlife::mount(0)); // Tiger fits, but its standing rider does not.
    for(int x=2;x<=12;++x)for(int z=2;z<=6;++z)assert(mineTerrainVolume({x,-1,z}));
    assert(wildlife::mount(0));cameraYaw=0;keys['W']=true;Vec2 start=player;
    for(int n=0;n<120;++n){wildlife::updateRider(dt);wildlife::update(dt);jolt_world::step(dt);
        assert(std::abs(wildlife::originHeight(wildlife::animals[0])+80)<2&&playerY<-50);}
    keys['W']=false;assert(len(player-start)>100);rendered(wildlife::animals[0]);assert(wildlife::dismount());assert(std::abs(playerY+80)<2);
    std::puts("wildlife: underground attacks, tiger mount movement, shared model height and supported dismount passed");

    fixture();room();for(int x=2;x<=12;++x)for(int z=2;z<=6;++z)assert(mineTerrainVolume({x,-3,z}));
    wildlife::animals={animal(1,{240,160},-120)};playerAt({180,160},-120);
    assert(!wildlife::mount(0)); // Elephant fits, but elephant plus rider exceeds the 80-unit room.
    assert(!wildlife::riding());
    std::puts("wildlife: mounted headroom rejects elephant in the low underground room");

    fixture();room(false);wildlife::animals={animal(4,{220,140},0)};
    tick(90);auto& falling=wildlife::animals[0];assert(std::abs(wildlife::originHeight(falling)+80)<2&&falling.supported);
    rendered(falling);assert(jolt_world::activeAnimalColliderCount()==1);
    auto saved=builder::capture();int life=falling.health;falling.looted=true;
    toggle();assert(!builder::active()&&wildlife::originHeight(falling)>-2&&falling.health==life&&falling.looted);
    rendered(falling);toggle();assert(builder::capture()==saved&&wildlife::originHeight(falling)>-2);
    tick(3);assert(wildlife::originHeight(falling)>-20&&falling.verticalVelocity<0);tick(90);assert(std::abs(wildlife::originHeight(falling)+80)<2);
    std::puts("wildlife: actual gravity pit fall, kinematic contact body, immediate normal F5 resurface and builder re-entry fall passed");

    fixture();wildlife::animals={animal(4,{220,140},40)};
    assert(placeEditVolume({5,0,3},builder::itemIndex("granite"),false));tick(30);
    assert(std::abs(wildlife::originHeight(wildlife::animals[0])-40)<2);rendered(wildlife::animals[0]);
    assert(mineEditVolume({5,0,3}));tick(60);assert(std::abs(wildlife::originHeight(wildlife::animals[0]))<2);
    std::puts("wildlife: placed block support and removal use the same physics and rendered height");

    fixture();room();wildlife::animals={animal(4,{140,140},-80)};
    auto& walker=wildlife::animals[0];walker.state=wildlife::State::Wander;walker.target={440,140};walker.timer=100;
    assert(placeEditVolume({6,-2,3},builder::itemIndex("granite"),false));tick(1200);
    std::printf("wildlife: underground walker detours a placed block, final %.1f %.1f %.1f\n",walker.p.x,wildlife::originHeight(walker),walker.p.z);
    assert(walker.p.x>320&&std::abs(wildlife::originHeight(walker)+80)<2);

    fixture();room();wildlife::animals={animal(0,{220,140},0),animal(14,{230,140},-80)};
    wildlife::animals[0].timer=0;tick(1);assert(wildlife::animals[0].state!=wildlife::State::Attack&&wildlife::animals[1].health>0);
    auto& corpse=wildlife::animals[1];corpse.health=0;corpse.state=wildlife::State::Dead;
    playerAt({210,140},0);assert(wildlife::nearbyCorpse(false)<0&&!wildlife::carryDrop());
    playerAt({210,140},-80);assert(wildlife::nearbyCorpse(false)==1&&wildlife::carryDrop());tick(1);assert(corpse.carried);
    assert(wildlife::carryDrop()&&!corpse.carried);assert(std::abs(wildlife::originHeight(corpse)+80)<2);tick(30);rendered(corpse);
    std::puts("wildlife: separated prey and corpses cannot interact through a roof; underground carry/drop retains floor");

    fixture();room(false);
    for(int kind=0;kind<int(wildlife::species().size());++kind){
        wildlife::animals={animal(kind,{260,180},0)};tick(90);
        assert(std::abs(wildlife::originHeight(wildlife::animals[0])+80)<2&&wildlife::animals[0].supported);
        assert(jolt_world::animalClear(wildlife::animals[0],{260,wildlife::originHeight(wildlife::animals[0]),180},0));
        rendered(wildlife::animals[0]);
    }
    std::puts("wildlife: all 15 species settle and render on an actual excavated floor with species-sized clearance");

    fixture();room();wildlife::animals={animal(14,{260,140},-80)};playerAt({210,140},-80);
    Bullet bullet{};bullet.p={220,-78,140};bullet.v={12000,0,0};bullet.life=1;bullet.damage=50;bullet.range=500;bullets.push_back(bullet);
    update(dt);assert(wildlife::animals[0].health==0&&bullets.empty());
    std::puts("wildlife: actual swept game projectile hits small underground animal at its collision/render height");

    fixture();room();wildlife::animals={animal(4,{260,140},-80)};
    jolt_world::preparePedNavigation();assert(!canPlaceEditVolume({6,-2,3},builder::itemIndex("granite")));
    assert(canPlaceEditVolume({6,0,3},builder::itemIndex("granite"))); // Above the retained roof.
    camera::Pose aim{{210,-70,140},{310,-70,140}};
    auto hit=camera::traceReticle(aim,100);assert(hit.x>240&&hit.x<260&&std::abs(hit.y+70)<1);
    assert(builder::trace(aim.eye,{1,0,0}).source==builder::Source::None);
    std::puts("wildlife: oriented animal footprint blocks overlapping placement on its floor and reticle/edit targeting stops on the actual underground body");

    fixture();room();const int pistol=weapons::indexOf("pistol");pickups={{{220,140},pistol,true,0,"builder-height-pickup"}};
    playerAt({220,140},-80);update(dt);assert(pickups[0].available&&pickupOriginHeight(pickups[0])==0);
    playerAt({220,140},0);update(dt);assert(!pickups[0].available);
    pickups[0].available=true;playerAt({400,300},0);assert(placeEditVolume({5,0,3},builder::itemIndex("granite"),false));
    assert(pickupOriginHeight(pickups[0])==40);playerAt({220,140},-80);update(dt);assert(pickups[0].available);
    playerAt({220,140},40);update(dt);assert(!pickups[0].available);
    toggle();assert(pickupOriginHeight(pickups[0])==0);
    std::puts("pickups: actual game loop rejects roof-through and block-through collection, collects on matching surface, restores normal marker altitude");
    reset();savegame::save();jolt_world::shutdown();std::puts("Builder wildlife scenarios passed");
}
