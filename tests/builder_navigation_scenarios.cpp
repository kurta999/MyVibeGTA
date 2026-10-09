#include "builder_test_support.h"
#include "../src/ped_navigation.h"
#include "../src/ped_navigation_surface.h"
#include "../src/game_internal.h"
#include "../src/jolt_world.h"
#include "../src/ai.h"
#include "../src/birds.h"
#include "../src/wildlife.h"
#include "../src/terrain.h"
#include "../src/savegame.h"
#include "../src/weapons.h"
#include "../src/police.h"
#include "../src/regions.h"
#include "../src/excavation.h"
#include "../src/traffic.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <filesystem>

namespace {
using namespace game;
constexpr float dt=1.0f/60;
std::filesystem::path runtimeFile(const char* name){char executable[MAX_PATH]{};assert(GetModuleFileNameA(nullptr,executable,MAX_PATH)>0);
    return std::filesystem::path(executable).parent_path()/name;}
void toggle(){bool next=!builder::active();assert(builder::requestToggle()&&finishBuilderTransition()&&builder::active()==next);}
void fixture(){
    reset();assert(builder::loadCatalog());buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();bullets.clear();
    player=previousPlayer={400,300};playerY=0;health=PLAYER_MAX_HEALTH;occupied=enteringVehicle=-1;jolt_world::reset();toggle();
}
void tunnel(int layer,int z0=2,int z1=4){for(int x=2;x<=12;++x)for(int z=z0;z<=z1;++z)assert(mineTerrainVolume({x,layer,z}));}
void addPed(Vec2 p,float y){Ped ped{};ped.id="builder-nav-"+std::to_string(peds.size());ped.p=ped.target=p;ped.speed=65;peds.push_back(ped);jolt_world::addPed();jolt_world::teleportPed(peds.size()-1,p,y);}
Vec2 step(std::size_t index,Vec3 goal){
    ped_navigation::beginFrame();auto before=peds[index].p;auto v=ped_navigation::velocityAtHeight(peds[index],goal,65,dt);
    jolt_world::movePed(index,v,dt);jolt_world::step(dt);
    auto stats=ped_navigation_surface::stats();assert(stats.plans<=4&&stats.physicsQueries<=18000&&stats.expanded<=12000);
    assert(len(peds[index].p-before)<3);return v;
}
bool reached(std::size_t index,Vec3 goal){return len(peds[index].p-Vec2{goal.x,goal.z})<8&&std::abs(jolt_world::pedHeight(index)-goal.y)<2;}
}
void builderNavigationScenarios(){
    using namespace game;std::setvbuf(stdout,nullptr,_IONBF,0);
    fixture();Vec3 cachedFeet{};
    ped_navigation::beginFrame();assert(ped_navigation_surface::supported({140,140},0,cachedFeet,2.5f));
    unsigned coldQueries=ped_navigation_surface::stats().physicsQueries;
    for(int n=0;n<60;++n){ped_navigation::beginFrame();
        assert(ped_navigation_surface::supported({140,140},0,cachedFeet,2.5f));
        assert(ped_navigation_surface::stats().floorCacheHits>=9);
        assert(ped_navigation_surface::stats().physicsQueries==1);}
    // Static collision invalidates cached headroom even without a builder edit.
    buildings.push_back({120,120,40,40,60,{1,1,1},"cached-floor-wall"});jolt_world::refreshScenery();
    ped_navigation::beginFrame();assert(!ped_navigation_surface::supported({140,140},0,cachedFeet,2.5f));
    buildings.clear();jolt_world::refreshScenery();ped_navigation::beginFrame();
    assert(ped_navigation_surface::supported({140,140},0,cachedFeet,2.5f));
    assert(mineTerrainVolume({3,-1,3}));
    assert(!ped_navigation_surface::supported({140,140},0,cachedFeet,2.5f));
    std::printf("builder navigation: unchanged floor checks reuse nine columns (cold %u, warm 1 query); collision refresh and same-frame mining invalidate them\n",coldQueries);

    fixture();tunnel(-2);addPed({100,100},-80);addPed({300,100},0);
    player=previousPlayer={300,100};playerY=0;jolt_world::teleportCharacter(player,playerY);
    ped_navigation::beginFrame();auto floors=jolt_world::pedestrianFloors({300,100});bool surface=false,underground=false;
    for(float floor:floors){surface|=std::abs(floor)<1;underground|=std::abs(floor+80)<1;}assert(surface&&underground);
    // Surface actors at identical XZ must neither repel nor block the walker
    // beneath the retained roof. The whole path is executed by Jolt capsules.
    Vec3 goal{440,-80,100};
    for(int n=0;n<420&&!reached(0,goal);++n){step(0,goal);assert(std::abs(jolt_world::pedHeight(0)+80)<2&&std::abs(jolt_world::pedHeight(1))<2);}
    assert(reached(0,goal));std::puts("builder navigation: roofed tunnel walked beneath surface player/pedestrian");
    // A placed block invalidates the former direct route and requires a detour
    // within the same underground level, rather than a surface-height shortcut.
    jolt_world::teleportPed(0,{100,100},-80);player=previousPlayer={400,300};playerY=0;jolt_world::teleportCharacter(player,playerY);
    for(int n=0;n<30;++n)step(0,goal);
    assert(placeEditVolume({6,-2,2},builder::itemIndex("granite"),false));bool detour=false;
    for(int n=0;n<540&&!reached(0,goal);++n){step(0,goal);detour|=peds[0].p.z>128;assert(std::abs(jolt_world::pedHeight(0)+80)<2);}
    std::printf("underground detour: %.1f remaining; %.1f height\n",len(peds[0].p-Vec2{goal.x,goal.z}),jolt_world::pedHeight(0));assert(detour&&reached(0,goal));
    assert(mineEditVolume({6,-2,2}));jolt_world::teleportPed(0,{100,100},-80);
    for(int n=0;n<420&&!reached(0,goal);++n){step(0,goal);assert(std::abs(peds[0].p.z-100)<2);}assert(reached(0,goal));

    fixture();tunnel(-2);tunnel(-4);addPed({100,100},-80);addPed({440,100},-160);
    bool crossed=false;
    for(int n=0;n<360;++n){ped_navigation::beginFrame();
        for(int index=0;index<2;++index){auto destination=index==0?Vec3{440,-80,100}:Vec3{100,-160,100};
            jolt_world::movePed(index,ped_navigation::velocityAtHeight(peds[index],destination,65,dt),dt);}
        jolt_world::step(dt);crossed|=len(peds[0].p-peds[1].p)<12;
        assert(std::abs(jolt_world::pedHeight(0)+80)<2&&std::abs(jolt_world::pedHeight(1)+160)<2);
    }
    assert(crossed&&reached(0,{440,-80,100})&&reached(1,{100,-160,100}));std::puts("builder navigation: stacked passages retain distinct floors and allow independent crossing");

    fixture();for(int x=5;x<=7;++x)for(int z=2;z<=4;++z)assert(mineTerrainVolume({x,-1,z}));addPed({150,140},0);
    goal={370,0,140};bool avoided=false;
    for(int n=0;n<600&&!reached(0,goal);++n){step(0,goal);avoided|=peds[0].p.z<75||peds[0].p.z>205;assert(jolt_world::pedHeight(0)>-2);}
    std::printf("pit detour: %.1f remaining; %.1f height\n",len(peds[0].p-Vec2{goal.x,goal.z}),jolt_world::pedHeight(0));assert(avoided&&reached(0,goal));
    auto saved=builder::capture();toggle();jolt_world::teleportPed(0,{150,140},0);
    for(int n=0;n<250&&!reached(0,goal);++n){step(0,goal);assert(std::abs(peds[0].p.z-140)<2);}assert(reached(0,goal)&&!peds[0].navigation.surfaceMode);
    toggle();assert(builder::capture()==saved);jolt_world::teleportPed(0,{150,140},0);avoided=false;
    for(int n=0;n<600&&!reached(0,goal);++n){step(0,goal);avoided|=peds[0].p.z<75||peds[0].p.z>205;assert(jolt_world::pedHeight(0)>-2);}assert(avoided&&reached(0,goal));
    std::puts("builder navigation: surface walkers avoid pits; normal F5 restores the direct route; re-entry restores the detour");

    fixture();tunnel(-2,2,2);addPed({100,100},-80);
    Building lowRoof{200,80,40,40,40,{1,1,1},"nav-low-roof"};lowRoof.damaged=true;lowRoof.pieces={{{200,-55,80},{240,-40,120}}};buildings.push_back(lowRoof);jolt_world::refreshScenery();
    ped_navigation::beginFrame();floors=jolt_world::pedestrianFloors({220,100});for(float floor:floors)assert(std::abs(floor+80)>2);
    for(int n=0;n<180;++n){step(0,{300,-80,100});assert(peds[0].p.x<192&&std::abs(jolt_world::pedHeight(0)+80)<2);}
    std::puts("builder navigation: insufficient capsule headroom is rejected without climbing through the retained roof");

    fixture();Vec2 slope{},slopeGoal{};bool found=false;
    for(int x=12600;x<14500&&!found;x+=50)for(int z=1300;z<2600&&!found;z+=50){
        Vec2 a{float(x),float(z)},b=a+Vec2{150,0};float rise=terrain::baseHeight(b)-terrain::baseHeight(a);
        if(rise>40&&rise<100&&terrain::normal(a).y>.72f&&terrain::normal(b).y>.72f&&!regions::waterAt(a)&&!regions::waterAt(b)){slope=a;slopeGoal=b;found=true;}}
    assert(found);player=previousPlayer=slope+Vec2{0,100};playerY=terrain::baseHeight(player);jolt_world::teleportCharacter(player,playerY);
    ped_navigation::beginFrame();auto startFloors=jolt_world::pedestrianFloors(slope),goalFloors=jolt_world::pedestrianFloors(slopeGoal);
    assert(!startFloors.empty()&&!goalFloors.empty());addPed(slope,startFloors.back());goal={slopeGoal.x,goalFloors.back(),slopeGoal.z};
    for(int n=0;n<360&&!reached(0,goal);++n){step(0,goal);assert(std::abs(jolt_world::pedHeight(0)-terrain::baseHeight(peds[0].p))<6);}
    std::printf("slope walk: %.1f rise, %.1f remaining\n",goal.y-startFloors.back(),len(peds[0].p-slopeGoal));assert(reached(0,goal));

    // Cached straight routes leave query budget available to the rest of a
    // nearby crowd rather than recasting every long route on every tick.
    fixture();for(int i=0;i<12;++i)addPed({100,80+float(i)*36},0);
    std::vector<Vec2> initial;for(const auto& ped:peds)initial.push_back(ped.p);
    for(int n=0;n<120;++n){ped_navigation::beginFrame();for(std::size_t i=0;i<peds.size();++i)
        jolt_world::movePed(i,ped_navigation::velocityAtHeight(peds[i],{400,0,initial[i].z},65,dt),dt);
        jolt_world::step(dt);assert(ped_navigation_surface::stats().physicsQueries<=18000);}
    for(std::size_t i=0;i<peds.size();++i)assert(peds[i].p.x>initial[i].x+70);
    std::printf("builder navigation: all 12 walkers move with shared query budget; steady frame %u queries\n",ped_navigation_surface::stats().physicsQueries);

    fixture();tunnel(-2);addPed({260,100},0);player=previousPlayer={260,100};playerY=-80;jolt_world::teleportCharacter(player,playerY);
    auto& attacker=peds[0];attacker.state=PedState::Attack;attacker.hostile=true;attacker.alertTime=10;attacker.speed=0;attacker.fireCooldown=0;
    float hp=health;ai::update(dt);assert(health==hp&&bullets.empty()&&!ai::talkToPed(0));
    assert(!police::report(police::Crime::Assault,player,true,false));
    attacker.armed=true;attacker.weaponIndex=weapons::indexOf("pistol");attacker.fireCooldown=0;ai::update(dt);assert(bullets.empty());
    jolt_world::teleportPed(0,{230,100},-80);attacker.armed=false;attacker.hostile=false;attacker.state=PedState::Wander;attacker.knockedDown=0;
    assert(ai::talkToPed(0));attacker.state=PedState::Attack;attacker.hostile=true;attacker.alertTime=10;attacker.armed=true;attacker.fireCooldown=0;attacker.socialPartner=-1;
    assert(police::report(police::Crime::Assault,player,true,false));
    ai::update(dt);assert(!bullets.empty()&&std::abs(bullets.back().p.y+62)<2);
    bullets.clear();attacker.armed=false;attacker.fireCooldown=0;jolt_world::teleportPed(0,{240,100},-80);hp=health;ai::update(dt);assert(health<hp);
    // Dispatch now requires a reachable, concealed spawn at least 130 units
    // away. Extend the passage to a rear spawn instead of relying on the old
    // surface-spawn fallback above this sealed tunnel.
    for(int x=13;x<=18;++x)for(int z=2;z<=4;++z)assert(mineTerrainVolume({x,-2,z}));
    cameraYaw=PI;ped_navigation::beginFrame();
    police::setWantedLevel(1);police::update(.01f);bool officerFound=false;
    for(const auto& ped:peds)if(ped.police){officerFound=true;assert(std::abs(ped.lastKnownHeight+80)<1);}
    assert(officerFound);police::setWantedLevel(0);
    std::puts("builder navigation: actual actor heights prevent roof-through sight, talk and melee; underground sight, talk and attacks work");

    fixture();for(int x=2;x<=36;++x)for(int z=2;z<=4;++z)assert(mineTerrainVolume({x,-2,z}));addPed({100,100},-80);
    // Actual AI at a distance formerly handled by the flat, 420-unit fallback.
    player=previousPlayer={850,300};playerY=0;jolt_world::teleportCharacter(player,playerY);
    peds[0].state=PedState::Investigate;peds[0].target={1000,100};peds[0].alertTime=30;
    for(int n=0;n<90;++n){ai::update(dt);jolt_world::step(dt);assert(std::abs(jolt_world::pedHeight(0)+80)<2);}
    assert(peds[0].p.x>160&&jolt_world::activePedCharacterCount()==1);
    auto beforeStreaming=peds[0].p;float heightBeforeStreaming=jolt_world::pedHeight(0);
    player=previousPlayer={1800,300};jolt_world::teleportCharacter(player,0);
    assert(jolt_world::activePedCharacterCount()==0&&std::abs(jolt_world::pedHeight(0)-heightBeforeStreaming)<.01f);
    for(int n=0;n<30;++n){ai::update(dt);jolt_world::step(dt);assert(len(peds[0].p-beforeStreaming)<.01f);}
    player=previousPlayer={400,300};jolt_world::teleportCharacter(player,0);
    std::printf("stream return before movement: characters %zu, before %.4f, restored %.4f, vertical %.4f\n",jolt_world::activePedCharacterCount(),heightBeforeStreaming,jolt_world::pedHeight(0),peds[0].elevationVelocity);
    assert(jolt_world::activePedCharacterCount()==1&&std::abs(jolt_world::pedHeight(0)-heightBeforeStreaming)<.01f);
    for(int n=0;n<90;++n){ai::update(dt);jolt_world::step(dt);assert(std::abs(jolt_world::pedHeight(0)+80)<2);}
    assert(peds[0].p.x>beforeStreaming.x+80&&jolt_world::activePedCharacterCount()==1);
    std::puts("builder streaming: real far AI walks underground; unloaded pose stays on its floor and resumes without a surface teleport");

    fixture();for(int x=2;x<=4;++x)for(int z=2;z<=4;++z)for(int y=-2;y<=-1;++y)assert(mineTerrainVolume({x,y,z}));addPed({140,140},0);
    for(int n=0;n<12;++n){jolt_world::movePed(0,{},dt);jolt_world::step(dt);}
    float fallHeight=jolt_world::pedHeight(0),fallVelocity=peds[0].elevationVelocity;assert(fallHeight<-5&&fallHeight>-70&&fallVelocity<-50);
    player=previousPlayer={1800,300};jolt_world::teleportCharacter(player,0);assert(jolt_world::activePedCharacterCount()==0);
    assert(std::abs(jolt_world::pedHeight(0)-fallHeight)<.01f&&peds[0].elevationVelocity==fallVelocity);
    player=previousPlayer={400,300};jolt_world::teleportCharacter(player,0);
    assert(jolt_world::activePedCharacterCount()==1&&std::abs(jolt_world::pedHeight(0)-fallHeight)<.01f&&peds[0].elevationVelocity==fallVelocity);
    jolt_world::movePed(0,{},dt);
    assert(jolt_world::pedHeight(0)<fallHeight-1&&peds[0].elevationVelocity<fallVelocity);
    for(int n=0;n<60;++n){jolt_world::movePed(0,{},dt);jolt_world::step(dt);}assert(std::abs(jolt_world::pedHeight(0)+80)<2);
    std::puts("builder streaming: in-progress fall retains height and gravity velocity across capsule unloading/recreation");

    // F5 can run while an actor is outside the recovery radius. Its stale
    // underground pose must be reconciled when normal collision streams in.
    player=previousPlayer={1800,300};jolt_world::teleportCharacter(player,0);toggle();assert(!builder::active());
    player=previousPlayer={400,300};jolt_world::teleportCharacter(player,0);
    assert(jolt_world::activePedCharacterCount()==1&&jolt_world::pedHeight(0)>-2);toggle();jolt_world::movePed(0,{},dt);
    for(int n=0;n<60;++n){jolt_world::movePed(0,{},dt);jolt_world::step(dt);}assert(std::abs(jolt_world::pedHeight(0)+80)<2);
    std::puts("builder streaming: distant F5 layer change reconciles the actor against incoming collision when it returns");

    // Reusing a vector slot must not reuse the previous actor's character or
    // cached floor; generated/save-loaded actors begin with an unknown pose.
    Ped replacement{};replacement.id="builder-nav-replacement";replacement.p=replacement.target={140,140};replacement.speed=65;
    float replacementFloor=terrain::height(replacement.p);
    peds[0]=replacement;jolt_world::preparePedNavigation();
    assert(jolt_world::activePedCharacterCount()==1&&std::abs(jolt_world::pedHeight(0)-replacementFloor)<.01f&&peds[0].elevationVelocity==0);
    jolt_world::teleportPed(0,replacement.p,replacementFloor+25);
    peds[0]=replacement;jolt_world::preparePedNavigation();
    assert(jolt_world::activePedCharacterCount()==1&&std::abs(jolt_world::pedHeight(0)-replacementFloor)<.01f&&peds[0].elevationVelocity==0);
    peds[0]=replacement;peds[0].id="builder-nav-0";jolt_world::preparePedNavigation();
    assert(jolt_world::activePedCharacterCount()==1&&std::abs(jolt_world::pedHeight(0)-replacementFloor)<.01f&&peds[0].elevationVelocity==0);
    std::puts("builder streaming: returning actor is reconciled before movement/render; replacement actors never inherit an old floor");

    fixture();player=previousPlayer={1000,1000};playerY=0;
    Tree edgeTree{};edgeTree.id="builder-nav-stream-edge";edgeTree.p={2160,1000};edgeTree.scale=2;trees.push_back(edgeTree);
    jolt_world::refreshScenery();jolt_world::teleportCharacter(player,0);addPed({2120,1000},0);
    Vec3 anchor{};assert(jolt_world::staticAnchor({2100,20,1000},{1,0,0},100,anchor));
    for(int n=0;n<90;++n){jolt_world::movePed(0,{130,0},dt);jolt_world::step(dt);}
    assert(peds[0].p.x<2143&&std::abs(jolt_world::pedHeight(0))<2);

    // Both focus axes move inside the same streaming cell. Obstacles that were
    // far from the old focus can now be inside the 1,200-unit actor budget.
    fixture();player=previousPlayer={400.1f,400.1f};playerY=0;
    Building edgeBuilding{};edgeBuilding.id="builder-nav-cell-edge";edgeBuilding.x=1600;edgeBuilding.z=1600;edgeBuilding.w=edgeBuilding.d=80;edgeBuilding.h=80;
    buildings.push_back(edgeBuilding);jolt_world::refreshScenery();jolt_world::teleportCharacter(player,0);
    assert(placeEditVolume({43,0,38},builder::itemIndex("granite"),false));jolt_world::preparePedNavigation();
    player=previousPlayer={799.9f,799.9f};jolt_world::teleportCharacter(player,0);
    assert(jolt_world::staticAnchor({1560,20,1640},{1,0,0},100,anchor));
    assert(jolt_world::staticAnchor({1680,20,1540},{1,0,0},100,anchor));
    addPed({1560,1640},0);for(int n=0;n<90;++n){jolt_world::movePed(0,{130,0},dt);jolt_world::step(dt);}
    std::printf("cell drift building walker: %.2f %.2f %.2f; terrain %.2f\n",peds[0].p.x,jolt_world::pedHeight(0),peds[0].p.z,terrain::height(peds[0].p));
    assert(peds[0].p.x<1594&&std::abs(jolt_world::pedHeight(0))<2);
    std::puts("builder streaming: tree, building and placed-block collision cover the actor boundary and diagonal focus-cell drift");

    fixture();tunnel(-2,0,8);Vehicle car{};car.id="underground-nav-car";car.kind=Kind::Car;car.p={340,140};car.angle=0;vehicles.push_back(car);jolt_world::reset();
    jolt_world::teleportVehicle(0,car.p,car.angle,-80);addPed(car.p,-80);
    Vehicle surfaceImpact=vehicles[0];surfaceImpact.rideHeight=0;surfaceImpact.speed=35;surfaceImpact.velocity={35,0};
    int pedHealth=peds[0].health;assert(!ai::vehicleImpact(peds[0],surfaceImpact)&&peds[0].health==pedHealth);
    auto undergroundImpact=surfaceImpact;undergroundImpact.rideHeight=-80;assert(ai::vehicleImpact(peds[0],undergroundImpact)&&peds[0].health<pedHealth);
    // A driver leaving a real underground car retains the door's floor, even
    // when its old virtual capsule has already been released for driving.
    peds[0].drivingVehicle=0;peds[0].knockedDown=0;vehicles[0].driver=0;vehicles[0].speed=0;jolt_world::step(dt);
    assert(std::abs(jolt_world::pedHeight(0)-vehicles[0].rideHeight)<.01f);traffic::carjacked(0);
    assert(peds[0].drivingVehicle<0&&std::abs(jolt_world::pedHeight(0)+80)<2);
    jolt_world::teleportPed(0,{100,140},-80);
    player=previousPlayer={450,140};playerY=-80;jolt_world::teleportCharacter(player,playerY);traffic::provoke(peds[0],player);peds[0].carSearchCooldown=0;
    jolt_world::teleportVehicle(0,car.p,car.angle,0);traffic::updatePed(peds[0],dt);assert(peds[0].seekingVehicle<0);
    jolt_world::teleportVehicle(0,car.p,car.angle,-80);peds[0].carSearchCooldown=0;
    assert(placeEditVolume({5,-2,3},builder::itemIndex("granite"),false));traffic::updatePed(peds[0],dt);assert(peds[0].seekingVehicle<0);
    assert(mineEditVolume({5,-2,3}));peds[0].carSearchCooldown=0;
    for(int n=0;n<240&&peds[0].drivingVehicle<0;++n){ped_navigation::beginFrame();traffic::updatePed(peds[0],dt);jolt_world::step(dt);}
    std::printf("underground boarding: ped %.1f %.1f y %.1f, car %.1f %.1f y %.1f, state %d seeking %d\n",peds[0].p.x,peds[0].p.z,jolt_world::pedHeight(0),vehicles[0].p.x,vehicles[0].p.z,vehicles[0].rideHeight,int(peds[0].state),peds[0].seekingVehicle);
    assert(peds[0].drivingVehicle==0&&std::abs(jolt_world::pedHeight(0)-vehicles[0].rideHeight)<.01f);
    std::puts("builder vehicle heights: surface impacts/cars cannot reach underground pedestrians; real door exit, obstruction and reboarding retain the underground floor");

    traffic::carjacked(0);jolt_world::teleportPed(0,{100,200},-80);player=previousPlayer={265,140};playerY=-80;jolt_world::teleportCharacter(player,playerY);
    assert(traffic::canPlayerEnter(0));player=previousPlayer={225.5f,140};jolt_world::teleportCharacter(player,playerY);
    // Place while both actors are clear, then bring the narrow side of the
    // chassis beside it. The legal block now separates an in-reach player.
    assert(placeEditVolume({6,-2,3},builder::itemIndex("granite"),false));jolt_world::teleportVehicle(0,{310,140},PI/2,-80);
    assert(!traffic::canPlayerEnter(0));
    occupied=0;player=previousPlayer=vehicles[0].p;playerY=vehicles[0].rideHeight;enterExit();
    assert(occupied<0&&player.x>340&&std::abs(playerY+80)<2);
    std::puts("builder vehicle heights: player entry rejects the underground wall and exit chooses the clear door without teleporting through terrain");

    reset();assert(savegame::save());jolt_world::shutdown();std::puts("builder navigation scenarios passed");
}

