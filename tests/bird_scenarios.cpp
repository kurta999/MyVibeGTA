#include "../src/birds.h"
#include "../src/game.h"
#include "../src/regions.h"
#include "../src/jolt_world.h"
#include "../src/savegame.h"
#include "../src/camera.h"
#include "../src/weapons.h"
#include "../src/wildlife.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <set>
#include <cstdio>

void birdScenarios(){
    using namespace game;
    std::set<int> kinds;std::set<std::string> ids;
    assert(birds::flock.size()>=130&&birds::flock.size()<=150);
    for(const auto& b:birds::flock){kinds.insert(b.species);assert(ids.insert(b.id).second);
        assert(birds::clearFlight(b.p,b.p,birds::radius(b)));}
    assert(kinds.size()==5);
    auto initial=birds::flock;birds::reset();assert(birds::flock.size()==initial.size());
    for(std::size_t i=0;i<initial.size();++i)assert(len(initial[i].p-birds::flock[i].p)<0.001f);
    // Real-world local flight continuously respects the whole wingspan envelope.
    player={550,600};int moving=0;
    for(int tick=0;tick<180;++tick){birds::update(1.0f/60);
        for(const auto& b:birds::flock)assert(birds::clearFlight(b.p,b.p,birds::radius(b)));}
    for(std::size_t i=0;i<initial.size();++i)if(len(initial[i].p-birds::flock[i].p)>20)++moving;
    assert(moving>=5);
    const auto tree=trees.front();
    float reach=tree.crownWidth*tree.scale*0.5f+30;
    float canopy=tree.height*tree.scale*0.7f;
    assert(!birds::clearFlight({tree.p.x-reach,canopy,tree.p.z},
        {tree.p.x+reach,canopy,tree.p.z},8));
    // A thin wall must block a long segment even if neither endpoint intersects it.
    buildings.clear();trees.clear();vehicles.clear();props.clear();peds.clear();bullets.clear();
    buildings.push_back({4550,4460,2,80,180,{1,1,1},"bird-wall"});
    assert(!birds::clearFlight({4500,100,4500},{4600,100,4500},16));
    assert(!birds::clearFlight({4500,190,4500},{4600,190,4500},16));
    assert(birds::clearFlight({4500,210,4500},{4600,210,4500},16));
    assert(!birds::clearFlight({4500,4,4500},{4600,4,4500},16));
    assert(!birds::clearFlight({20,100,4500},{-20,100,4500},16));
    birds::Bird b;b.id="test-bird";b.species=0;b.health=24;b.p={4500,100,4500};b.home={4680,100,4500};
    birds::flock={b};player={4500,4450};
    for(int tick=0;tick<360;++tick){Vec3 old=birds::flock[0].p;birds::update(1.0f/60);
        assert(birds::clearFlight(old,birds::flock[0].p,birds::radius(birds::flock[0])));}
    assert(len(birds::flock[0].p-b.p)>25);
    // Vehicle and prop volumes also block flight corridors.
    buildings.clear();Vehicle car{};car.p={4550,4500};car.kind=Kind::Car;vehicles.push_back(car);
    assert(!birds::clearFlight({4500,35,4500},{4600,35,4500},8));vehicles.clear();
    Prop prop{};prop.p={4550,4500};props.push_back(prop);
    assert(!birds::clearFlight({4500,20,4500},{4600,20,4500},8));props.clear();
    // Hit a small airborne target with a bullet that crosses it in one fixed tick.
    b.species=2;b.health=12;b.p={4580,120,4500};b.home=b.p;birds::flock={b};
    player={4500,4500};previousPlayer=player;playerY=0;occupied=-1;
    jolt_world::teleportCharacter(player,0);
    Bullet shot{};shot.p={4540,120,4500};shot.v={12000,0,0};shot.life=1;shot.range=500;shot.damage=50;
    bullets.push_back(shot);game::update(1.0f/60);
    assert(birds::flock[0].health==0&&bullets.empty()&&!birds::flock[0].settled);
    float height=birds::flock[0].p.y;
    for(int tick=0;tick<180;++tick)birds::update(1.0f/60);
    assert(birds::flock[0].settled&&birds::flock[0].p.y<height&&birds::flock[0].p.y==2);
    // Fire the real weapon at the reticle, including upper wing hits, moving
    // targets, steep upward aim, and both shoulder and first-person cameras.
    wildlife::animals.clear();rightMouse=true;weapon=weapons::indexOf("pistol");
    for(auto mode:{CameraMode::ThirdNear,CameraMode::FirstClose})
        for(float pitch:{0.5f,1.25f})for(int species=0;species<5;++species){
            cameraMode=mode;cameraYaw=0;cameraPitch=pitch;fireCooldown=0;
            recoil=0;reloadRemaining=0;magazine[weapon]=12;bullets.clear();
            auto pose=camera::compute(player,playerY,true,-1);
            Vec3 aim=pose.eye+norm(pose.target-pose.eye)*220;
            b.species=species;b.health=birds::species()[species].health;
            b.p=aim-Vec3{0,birds::species()[species].span*0.25f,0};
            b.home=b.p;b.velocity={0,0,60};b.planTime=10;b.angle=0;
            birds::flock={b};
            assert(len(camera::traceReticle(pose,650)-pose.eye)<230);
            shoot();assert(!bullets.empty());
            for(int tick=0;tick<16&&birds::flock[0].health>0;++tick)update(1.0f/60);
            assert(birds::flock[0].health==0);
            assert(cameraPitch>pitch); // Recoil must not snap high aim downward.
        }
    // A solid wall still wins before the airborne target.
    cameraMode=CameraMode::FirstClose;cameraYaw=0;cameraPitch=0.5f;fireCooldown=0;
    magazine[weapon]=12;bullets.clear();
    auto pose=camera::compute(player,playerY,true,-1);
    b.p=pose.eye+norm(pose.target-pose.eye)*220;b.home=b.p;b.velocity={};
    b.health=24;b.species=0;b.planTime=10;birds::flock={b};
    buildings.push_back({4550,4400,10,200,400,{1,1,1},"bird-shot-cover"});
    shoot();for(int tick=0;tick<16;++tick)update(1.0f/60);
    assert(birds::flock[0].health==24);buildings.clear();
    // A shot-down bird lands on a roof, never through it, even at a larger dt.
    buildings.push_back({4530,4430,140,140,50,{1,1,1},"bird-roof"});
    b.health=0;b.p={4580,120,4500};b.velocity={};b.settled=false;birds::flock={b};
    for(int tick=0;tick<30;++tick)birds::update(0.1f);
    assert(birds::flock[0].settled&&birds::flock[0].p.y==52);
    // Stable IDs retain kills after a real save/reset/load, including mid-fall.
    game::reset();auto id=birds::flock[0].id;birds::hurt(0,1000);
    assert(savegame::save()&&savegame::load());
    assert(birds::flock[0].id==id&&birds::flock[0].health==0);
    for(int tick=0;tick<300;++tick)birds::update(1.0f/60);
    assert(birds::flock[0].settled);
    std::puts("Bird scenarios passed: five types, flight clearance, camera-fired moving/wing/overhead hits, shot cover, falling, persistence.");
}
