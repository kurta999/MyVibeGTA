#include "../src/game_internal.h"
#include "../src/ordnance.h"
#include "../src/debug_menu.h"
#include "../src/police.h"
#include "../src/jolt_world.h"
#include "../src/destruction.h"
#include "../src/fire.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#include "../src/physics.h"
#include "../src/savegame.h"
#include "../src/terrain.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

namespace {
using namespace game;
using weapons::Payload;
void stage(){
    reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();
    wildlife::animals.clear();birds::flock.clear();fire::reset();ordnance::reset();police::reset();
    player=previousPlayer={4500,4500};playerY=0;grounded=true;cameraYaw=cameraPitch=0;
    leftMouse=rightMouse=false;std::fill(std::begin(keys),std::end(keys),false);
    health=PLAYER_MAX_HEALTH;debug_menu::godMode=true;jolt_world::reset();
}
void equip(const char* id){weapon=weapons::indexOf(id);assert(weapon>=0);
    unlocked[weapon]=true;magazine[weapon]=weapons::stats(weapon).magazine;
    ammo[weapon]=100;fireCooldown=reloadRemaining=0;}
void ticks(int count){for(int i=0;i<count;++i)update(1.0f/60);}
Ped ped(Vec2 p,float angle=0){Ped result{};result.id="ordnance-ped";result.p=result.target=p;
    result.health=100;result.alive=true;result.angle=angle;return result;}
}
void ordnanceScenarios(){
    using namespace game;
    for(const char* id:{"c4","remote-trigger","grenade","smoke-grenade","molotov","flashbang","timed-bomb"}){
        int i=weapons::indexOf(id);assert(i>=20&&weapons::stats(i).payload!=Payload::None);
    }
    // Real held input must throw after the capsule settles onto a city street.
    stage();player=previousPlayer={772,242};jolt_world::teleportCharacter(player,0);
    equip("c4");ticks(30);
    std::printf("C4 city capsule height: %.6f\n",playerY);
    assert(grounded&&!swimming);
    leftMouse=true;ticks(1);leftMouse=false;
    assert(ordnance::c4Count()==1&&magazine[weapon]==39&&ammo[weapon]==100);
    ticks(120);assert(ordnance::devices[0].settled);
    equip("remote-trigger");shoot();assert(ordnance::c4Count()==0&&!blasts.empty());

    // A dry basin below sea level is also valid throwing ground.
    stage();player=previousPlayer={13800,4550};playerY=terrain::height(player)+4;
    jolt_world::teleportCharacter(player,playerY);ticks(120);
    assert(grounded&&!swimming&&playerY<-100);
    equip("c4");rightMouse=true;cameraPitch=-.2f;
    leftMouse=true;ticks(1);leftMouse=false;
    assert(ordnance::c4Count()==1&&magazine[weapon]==39&&ammo[weapon]==100);
    ticks(180);assert(ordnance::devices[0].settled&&ordnance::devices[0].p.y<0);
    fireCooldown=0;shoot();assert(ordnance::c4Count()==2&&magazine[weapon]==38);
    equip("grenade");shoot();assert(ordnance::devices.size()==3&&magazine[weapon]==5);
    // Swimming still blocks placement and preserves ammunition.
    stage();equip("c4");swimming=true;playerY=-20;shoot();
    assert(ordnance::devices.empty()&&magazine[weapon]==40&&ammo[weapon]==100);

    // The cap includes airborne charges, even with infinite debug ammunition.
    stage();equip("c4");debug_menu::infiniteAmmo=true;
    for(int i=0;i<40;++i){fireCooldown=0;shoot();}
    assert(ordnance::c4Count()==40);fireCooldown=0;shoot();assert(ordnance::c4Count()==40);
    assert(unlocked[weapons::indexOf("remote-trigger")]);
    ordnance::detonateRemote();assert(ordnance::c4Count()==0&&blasts.size()>=40);

    // Swept wall hit and remote explosion share RPG collision-hole destruction.
    stage();buildings.push_back({4550,4440,70,120,130,rgb(140,140,140),"c4-wall"});
    jolt_world::reset();equip("c4");shoot();ticks(120);
    assert(ordnance::c4Count()==1&&ordnance::devices[0].settled);
    Vec3 stuck=ordnance::devices[0].p;assert(stuck.x<4551);
    ticks(90);assert(len(ordnance::devices[0].p-stuck)<.01f);
    equip("remote-trigger");shoot();assert(ordnance::devices.empty());
    assert(buildings[0].damaged&&!buildings[0].cuts.empty());
    assert(!destruction::contains(buildings[0],stuck+Vec3{5,0,0}));

    // Vehicle attachment follows translation and full rigid orientation.
    stage();Vehicle car{};car.p={4550,4500};car.kind=Kind::Car;car.id="sticky-car";
    vehicles.push_back(car);
    ordnance::devices.push_back({Payload::C4,{4500,30,4500},{800,0,0},{0,1,0},{},{0,1,0},3,130,210});
    ordnance::update(.1f);assert(ordnance::devices[0].settled&&ordnance::devices[0].vehicle==0);
    Vec3 before=ordnance::devices[0].p;
    vehicles[0].p.x+=100;vehicles[0].p.z+=40;vehicles[0].rideHeight+=15;
    ordnance::update(.01f);Vec3 translated=ordnance::devices[0].p;
    assert(len(translated-before-Vec3{100,15,40})<.01f);
    vehicles[0].qy=std::sin(PI/4);vehicles[0].qw=std::cos(PI/4);
    ordnance::update(.01f);assert(len(ordnance::devices[0].p-translated)>10);
    Vec2 vehicleCenter=vehicles[0].p;ordnance::detonateRemote();assert(vehicles[0].damage>0);
    assert(len(Vec2{blasts[0].p.x,blasts[0].p.z}-vehicleCenter)<100);

    // A frag bounces off the ground and detonates only after its fuse.
    stage();ordnance::devices.push_back({Payload::Grenade,{4560,4,4500},{35,-80,0},
        {0,1,0},{},{0,1,0},1.2f,115,140});
    ordnance::update(.1f);assert(ordnance::devices.size()==1&&ordnance::devices[0].p.y>=0&&
        ordnance::devices[0].v.y>0&&blasts.empty());
    buildings.push_back({4560,4480,70,80,100,rgb(140,140,140),"frag-wall"});
    for(int i=0;i<80;++i)ordnance::update(1.0f/60);
    assert(ordnance::devices.empty()&&!blasts.empty()&&buildings[0].damaged);

    // Smoke blocks police/AI sight, expires, and does not cut buildings.
    stage();buildings.push_back({4700,4500,80,80,100,rgb(140,140,140),"smoke-wall"});
    ordnance::devices.push_back({Payload::Smoke,{4550,0,4500},{},{0,1,0},{},{0,1,0},.01f,0,0,true});
    ordnance::update(.02f);ordnance::update(1);
    assert(!clearLine({4500,4500},{4600,4500})&&!buildings[0].damaged&&blasts.empty());
    ordnance::update(25);assert(clearLine({4500,4500},{4600,4500}));

    // Flashbang stuns nearby visible actors and flashes the player without lethal damage.
    stage();peds.push_back(ped({4540,4500}));
    ordnance::devices.push_back({Payload::Flashbang,{4550,20,4500},{},{0,1,0},{},{0,1,0},.01f,0,0,true});
    ordnance::update(.02f);assert(peds[0].stunRemaining==5&&peds[0].health==100&&ordnance::flash>0);
    ordnance::update(3);assert(ordnance::flash==0);

    // Molotov breaks on impact and ignites actors and a persistent fire patch.
    stage();peds.push_back(ped({4550,4500}));vehicles.push_back(car);
    ordnance::devices.push_back({Payload::Molotov,{4550,3,4500},{0,-80,0},
        {0,1,0},{},{0,1,0},8,0,0});ordnance::update(.1f);
    assert(ordnance::devices.empty()&&!fire::active().empty()&&peds[0].burnTime>0&&vehicles[0].burnTime>0);

    // Timer UI supports cancellation/invalid values and consumes only on placement.
    stage();equip("timed-bomb");int count=magazine[weapon];shoot();assert(ordnance::timerOpen());
    ordnance::timerKey(VK_ESCAPE);assert(!ordnance::timerOpen()&&magazine[weapon]==count&&ordnance::devices.empty());
    fireCooldown=0;shoot();ordnance::timerKey('0');ordnance::timerKey(VK_RETURN);
    assert(ordnance::timerOpen()&&ordnance::devices.empty());
    ordnance::timerKey(VK_BACK);ordnance::timerKey('2');ordnance::timerKey(VK_RETURN);
    assert(!ordnance::timerOpen()&&magazine[weapon]==count-1&&ordnance::devices.size()==1);
    buildings.push_back({4650,4400,160,200,200,rgb(140,140,140),"bomb-near"});
    buildings.push_back({4710,4800,130,100,130,rgb(140,140,140),"bomb-far"});
    car.p={4600,4500};vehicles.push_back(car);peds.push_back(ped({4590,4500}));
    Prop prop{};prop.p={4520,4500};props.push_back(prop);
    ticks(110);assert(!ordnance::devices.empty()&&!buildings[0].damaged);
    ticks(20);assert(ordnance::devices.empty()&&buildings[0].damaged&&buildings[1].damaged);
    assert(vehicles[0].exploded&&!peds[0].alive&&props[0].health<=0);
    assert(!ordnance::smoke.empty()&&!fire::active().empty());

    // Knife takedown requires being behind; unseen victims cannot self-report.
    stage();equip("knife");peds.push_back(ped({4530,4500}));
    assert(stealthTarget()==0);stealthKill();assert(!peds[0].alive);
    for(int i=0;i<100;++i)police::update(.02f);assert(police::wantedLevel()==0);
    stage();equip("knife");peds.push_back(ped({4530,4500},PI));assert(stealthTarget()<0);
    peds[0].angle=0;auto witness=ped({4530,4550},-PI/2);witness.id="witness";peds.push_back(witness);
    stealthKill();for(int i=0;i<100;++i)police::update(.02f);assert(police::wantedLevel()>0);
    stage();equip("knife");peds.push_back(ped({4530,4500}));
    witness=ped({4530,4550},-PI/2);witness.id="occluded-witness";peds.push_back(witness);
    buildings.push_back({4510,4523,40,2,90,rgb(140,140,140),"thin-witness-wall"});
    stealthKill();for(int i=0;i<100;++i)police::update(.02f);assert(police::wantedLevel()==0);
    stage();equip("knife");peds.push_back(ped({4530,4500}));
    witness=ped({4530,4550},PI/2);witness.id="rear-witness";peds.push_back(witness);
    stealthKill();for(int i=0;i<100;++i)police::update(.02f);assert(police::wantedLevel()==0);

    // Hideouts lose one star per 15 stationary unseen seconds; visible police interrupt.
    reset();assert(!police::hideouts().empty());Vec2 hidingPlace=police::hideouts().front();
    peds.clear();health=0;player=hidingPlace;playerVelocity={};playerY=0;occupied=-1;
    police::setWantedLevel(2);health=PLAYER_MAX_HEALTH;
    for(int i=0;i<59;++i){police::update(.25f);peds.clear();}
    assert(police::wantedLevel()==2&&police::hiding());police::update(.25f);
    assert(police::wantedLevel()==1);
    auto officer=ped(hidingPlace+Vec2{10,0});officer.police=true;peds={officer};
    police::update(.25f);assert(!police::hiding()&&police::hideProgress()==0);

    // Mouse swimming descends, holds depth, ascends and never fires a weapon.
    stage();player=previousPlayer={600,2000};playerY=0;jolt_world::teleportCharacter(player,0);
    leftMouse=true;ticks(60);assert(swimming&&playerY<-35&&bullets.empty());
    leftMouse=false;float depth=playerY;ticks(30);assert(std::abs(playerY-depth)<.1f);
    rightMouse=true;ticks(120);assert(swimming&&std::abs(playerY)<.1f);rightMouse=false;

    // Actual projectile range is clipped even for a steep high-speed trajectory.
    stage();bullets.push_back({{4500,200,4500},{18000,6000,0},10,52,0,0,70,.2f});
    ticks(1);assert(bullets.empty());
    bullets.push_back({{4500,200,4500},{18000,0,0},10,150,0,0,70,0,false,true,115,140});
    ticks(1);assert(bullets.empty()&&!blasts.empty());
    assert(std::abs(blasts.back().p.x-4570)<.1f);

    stage();debug_menu::open=true;debug_menu::selection=6;debug_menu::handleKey(VK_RETURN);assert(money==100000);
    police::setWantedLevel(4);debug_menu::selection=7;debug_menu::handleKey(VK_RETURN);assert(police::wantedLevel()==0);
    equip("grenade");ammo[weapon]=0;magazine[weapon]=0;debug_menu::selection=8;debug_menu::handleKey(VK_RETURN);
    assert(ammo[weapon]==1000&&magazine[weapon]==6&&ammo[0]==-1);
    puts("Ordnance, stealth, hideout, range, debug and underwater scenarios passed.");
}