void builderDestinationScenarios(){
    using namespace game;std::setvbuf(stdout,nullptr,_IONBF,0);
    // Original surface footprints must not reject a legal destination in the
    // lower passage. The actual AI chooses its own targets and moves capsules.
    fixture();tunnel(-2,2,8);
    buildings.push_back({80,80,440,280,120,{1,1,1},"destination-surface-building"});jolt_world::refreshScenery();
    addPed({240,180},-80);player=previousPlayer={400,400};playerY=0;jolt_world::teleportCharacter(player,playerY);
    ped_navigation::beginFrame();Vec3 chosen{};
    assert(solid({360,180},12)&&ped_navigation_surface::destination(peds[0],{360,180},chosen));
    assert(std::abs(chosen.y+80)<2&&len(Vec2{chosen.x,chosen.z}-Vec2{360,180})<1);
    peds[0].target=peds[0].p;peds[0].navigation={};peds[0].socialCooldown=999;std::srand(41);auto start=peds[0].p;float farthest=0;int undergroundTargets=0;
    for(int n=0;n<180;++n){ai::update(dt);jolt_world::step(dt);
        assert(std::abs(jolt_world::pedHeight(0)+80)<2&&ped_navigation_surface::stats().physicsQueries<=18000);
        if(ped_navigation_surface::hasDestination(peds[0])){++undergroundTargets;assert(std::abs(peds[0].navigation.goalHeight+80)<2&&solid(peds[0].target,12));}
        farthest=std::max(farthest,len(peds[0].p-start));}
    std::printf("builder destinations: underground wander %.1f units, %d supported lower-floor targets beneath original building\n",farthest,undergroundTargets);
    assert(farthest>40&&undergroundTargets>120);

    fixture();for(int x=5;x<=7;++x)for(int z=2;z<=4;++z)assert(mineTerrainVolume({x,-1,z}));addPed({150,140},0);
    player=previousPlayer={100,140};playerY=0;jolt_world::teleportCharacter(player,playerY);ped_navigation::beginFrame();
    assert(ai::notifyThreat(player,0)==1&&peds[0].state==PedState::Flee);bool detour=false;
    for(int n=0;n<240;++n){ai::update(dt);jolt_world::step(dt);detour|=peds[0].p.z<75||peds[0].p.z>205;
        assert(jolt_world::pedHeight(0)>-2&&ped_navigation_surface::stats().plans<=4&&ped_navigation_surface::stats().physicsQueries<=18000);}
    std::printf("builder destinations: real fleeing actor %.1f %.1f avoids pit, detour %d\n",peds[0].p.x,peds[0].p.z,int(detour));
    assert(detour&&peds[0].p.x>310);

    fixture();addPed({100,140},0);ped_navigation::beginFrame();
    assert(ped_navigation_surface::destination(peds[0],{260,140},chosen));peds[0].target={chosen.x,chosen.z};
    assert(ped_navigation_surface::hasDestination(peds[0])&&std::abs(chosen.y)<2);
    assert(mineTerrainVolume({6,-1,3}));assert(!ped_navigation_surface::hasDestination(peds[0]));
    // No beginFrame here: an edit between a player action and AI notification
    // must invalidate cached floor samples in that same frame.
    assert(ped_navigation_surface::destination(peds[0],{260,140},chosen));
    std::printf("builder destinations: edited-floor destination %.3f %.3f %.3f queries %u\n",chosen.x,chosen.y,chosen.z,ped_navigation_surface::stats().physicsQueries);
    assert(std::abs(chosen.y)<2&&(chosen.x<231||chosen.x>289||chosen.z<111||chosen.z>169));
    assert(jolt_world::pedestrianClear(chosen));
    std::printf("builder destinations: same-frame terrain edit replaces cached floor with supported approach %.1f %.1f y %.1f\n",chosen.x,chosen.z,chosen.y);

    // Cover can be an actual player-placed block beneath a retained roof.
    // Both upper/lower body sight lines, the reachable floor and approach are
    // checked; the original ground-height line is clear and is misleading.
    fixture();tunnel(-2,2,5);assert(placeEditVolume({6,-2,3},builder::itemIndex("granite"),false));addPed({360,140},-80);
    player=previousPlayer={160,140};playerY=-80;jolt_world::teleportCharacter(player,playerY);
    auto& guard=peds[0];guard.armed=true;guard.weaponIndex=weapons::indexOf("pistol");guard.tacticTimer=10;
    ped_navigation::beginFrame();ai::reactToHit(guard,player);
    std::printf("builder destinations: selected cover state %d at %.1f %.1f floor %.1f\n",int(guard.state),guard.target.x,guard.target.z,guard.navigation.goalHeight);
    assert(guard.state==PedState::TakeCover&&ped_navigation_surface::hasDestination(guard)&&std::abs(guard.navigation.goalHeight+80)<2);
    assert(clearLine(player,guard.target));
    assert(!clearLineAtHeight({player.x,playerY+22,player.z},{guard.target.x,guard.navigation.goalHeight+28,guard.target.z}));
    bool defended=false;for(int n=0;n<360;++n){ai::update(dt);jolt_world::step(dt);defended|=guard.state==PedState::Defend;assert(std::abs(jolt_world::pedHeight(0)+80)<2);}
    std::printf("builder destinations: cover execution %.1f %.1f y %.1f state %d defended %d target %.1f %.1f nav %.1f selected %d\n",guard.p.x,guard.p.z,jolt_world::pedHeight(0),int(guard.state),int(defended),guard.target.x,guard.target.z,guard.navigation.goalHeight,int(guard.navigation.destinationSelected));
    assert(defended&&guard.p.x>280&&guard.p.x<330);
    assert(guard.state==PedState::Defend&&guard.sightMemory==0&&guard.tacticTimer>0&&ped_navigation_surface::hasDestination(guard));
    guard.tacticTimer=.01f;ai::update(dt);jolt_world::step(dt);
    assert(guard.state==PedState::Investigate&&!guard.navigation.destinationSelected);
    // A further threat while already sheltered may retain a nearby cover
    // endpoint; it must not force an unnecessary eight-unit relocation.
    guard.tacticTimer=10;ped_navigation::beginFrame();ai::reactToHit(guard,player);
    assert(guard.state==PedState::TakeCover);ai::update(dt);jolt_world::step(dt);assert(guard.state==PedState::Defend);
    auto formerCover=guard.target;assert(mineEditVolume({6,-2,3}));ai::update(dt);jolt_world::step(dt);
    assert(guard.state==PedState::Attack);
    // Removing cover can now immediately select a supported Attack endpoint
    // in this same AI update; the former cover route must not remain stale.
    if(guard.navigation.destinationSelected){assert(ped_navigation_surface::hasDestination(guard)&&std::abs(guard.navigation.goalHeight+80)<2&&len(guard.target-formerCover)>8);
        assert(jolt_world::pedestrianClear({guard.target.x,guard.navigation.goalHeight,guard.target.z}));}
    std::puts("builder destinations: cover persists beyond sight memory, expires on the investigation timer, can be retained when already sheltered, and removal permits only valid supported Attack routes");

    fixture();tunnel(-2,2,5);assert(placeEditVolume({6,-2,3},builder::itemIndex("granite"),false));addPed({360,140},-80);
    player=previousPlayer={160,140};playerY=-80;jolt_world::teleportCharacter(player,playerY);
    peds[0].armed=true;peds[0].weaponIndex=weapons::indexOf("pistol");peds[0].fireCooldown=100;
    ped_navigation::beginFrame();ai::notifyGunshot(player);assert(peds[0].state==PedState::TakeCover&&peds[0].tacticTimer==0);
    for(int n=0;n<80;++n){ai::update(dt);jolt_world::step(dt);}
    assert(peds[0].state==PedState::Defend&&peds[0].tacticTimer>0);bool investigated=false;
    for(int n=0;n<180;++n){ai::update(dt);jolt_world::step(dt);investigated|=peds[0].state==PedState::Investigate;assert(std::abs(jolt_world::pedHeight(0)+80)<2);}
    assert(investigated);std::puts("builder destinations: ordinary gunshot response with default timer reaches and holds cover before investigating");

    fixture();for(int i=0;i<12;++i){addPed({600+float(i%4)*60,600+float(i/4)*60},0);peds.back().socialCooldown=999;}
    std::vector<Vec2> initial;for(const auto& ped:peds)initial.push_back(ped.p);std::srand(23);unsigned peak=0;
    for(int n=0;n<150;++n){ai::update(dt);jolt_world::step(dt);auto stats=ped_navigation_surface::stats();peak=std::max(peak,stats.physicsQueries);
        assert(stats.plans<=4&&stats.expanded<=12000&&stats.physicsQueries<=18000);}
    for(std::size_t i=0;i<peds.size();++i){assert(len(peds[i].p-initial[i])>15&&std::abs(jolt_world::pedHeight(i))<2);}
    std::printf("builder destinations: 12 real wandering actors choose and execute routes within budget, peak %u queries\n",peak);
    toggle();assert(!ped_navigation_surface::hasDestination(peds[0]));ai::update(dt);jolt_world::step(dt);assert(!peds[0].navigation.surfaceMode);
    toggle();std::srand(23);for(int n=0;n<60;++n){ai::update(dt);jolt_world::step(dt);assert(ped_navigation_surface::stats().physicsQueries<=18000);}
    bool selectedAgain=false;for(const auto& ped:peds)selectedAgain|=ped_navigation_surface::hasDestination(ped);assert(selectedAgain);
    reset();assert(savegame::save());jolt_world::shutdown();std::puts("builder destination scenarios passed");
}

