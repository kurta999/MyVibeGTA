#include "../src/game.h"
#include "../src/game_internal.h"
#include "../src/wildlife.h"
#include "../src/regions.h"
#include "../src/savegame.h"
#include "../src/weapons.h"
#include "../src/jolt_world.h"
#include "../src/dx11_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <set>
#include <cstdio>

namespace {
using namespace game;
using wildlife::State;
wildlife::Animal animal(int species,Vec2 position){
    wildlife::Animal a;a.species=species;a.id="test-"+std::to_string(species);
    a.p=a.home=a.target=position;a.health=wildlife::species()[species].health;
    a.timer=10;return a;
}
void tick(float seconds){for(int i=0;i<int(seconds*60);++i){worldTime+=1.0f/60;wildlife::update(1.0f/60);}}
}
void wildlifeScenarios(){
    using namespace game;
    std::set<int> species;std::set<std::string> ids;
    assert(wildlife::animals.size()>=120&&wildlife::animals.size()<=160);
    for(const auto& a:wildlife::animals){
        species.insert(a.species);assert(ids.insert(a.id).second);
        assert(regions::biomeAt(a.p)==regions::Biome::Countryside);
        assert(wildlife::walkable(a.p,wildlife::radius(a)));
    }
    assert(species.size()==15);
    const auto population=wildlife::animals;
    wildlife::reset();assert(wildlife::animals.size()==population.size());
    for(std::size_t i=0;i<population.size();++i){
        assert(wildlife::animals[i].id==population[i].id);
        assert(len(wildlife::animals[i].p-population[i].p)<0.001f);
    }
    // A controlled open forest clearing keeps combat tests independent of trees.
    buildings.clear();trees.clear();vehicles.clear();props.clear();peds.clear();bullets.clear();
    player={4500,4500};previousPlayer=player;playerY=0;health=PLAYER_MAX_HEALTH;
    occupied=-1;cameraYaw=0;keys[VK_SPACE]=false;rightMouse=false;
    assert(wildlife::walkable(player,40));
    wildlife::animals={animal(10,{4550,4500})};
    auto& timid=wildlife::animals[0];
    wildlife::hurt(0,10,player,true);assert(timid.state==State::Flee);
    float gap=len(timid.p-player);tick(1);assert(len(timid.p-player)>gap+20);
    assert(timid.health==65);
    wildlife::animals={animal(0,{4530,4500})};
    wildlife::hurt(0,1,player,true);assert(wildlife::animals[0].state==State::Attack);
    invulnerable=0;tick(0.1f);assert(health<PLAYER_MAX_HEALTH);
    float after=health;tick(0.4f);assert(health==after); // attack cooldown
    // Predation selects a smaller species and actual bites lower its health.
    wildlife::animals={animal(0,{4550,4500}),animal(14,{4565,4500})};
    wildlife::animals[0].timer=0;tick(0.1f);
    assert(wildlife::animals[1].health==0);
    assert(wildlife::animals[1].state==State::Dead);
    // An actual autonomous same-species decision produces mutual play.
    wildlife::animals={animal(6,{4550,4500}),animal(6,{4580,4500})};
    bool played=false;
    for(int i=0;i<600&&!played;++i){worldTime+=1.0f/60;wildlife::update(1.0f/60);
        played=wildlife::animals[0].state==State::Play;}
    assert(played&&wildlife::animals[1].state==State::Play);
    Vec2 before=wildlife::animals[0].p;tick(1);assert(len(before-wildlife::animals[0].p)>5);
    assert(wildlife::animals[0].health==65&&wildlife::animals[1].health==65);
    // The live game projectile loop must sweep through a small animal in one tick.
    wildlife::animals={animal(14,{4580,4500})};
    jolt_world::teleportCharacter(player,0);
    Bullet bullet{};bullet.p={4540,2,4500};bullet.v={12000,0,0};
    bullet.life=1;bullet.damage=50;bullet.range=500;bullets.push_back(bullet);
    update(1.0f/60);assert(wildlife::animals[0].health==0);assert(bullets.empty());
    // Melee uses the same input-facing cone and damage integration as pedestrians.
    wildlife::animals={animal(4,player+Vec2{25,0})};
    fireCooldown=0;keys[VK_SPACE]=true;shoot();keys[VK_SPACE]=false;
    assert(wildlife::animals[0].health<95);
    wildlife::hurt(0,1000,player,true);
    money=0;assert(wildlife::nearbyCorpse(true)==0);
    interact();assert(money==30&&wildlife::animals[0].looted);
    wildlife::loot(0);assert(money==30); // loot exactly once
    carryDrop();assert(carryingBody()&&wildlife::animals[0].carried);
    player.x+=30;tick(0.1f);assert(len(wildlife::animals[0].p-player)<25);
    int count=int(bullets.size());fireCooldown=0;shoot();assert(int(bullets.size())==count);
    carryDrop();assert(!carryingBody()&&!wildlife::animals[0].carried);
    assert(wildlife::walkable(wildlife::animals[0].p,wildlife::radius(wildlife::animals[0])));
    // Obstacle steering stays outside an expanded building and avoids water/roads.
    wildlife::animals={animal(3,{4550,4500})};
    buildings.push_back({4565,4470,25,60,40,{1,1,1},"wildlife-wall"});
    auto& walker=wildlife::animals[0];walker.target={4650,4500};walker.state=State::Wander;walker.timer=10;
    for(int i=0;i<180;++i){wildlife::update(1.0f/60);assert(!solid(walker.p,wildlife::radius(walker)));}
    bool testedWater=false,testedRoad=false;
    for(float x=100;x<regions::WIDTH&&(!testedWater||!testedRoad);x+=100)
        for(float z=100;z<regions::DEPTH&&(!testedWater||!testedRoad);z+=100){
            Vec2 p{x,z};
            if(regions::waterAt(p)){assert(!wildlife::walkable(p,10));testedWater=true;}
            if(regions::roadAt(p)){assert(!wildlife::walkable(p,10));testedRoad=true;}
        }
    assert(testedWater&&testedRoad);
    // Real E interaction, full simulation movement, rendered rider, obstacles,
    // dismount and death cleanup for both rideable species.
    buildings.clear();vehicles.clear();props.clear();peds.clear();trees.clear();
    dx11::loadMeshes(L"assets/models/baked");
    for(int kind:{0,1}){
        wildlife::animals={animal(kind,{4500,4500})};
        player={4500,4465};previousPlayer=player;playerY=0;health=PLAYER_MAX_HEALTH;
        occupied=-1;enteringVehicle=-1;swimming=false;grounded=true;cameraYaw=0;
        jolt_world::reset();
        assert(interactionPrompt().find("RIDE")!=std::string::npos);
        enterExit();assert(wildlife::mountedIndex()==0);
        keys['W']=true;Vec2 start=player;
        for(int tick=0;tick<120;++tick)update(1.0f/60);
        keys['W']=false;
        assert(len(player-start)>50&&len(player-wildlife::animals[0].p)<0.01f);
        assert(health==PLAYER_MAX_HEALTH&&grounded&&!swimming);
        std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];
        std::vector<dx11::ModelInstance> instances;
        cameraMode=CameraMode::ThirdNear;dx11::buildScene(groups,instances);
        assert(groups[5].size()==dx11::skinMesh("characters/hoodie-man")->vertices.size());
        for(const auto& vertex:groups[5]){
            assert(std::isfinite(vertex.y)&&vertex.y>playerY-1);
            assert(len(Vec2{vertex.x,vertex.z}-player)<40);
        }
        buildings.push_back({player.x+70,player.z-100,25,200,100,{1,1,1},"ride-wall"});
        keys['W']=true;keys[VK_SHIFT]=true;
        for(int tick=0;tick<180;++tick)update(1.0f/60);
        keys['W']=false;keys[VK_SHIFT]=false;
        assert(player.x<buildings.back().x-wildlife::radius(wildlife::animals[0]));
        buildings.clear();enterExit();assert(!wildlife::riding()&&playerY==0);
        Vec2 onFoot=player;update(1.0f/60);assert(len(player-onFoot)<1);
        enterExit();assert(wildlife::riding());
        wildlife::hurt(0,10000,player+Vec2{100,0},false);
        assert(!wildlife::riding()&&wildlife::animals[0].state==State::Dead);
    }
    wildlife::animals={animal(2,{4500,4500})};player={4500,4480};playerY=0;
    assert(!wildlife::mount(0));
    // The old length-sized clearance circle locked both mounts between trunks.
    // These gaps fit the body's width and must allow full-simulation riding.
    for(int kind:{0,1}){
        wildlife::animals={animal(kind,{4500,4500})};
        float r=wildlife::radius(wildlife::animals[0]);trees.clear();
        for(float x:{4480.0f,4520.0f,4560.0f,4600.0f})for(float side:{-1.0f,1.0f}){
            Tree tree{};tree.p={x,4500+side*(r+8)};trees.push_back(tree);
        }
        player={4460,4500};previousPlayer=player;playerY=0;health=PLAYER_MAX_HEALTH;
        occupied=-1;enteringVehicle=-1;swimming=false;grounded=true;
        jolt_world::reset();assert(wildlife::mount(0));
        cameraYaw=0;Vec2 start=player;float phase=wildlife::animals[0].phase;
        keys['W']=true;
        for(int i=0;i<120;++i)update(1.0f/60);
        keys['W']=false;
        assert(player.x>start.x+wildlife::species()[kind].speed*1.8f);
        assert(wildlife::animals[0].phase>phase+5);
        assert(wildlife::bodyWalkable(wildlife::animals[0],player,0,true));
        // Reverse, sideways controls and sprint must also move the mount.
        keys['S']=true;cameraYaw=PI;
        for(int i=0;i<30;++i)update(1.0f/60);
        keys['S']=false;assert(player.x>start.x+wildlife::species()[kind].speed*2.2f);
        trees.clear();cameraYaw=0;Vec2 sideways=player;
        keys['D']=true;keys[VK_SHIFT]=true;
        for(int i=0;i<60;++i)update(1.0f/60);
        keys['D']=false;keys[VK_SHIFT]=false;
        assert(player.z>sideways.z+wildlife::species()[kind].speed*1.5f);
        assert(wildlife::dismount(true));
    }
    for(int kind:{0,1}){
        wildlife::animals={animal(kind,{4500,4500})};trees.clear();
        float r=wildlife::radius(wildlife::animals[0]);
        Tree trunk{};trunk.p={4500+r+8,4500};trees={trunk};
        player={4500,4500-r-18};previousPlayer=player;playerY=0;occupied=-1;
        health=PLAYER_MAX_HEALTH;swimming=false;jolt_world::reset();
        assert(wildlife::walkable(wildlife::animals[0].p,r,true));
        assert(!wildlife::bodyWalkable(wildlife::animals[0],wildlife::animals[0].p,0,true));
        assert(wildlife::mount(0));
        assert(wildlife::bodyWalkable(wildlife::animals[0],wildlife::animals[0].p,0,true));
        cameraYaw=PI;Vec2 start=player;keys['W']=true;
        for(int i=0;i<60;++i)update(1.0f/60);
        keys['W']=false;assert(len(player-start)>wildlife::species()[kind].speed*0.8f);
        assert(wildlife::dismount(true));
    }
    trees.clear();
    // Every species has a physical body, independent of its AI state.
    for(int kind=0;kind<int(wildlife::species().size());++kind){
        wildlife::animals={animal(kind,{4500,4500})};
        player={4400,4500};playerY=0;occupied=-1;health=PLAYER_MAX_HEALTH;
        jolt_world::reset();assert(jolt_world::activeAnimalColliderCount()==1);
        bool stopped=false,steppedOver=false;
        for(int i=0;i<180;++i){
            Vec2 before=player;jolt_world::moveCharacter({80,0},false,1.0f/60);
            if(len(player-before)<0.1f)stopped=true;
            if(player.x>4500-wildlife::species()[kind].length*0.4f&&
               player.x<4500+wildlife::species()[kind].length*0.4f){
                // Tiny animals may be stepped over; their bodies must still
                // lift the character instead of permitting penetration.
                assert(playerY>=wildlife::species()[kind].height-0.5f);
                steppedOver=true;
            }
        }
        assert(stopped||steppedOver);
        wildlife::hurt(0,10000,player,true);jolt_world::step(1.0f/60);
        assert(jolt_world::activeAnimalColliderCount()==0);
        for(int i=0;i<120;++i)jolt_world::moveCharacter({80,0},false,1.0f/60);
        assert(player.x>4510);
    }
    // Real stable population IDs survive reset/load; carry becomes a ground corpse.
    reset();auto& saved=wildlife::animals[0];
    const std::string savedId=saved.id;
    player=saved.p+Vec2{15,0};previousPlayer=player;
    wildlife::hurt(0,10000,player,true);wildlife::loot(0);carryDrop();
    assert(wildlife::carrying());int cash=money;assert(savegame::save());
    assert(savegame::load());assert(!carryingBody());
    const auto& loaded=wildlife::animals[0];
    assert(loaded.id==savedId&&loaded.health>0&&!loaded.looted&&!loaded.carried);
    assert(loaded.state!=State::Dead&&money==cash);
    reset();
    player=wildlife::animals[0].p+Vec2{0,30};playerY=0;swimming=false;
    enterExit();assert(wildlife::riding());
    assert(savegame::save()&&savegame::load());
    assert(!wildlife::riding()&&occupied<0&&playerY==0);
    // Sleeping populations do not drift while the player is elsewhere.
    player={300,250};Vec2 asleep=wildlife::animals.back().p;tick(5);
    assert(len(asleep-wildlife::animals.back().p)<0.001f);
    std::puts("Wildlife scenarios passed: 15 species, placement, flee, retaliation, predation, play, swept projectile, melee, loot, carry, obstacles, persistence, sleep.");
}
