#include "../src/game.h"
#include "../src/game_internal.h"
#include "../src/jolt_world.h"
#include "../src/grapple.h"
#include "../src/physics.h"
#include "../src/weapons.h"
#include "../src/savegame.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#include "../src/dx11_assets.h"
#include "../src/ui.h"
#include "../src/ai.h"
#include "../src/debug_menu.h"
#include "../src/ordnance.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <cstring>
namespace {
using namespace game;
void stage(){
    reset();buildings.clear();trees.clear();peds.clear();props.clear();vehicles.clear();pickups.clear();
    wildlife::animals.clear();birds::flock.clear();
    std::fill(std::begin(keys),std::end(keys),false);leftMouse=rightMouse=false;
    player=previousPlayer={4500,4500};playerY=0;cameraYaw=cameraPitch=0;
    grounded=true;health=PLAYER_MAX_HEALTH;
}
void ticks(int n,float dt=1.0f/60){for(int i=0;i<n;++i)update(dt);}
std::string savePath(){char p[MAX_PATH]{};GetModuleFileNameA(nullptr,p,MAX_PATH);
    std::string path(p);return path.substr(0,path.find_last_of("\\/")+1)+"savegame.ini";}
}
void equipmentScenarios(){
    using namespace game;
    reset();int helicopterCount=0;
    for(const auto& v:vehicles)if(v.kind==Kind::Helicopter){
        ++helicopterCount;assert(!solid(v.p,35));
    }
    assert(helicopterCount==3);
    // Real entry, lift, hover, yaw, forward travel, descent and airborne exit.
    for(float dt:{1.0f/60,1.0f/30}){
        stage();Vehicle v{};v.p=player;v.kind=Kind::Helicopter;v.id="test-heli";v.c=rgb(220,180,70);
        vehicles.push_back(v);jolt_world::reset();player=previousPlayer=v.p+Vec2{45,0};
        jolt_world::teleportCharacter(player,0);enterExit();ticks(int(1/dt),dt);
        assert(occupied==0);
        keys[VK_SPACE]=true;ticks(int(3/dt),dt);keys[VK_SPACE]=false;
        assert(vehicles[0].rideHeight>170&&vehicles[0].rotorSpeed>.95f);
        assert(vehicles[0].rotorAngle>0&&vehicles[0].rotorAngle<PI*2);
        ticks(int(1.5f/dt),dt);float height=vehicles[0].rideHeight;
        ticks(int(1/dt),dt);assert(std::abs(vehicles[0].rideHeight-height)<10);
        float oldAngle=vehicles[0].angle;keys[ui::bindings[int(ui::Action::Right)]]=true;
        ticks(int(.8f/dt),dt);keys[ui::bindings[int(ui::Action::Right)]]=false;
        assert(std::abs(vehicles[0].angle-oldAngle)>.4f);
        Vec2 before=vehicles[0].p;keys[ui::bindings[int(ui::Action::Forward)]]=true;
        ticks(int(1.5f/dt),dt);keys[ui::bindings[int(ui::Action::Forward)]]=false;
        assert(len(vehicles[0].p-before)>180);
        keys[VK_CONTROL]=true;ticks(int(1/dt),dt);keys[VK_CONTROL]=false;
        assert(vehicles[0].rideHeight<height-50);
        float exitHeight=vehicles[0].rideHeight;enterExit();
        assert(occupied<0&&playerY>10&&std::abs(playerY-exitHeight)<1);
        ticks(int(2/dt),dt);assert(playerY<exitHeight&&vehicles[0].rotorSpeed<.05f);
        assert(!vehicles[0].exploded);
        occupied=0;player=vehicles[0].p;keys[VK_CONTROL]=true;ticks(int(4/dt),dt);
        keys[VK_CONTROL]=false;ticks(int(1/dt),dt);
        assert(std::abs(vehicles[0].rideHeight-game::groundHeight(vehicles[0].p))<2&&std::abs(vehicles[0].verticalSpeed)<2);
        assert(!vehicles[0].exploded);
    }
    // A fast airborne hull must collide with a tall wall, not tunnel through it.
    stage();Vehicle v{};v.p=player;v.kind=Kind::Helicopter;v.id="wall-heli";v.c=rgb(220,180,70);
    vehicles.push_back(v);buildings.push_back({4700,4420,50,160,650,rgb(120,120,120),"flight-wall"});
    jolt_world::reset();occupied=0;keys[VK_SPACE]=true;ticks(160);keys[VK_SPACE]=false;
    keys[ui::bindings[int(ui::Action::Forward)]]=true;ticks(180);
    assert(vehicles[0].p.x<4700&&vehicles[0].damage>0);
    // Flying over a pedestrian must not trigger a planar car-impact shortcut.
    Ped pedestrian{};pedestrian.p=vehicles[0].p;pedestrian.health=100;
    Vehicle overhead=vehicles[0];overhead.rideHeight=200;overhead.speed=200;
    assert(!ai::vehicleImpact(pedestrian,overhead)&&pedestrian.health==100);
    // Falling over water enters swimming only when the capsule reaches the surface.
    stage();jolt_world::reset();player=previousPlayer={600,2000};playerY=200;
    jolt_world::teleportCharacter(player,playerY);jolt_world::moveCharacter({},false,1.0f/60);
    assert(!swimming&&playerY>150&&playerVerticalSpeed<0);
    // Sustained minigun fire uses 25 rounds per second at a fixed 60 Hz tick.
    stage();jolt_world::reset();weapon=weapons::indexOf("minigun");unlocked[weapon]=true;
    magazine[weapon]=300;ammo[weapon]=600;leftMouse=true;ticks(120);leftMouse=false;
    int rounds=300-magazine[weapon];std::printf("Minigun rounds in 2s: %d\n",rounds);
    assert(rounds>=49&&rounds<=51);assert(casings.size()==size_t(rounds));
    magazine[weapon]=0;startReload();ticks(210);assert(magazine[weapon]==300&&ammo[weapon]==300);
    // F4 unlimited ammo fires an empty weapon, preserves inventory, and restores depletion when disabled.
    stage();jolt_world::reset();weapon=weapons::indexOf("minigun");unlocked[weapon]=true;
    magazine[weapon]=ammo[weapon]=0;debug_menu::toggle();debug_menu::selection=5;
    debug_menu::handleKey(VK_RETURN);assert(debug_menu::infiniteAmmo);
    debug_menu::toggle();leftMouse=true;ticks(120);leftMouse=false;
    assert(casings.size()>=49&&magazine[weapon]==0&&ammo[weapon]==0&&reloadRemaining==0);
    startReload();assert(reloadRemaining==0);
    for(int gun=0;gun<weapons::count();++gun){
        if(weapons::stats(gun).melee||weapons::stats(gun).grapple)continue;
        weapon=gun;unlocked[gun]=true;magazine[gun]=ammo[gun]=0;fireCooldown=0;
        auto before=bullets.size(),devices=ordnance::devices.size();shoot();
        auto payload=weapons::stats(gun).payload;
        if(payload==weapons::Payload::None)assert(bullets.size()>before);
        else if(payload==weapons::Payload::Remote)assert(ordnance::c4Count()==0);
        else if(payload==weapons::Payload::TimedBomb){assert(ordnance::timerOpen());ordnance::timerKey(VK_ESCAPE);}
        else assert(ordnance::devices.size()>devices);
        assert(magazine[gun]==0&&ammo[gun]==0);
    }
    weapon=weapons::indexOf("minigun");
    debug_menu::toggle();debug_menu::handleKey(VK_RETURN);assert(!debug_menu::infiniteAmmo);
    debug_menu::toggle();ticks(30);auto spent=casings.size();shoot();assert(casings.size()==spent);
    magazine[weapon]=3;ammo[weapon]=5;fireCooldown=0;shoot();assert(magazine[weapon]==2);
    debug_menu::reset();assert(!debug_menu::infiniteAmmo);
    // Shovel shares actual melee damage and a visible swing pose, with no ammunition cost.
    stage();Ped p{};p.id="shovel-target";p.p=player+Vec2{40,0};p.health=100;
    peds.push_back(p);jolt_world::reset();weapon=weapons::indexOf("shovel");unlocked[weapon]=true;
    shoot();assert(peds[0].health==35&&meleeVisualTime>0&&meleeVisualAction==11);
    dx11::loadMeshes(L"assets/models/baked");
    std::vector<dx11::Vertex> start[dx11::MATERIAL_GROUPS],middle[dx11::MATERIAL_GROUPS];
    std::vector<dx11::ModelInstance> instances;
    dx11::buildScene(start,instances);meleeVisualTime=.2f;dx11::buildScene(middle,instances);
    assert(!start[5].empty()&&start[5].size()==middle[5].size());
    bool changed=false;for(size_t i=0;i<start[5].size();++i)
        if(std::abs(start[5][i].x-middle[5][i].x)>.02f||std::abs(start[5][i].y-middle[5][i].y)>.02f||std::abs(start[5][i].z-middle[5][i].z)>.02f)changed=true;
    assert(changed);assert(dx11::mesh("vehicles/helicopter"));assert(dx11::mesh("vehicles/helicopter-rotor"));
    // Every world pickup uses its own detailed texture; inactive pickups vanish.
    stage();int savedGrassDistance=ui::grassDistance;ui::grassDistance=0;
    for(int i=0;i<weapons::count();++i)
        pickups.push_back({player+Vec2{80+float(i)*15,0},i,true,0,"icon-test-"+std::to_string(i)});
    dx11::buildScene(start,instances,player.x,80,player.z-80);
    for(int i=0;i<weapons::count();++i){
        const auto* icon=dx11::mesh("icons/"+weapons::stats(i).id);assert(icon);
        int matches=0;for(const auto& instance:instances)if(instance.source==icon){
            ++matches;assert(instance.material==13&&instance.scaleX<=32.01f);
            assert(instance.scaleY*icon->maxY<=26.01f&&instance.sinPitch>0);
        }
        assert(matches==1);pickups[i].available=false;
    }
    dx11::buildScene(start,instances);for(const auto& instance:instances)assert(!instance.source->unlit);
    ui::grassDistance=savedGrassDistance;
    // Grapple hook launches toward reticle-picked static cover, reels in, and releases momentum.
    stage();buildings.push_back({4700,4400,40,200,500,rgb(140,140,140),"grapple-wall"});
    jolt_world::reset();weapon=weapons::indexOf("grapple-hook");unlocked[weapon]=true;
    cameraPitch=.45f;rightMouse=leftMouse=true;shoot();assert(grapple::active());
    Vec3 anchor=grapple::anchor();assert(anchor.x>=4699&&anchor.x<4701&&anchor.y>70);
    ticks(24);assert(grapple::active()&&player.x>4510&&playerY>5);
    keys[ui::bindings[int(ui::Action::Right)]]=true;ticks(10);
    keys[ui::bindings[int(ui::Action::Right)]]=false;
    float lateral=player.z;Vec2 releaseVelocity=playerVelocity;leftMouse=false;update(1.0f/60);
    assert(!grapple::active()&&grapple::cooldown()>0&&len(releaseVelocity)>100&&player.z>=lateral);
    leftMouse=true;assert(!grapple::fire());leftMouse=false;ticks(40);
    assert(grapple::cooldown()==0);
    // Empty sky and ground miss; a dynamic obstacle in front of a wall rejects attachment.
    stage();jolt_world::reset();weapon=weapons::indexOf("grapple-hook");cameraPitch=.6f;
    assert(!grapple::fire());grapple::reset();cameraPitch=-.7f;assert(!grapple::fire());
    stage();buildings.push_back({4700,4400,40,200,500,rgb(140,140,140),"grapple-wall"});
    Prop cover{};cover.p={4610,4500};cover.y=30;props.push_back(cover);jolt_world::reset();
    Vec3 point;assert(!jolt_world::staticAnchor({4500,40,4500},{1,0,0},400,point));
    // Saving omits every dead actor, including carried/pinned corpses, but keeps money/weapons.
    reset();const auto pedId=peds[0].id,animalId=wildlife::animals[0].id,birdId=birds::flock[0].id;
    peds[0].alive=false;peds[0].pinned=true;peds[0].carried=true;peds[0].looted=true;
    wildlife::animals[0].health=0;birds::flock[0].health=0;
    money=4321;weapon=weapons::indexOf("minigun");unlocked[weapon]=true;magazine[weapon]=137;
    assert(savegame::save());
    std::ifstream file(savePath());std::string saved((std::istreambuf_iterator<char>(file)),{});file.close();
    assert(saved.find("[Ped."+pedId+"]")==std::string::npos);
    assert(saved.find("[Animal."+animalId+"]")==std::string::npos);
    assert(saved.find("[Bird."+birdId+"]")==std::string::npos);
    // Emulate a legacy save's death records: they must also be discarded.
    const auto path=savePath();
    WritePrivateProfileStringA(("Ped."+pedId).c_str(),"Dead","1",path.c_str());
    WritePrivateProfileStringA(("Animal."+animalId).c_str(),"Health","0",path.c_str());
    WritePrivateProfileStringA(("Bird."+birdId).c_str(),"Health","0",path.c_str());
    assert(savegame::load());
    assert(peds[0].alive&&wildlife::animals[0].health>0&&birds::flock[0].health>0);
    assert(!carryingBody()&&corpseSnapshots.empty()&&ragdollParts.empty());
    assert(money==4321&&magazine[weapon]==137&&!grapple::active());
    std::puts("Equipment scenarios passed: flight, collisions, weapons, animation, grapple, transient corpses.");
}