void builderNavigationRestartSave(){
    using namespace game;fixture();tunnel(-2);
    for(int x=5;x<=7;++x)for(int z=6;z<=8;++z)assert(mineTerrainVolume({x,-1,z}));
    assert(placeEditVolume({6,-2,2},builder::itemIndex("granite"),false));
    assert(savegame::save());std::filesystem::copy_file(runtimeFile("savegame.ini"),runtimeFile("builder-navigation-restart.ini"),std::filesystem::copy_options::overwrite_existing);
    jolt_world::shutdown();std::puts("builder navigation restart: edited tunnel, pit and placed obstacle saved; process exits");
}

void builderNavigationRestartLoad(){
    using namespace game;std::filesystem::copy_file(runtimeFile("builder-navigation-restart.ini"),runtimeFile("savegame.ini"),std::filesystem::copy_options::overwrite_existing);
    assert(savegame::load()&&!builder::active()&&excavation::cells().size()==336&&builder::blocks().size()==8);
    assert(builder::blocks().begin()->second.item==builder::itemIndex("granite"));
    // Recreate the same controlled surroundings; the edited cells/block are
    // read only from the previous process's save, never rebuilt by this phase.
    buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();bullets.clear();
    player=previousPlayer={400,400};playerY=0;jolt_world::reset();toggle();
    addPed({100,100},-80);Vec3 goal{440,-80,100};bool detour=false;
    for(int n=0;n<600&&!reached(0,goal);++n){step(0,goal);detour|=peds[0].p.z>128;assert(std::abs(jolt_world::pedHeight(0)+80)<2);}
    assert(detour&&reached(0,goal));addPed({150,280},0);goal={370,0,280};detour=false;
    for(int n=0;n<600&&!reached(1,goal);++n){step(1,goal);detour|=peds[1].p.z<235||peds[1].p.z>365;assert(jolt_world::pedHeight(1)>-2);}
    assert(detour&&reached(1,goal));toggle();jolt_world::teleportPed(1,{150,280},0);
    for(int n=0;n<250&&!reached(1,goal);++n){step(1,goal);assert(std::abs(peds[1].p.z-280)<2);}assert(reached(1,goal));
    toggle();assert(excavation::cells().size()==336&&builder::blocks().size()==8);
    reset();assert(savegame::save());jolt_world::shutdown();std::puts("builder navigation restart: fresh process walks saved tunnel/block and pit detours; F5 restores the direct normal route and saved edits");
}
