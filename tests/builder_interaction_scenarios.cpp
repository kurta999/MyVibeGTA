#include "builder_test_support.h"
#include "../src/game_internal.h"
#include "../src/jolt_world.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#include "../src/ai.h"
#include "../src/weapons.h"
#include "../src/commerce.h"
#include "../src/traversal.h"
#include "../src/savegame.h"
#include "../src/ped_navigation_surface.h"
#include "../src/dx11_assets.h"
#include "../src/debug_menu.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
namespace {
using namespace game;
constexpr float dt=1.0f/60;
void toggle(){bool next=!builder::active();assert(builder::requestToggle()&&finishBuilderTransition()&&builder::active()==next);}
void fixture(){
    reset();assert(builder::loadCatalog());buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();bullets.clear();
    wildlife::animals.clear();birds::flock.clear();commerce::shops.clear();commerce::houses.clear();traversal::ladders.clear();traversal::trees.clear();missions.clear();
    player=previousPlayer={400,300};playerY=0;occupied=enteringVehicle=-1;health=PLAYER_MAX_HEALTH;jolt_world::reset();toggle();
    for(int x=2;x<=12;++x)for(int z=2;z<=6;++z)assert(builder::mineTerrain({x,-2,z}));
    grounded=true;swimming=false;debug_menu::flyMode=false;
}
void playerAt(Vec2 p,float y){player=previousPlayer=p;playerY=y;jolt_world::teleportCharacter(p,y);}
void actor(Vec2 p,float y,bool dead=false){Ped ped;ped.id="builder-interaction-"+std::to_string(peds.size());ped.p=ped.target=p;ped.speed=65;ped.cash=73;
    peds.push_back(ped);jolt_world::addPed();jolt_world::teleportPed(peds.size()-1,p,y);
    if(dead){auto& body=peds.back();body.alive=false;body.health=0;body.respawn=45;body.corpseVisualDelay=6;jolt_world::spawnRagdoll(body,{});}
}
void ticks(int count){for(int n=0;n<count;++n)jolt_world::step(dt);}
Vec3 pose(float floor){Vec3 low{},high{},contact{};assert(jolt_world::corpsePose(peds[0],low,high,contact));
    assert(low.y>floor-2&&high.y<floor+42);return contact;}
void punch(){cameraYaw=0;keys[VK_SPACE]=true;rightMouse=false;fireCooldown=0;shoot();keys[VK_SPACE]=false;}
void nearBody(float y){playerAt(peds[0].p+Vec2{-20,0},y);}
void checkBudget(){auto stats=ped_navigation_surface::stats();assert(stats.plans<=4&&stats.physicsQueries<=18000&&stats.expanded<=12000);}
void heldModel(bool expected){const auto& stack=builder::inventory()[builder::selected()];assert(stack.item>=0);
    auto source=dx11::mesh("builder/"+builder::items()[stack.item].id);assert(source);
    for(auto mode:{CameraMode::FirstWide,CameraMode::ThirdNear}){cameraMode=mode;
        std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];std::vector<dx11::ModelInstance> instances;dx11::buildScene(groups,instances);
        bool found=false;for(const auto& instance:instances)found|=instance.source==source;assert(found==expected);}
}
}
void builderInteractionScenarios(){
    using namespace game;std::setvbuf(stdout,nullptr,_IONBF,0);
    dx11::loadMeshes(L"assets/models/baked");
    fixture();actor({220,140},0,true);ticks(90);playerAt(peds[0].p+Vec2{-12,0},-80);
    int beforeMoney=money;assert(interactionPrompt().find("LOOT")==std::string::npos&&carryPrompt().empty());interact();carryDrop();
    assert(!peds[0].looted&&!peds[0].carried&&money==beforeMoney);pose(0);
    ticks(360);assert(corpseSnapshots.size()==1);nearBody(-80);interact();carryDrop();
    assert(!peds[0].looted&&!peds[0].carried&&money==beforeMoney);
    nearBody(0);assert(interactionPrompt().find("LOOT")!=std::string::npos);interact();assert(money==beforeMoney+73&&peds[0].looted);
    interact();assert(money==beforeMoney+73);carryDrop();assert(peds[0].carried&&corpseSnapshots.empty()&&ragdollParts.empty());
    clearCarry();std::puts("interactions: live/captured surface corpse cannot be looted or carried through retained roof; same-floor loot is single-use");

    fixture();actor({220,140},-80,true);ticks(120);nearBody(-80);pose(-80);beforeMoney=money;interact();
    assert(money==beforeMoney+73&&peds[0].looted&&peds[0].cash==0);float respawn=peds[0].respawn;carryDrop();
    assert(peds[0].carried&&carryingBody()&&ragdollParts.empty());heldModel(false);auto beforeCuts=builder::capture();
    leftMouse=true;builder::setUseHeld(true);builder::update(5);leftMouse=false;
    assert(builder::miningProgress()==0&&builder::target().source==builder::Source::None&&builder::capture()==beforeCuts);
    playerAt({200,160},-80);cameraYaw=0;carryDrop();heldModel(true);
    assert(!peds[0].carried&&!carryingBody()&&ragdollParts.size()==6);ticks(180);auto contact=pose(-80);
    assert(contact.y<-60&&peds[0].looted&&peds[0].cash==0&&peds[0].respawn==respawn&&peds[0].health==0);
    nearBody(-80);assert(carryPrompt().find("CARRY")!=std::string::npos);carryDrop();assert(peds[0].carried);clearCarry();
    std::puts("interactions: underground corpse loot, physical drop/settling and pickup preserve cash, health, loot and timer state");

    // Death capsule height remains at the old surface; current physical pose
    // must follow the body falling through a newly removed support.
    fixture();actor({220,140},0,true);
    for(int x=2;x<=12;++x)for(int z=2;z<=6;++z)assert(builder::mineTerrain({x,-1,z}));
    ticks(180);contact=pose(-80);assert(contact.y<-60);nearBody(-80);assert(interactionPrompt().find("LOOT")!=std::string::npos);
    carryDrop();assert(peds[0].carried);auto edits=builder::capture();toggle();assert(peds[0].carried&&playerY>-2);toggle();
    assert(peds[0].carried&&builder::capture()==edits);playerAt({200,160},-80);cameraYaw=0;carryDrop();ticks(180);pose(-80);
    ticks(240);assert(corpseSnapshots.size()==1);nearBody(-80);carryDrop();assert(peds[0].carried&&corpseSnapshots.empty());
    playerAt({200,160},-80);carryDrop();ticks(180);pose(-80);
    std::puts("interactions: falling/captured corpses use current body height; carried F5 transitions and snapshot pickup/drop restore real physics");

    fixture();actor({220,140},-80,true);ticks(120);nearBody(-80);carryDrop();assert(peds[0].carried);
    playerAt({220,140},-80);Building enclosure{160,80,120,120,40,{1,1,1},"corpse-drop-enclosure"};enclosure.damaged=true;
    enclosure.pieces={{{160,-80,80},{185,-40,200}},{{255,-80,80},{280,-40,200}},{{185,-80,80},{255,-40,105}},{{185,-80,175},{255,-40,200}}};
    buildings.push_back(enclosure);jolt_world::refreshScenery();cameraYaw=0;carryDrop();
    assert(peds[0].carried&&carryingBody()&&ragdollParts.empty());buildings.clear();jolt_world::refreshScenery();carryDrop();
    assert(!peds[0].carried&&ragdollParts.size()==6);ticks(180);pose(-80);
    std::puts("interactions: blocked release retains carried state; opening space permits retry without losing corpse");

    fixture();actor({220,140},-80,true);ticks(120);nearBody(-80);carryDrop();assert(peds[0].carried);playerAt({400,300},0);
    for(int x=4;x<=8;++x)for(int z=2;z<=5;++z)assert(builder::place({x,0,z},builder::itemIndex("granite"),false));
    playerAt({220,140},40);cameraYaw=0;carryDrop();ticks(180);contact=pose(40);assert(contact.y>40&&contact.y<60);
    nearBody(40);assert(carryPrompt().find("CARRY")!=std::string::npos);carryDrop();assert(peds[0].carried);clearCarry();
    std::puts("interactions: corpse release and pickup respect actual placed-block support rather than original terrain");

    fixture();actor({220,140},0);playerAt({195,140},-80);peds[0].angle=0;
    int beforeHealth=peds[0].health;beforeMoney=money;assert(interactionPrompt().empty());interact();punch();
    assert(peds[0].health==beforeHealth&&peds[0].cash==73&&money==beforeMoney&&!peds[0].hostile);
    jolt_world::teleportPed(0,{220,140},-80);assert(interactionPrompt().find("PICKPOCKET")!=std::string::npos);
    interact();assert(peds[0].cash==0||peds[0].hostile);peds[0].hostile=false;peds[0].cash=0;peds[0].knockedDown=0;
    assert(interactionPrompt().find("TALK")!=std::string::npos);interact();assert(peds[0].state==PedState::Talk);
    punch();assert(peds[0].health<beforeHealth);
    std::puts("interactions: roof blocks actual theft/talk/melee actions; same underground floor permits all three");

    fixture();Vehicle car{};car.p={220,140};car.kind=Kind::Car;car.damage=100;vehicles.push_back(car);jolt_world::reset();ticks(90);
    playerAt({195,140},-80);repairKits=1;assert(interactionPrompt().find("REPAIR")==std::string::npos&&!repairVehicle(0)&&repairKits==1);
    playerAt({195,140},vehicles[0].rideHeight);assert(interactionPrompt().find("REPAIR")!=std::string::npos);interact();
    assert(vehicles[0].damage==0&&repairKits==0);
    std::puts("interactions: repair prompt and direct action share actual vehicle floor/line checks");

    fixture();buildings.push_back({80,80,440,280,120,{1,1,1},"combat-surface-building"});jolt_world::refreshScenery();
    actor({360,180},-80);playerAt({160,180},-80);auto& guard=peds[0];guard.armed=guard.hostile=true;guard.state=PedState::Attack;
    guard.alertTime=100;guard.socialCooldown=999;guard.weaponIndex=weapons::indexOf("pistol");guard.accuracy=0;
    Vec2 start=guard.p;float farthest=0;bool selected=false,fired=false;for(int n=0;n<120;++n){ai::update(dt);jolt_world::step(dt);checkBudget();
        farthest=std::max(farthest,len(guard.p-start));
        assert(std::abs(jolt_world::pedHeight(0)+80)<2);if(ped_navigation_surface::hasDestination(guard)){
            selected=true;assert(std::abs(guard.navigation.goalHeight+80)<2&&solid(guard.target,12));}
        for(const auto& shot:bullets){assert(shot.p.y<-50);fired=true;}bullets.clear();}
    std::printf("combat underground: selected %d, fired %d, farthest %.1f, target %.1f %.1f floor %.1f\n",int(selected),int(fired),farthest,guard.target.x,guard.target.z,guard.navigation.goalHeight);
    assert(selected&&fired&&farthest>20);guard.tacticTimer=0;ai::update(dt);jolt_world::step(dt);
    assert(ped_navigation_surface::hasDestination(guard));auto target=guard.target;auto cell=builder::cellAt({target.x,-79,target.z});
    assert(builder::place(cell,builder::itemIndex("granite"),false));ai::update(dt);jolt_world::step(dt);checkBudget();
    assert(!ped_navigation_surface::hasDestination(guard)||len(guard.target-target)>8);assert(std::abs(jolt_world::pedHeight(0)+80)<2);
    std::puts("combat: actual armed AI strafes/fires under original building, retains supported floor and replans an edited endpoint within shared budgets");

    // An elevated visible threat must not replace the tactical endpoint's
    // lower supported height with the player's height at that same XZ.
    fixture();for(int x=3;x<=4;++x)for(int z=3;z<=4;++z)assert(builder::place({x,0,z},builder::itemIndex("granite"),false));
    actor({360,140},0);playerAt({140,140},40);peds[0].armed=peds[0].hostile=true;peds[0].state=PedState::Attack;
    peds[0].alertTime=100;peds[0].socialCooldown=999;peds[0].weaponIndex=weapons::indexOf("pistol");start=peds[0].p;selected=false;
    for(int n=0;n<70;++n){ai::update(dt);jolt_world::step(dt);checkBudget();bullets.clear();
        if(ped_navigation_surface::hasDestination(peds[0])){selected=true;assert(std::abs(peds[0].navigation.goalHeight)<2);}assert(std::abs(jolt_world::pedHeight(0))<2);}
    assert(selected&&len(peds[0].p-start)>20);
    std::puts("combat: elevated threat does not overwrite selected tactical floor");
    reset();assert(savegame::save());jolt_world::shutdown();std::puts("Builder interaction scenarios passed");
}
