#include "../game.h"
#include "../physics.h"
#include "../props.h"
#include "../savegame.h"
#include "../weapons.h"
#include "../camera.h"
#include "../ui.h"
#include "../ai.h"
#ifdef MINI_CITY_JOLT
#include "../jolt_world.h"
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

int main(){
    std::srand(1);
    char currentDirectory[MAX_PATH]{};GetCurrentDirectoryA(MAX_PATH,currentDirectory);
    std::string weaponConfig=std::string(currentDirectory)+"\\weapons-test.ini";
    {std::ofstream data(weaponConfig);data<<"[PISTOL]\nDamage=77\n";}
    assert(weapons::load(weaponConfig.c_str()));
    assert(weapons::stats(0).damage==77);
    std::remove(weaponConfig.c_str());
    assert(!weapons::load(weaponConfig.c_str()));
    assert(weapons::stats(0).damage==52);
    game::reset();
    assert(game::missions.size()==6);
    game::player=game::missions[1].start;
    game::startMission();
    assert(game::activeMission==-1&&game::nextMission()==0);
    game::reset();
    assert(game::magazine[0]==weapons::stats(0).magazine);
    assert(physics::tuning(game::Kind::SportCar).maxSpeed>physics::tuning(game::Kind::Boat).maxSpeed);
    assert(!game::props.empty());

    game::player=game::missions[0].start;
    game::startMission();
    assert(game::activeMission==0&&game::missionStep==0);
    int car=int(game::vehicles.size())-6;
    game::occupied=car;
    for(size_t step=0;step<game::missions[0].goals.size();++step){
        game::vehicles[car].p=game::missions[0].goals[step];
        game::player=game::vehicles[car].p;
        game::update(1.0f/60.0f);
        if(step+1<game::missions[0].goals.size())assert(game::missionStep==int(step+1));
    }
    assert(game::activeMission==-1&&game::missionDone[0]);
    assert(game::money==game::missions[0].reward);
    game::occupied=-1;
    game::player=game::missions[1].start;
    game::startMission();
    assert(game::activeMission==1&&game::nextMission()==1);
    game::reset();
    game::missionDone.fill(true);game::missionDone[5]=false;
    game::player=game::missions[5].start;game::startMission();
    assert(game::activeMission==5&&game::missionStep==0);
    car=int(game::vehicles.size())-6;game::occupied=car;
    game::vehicles[car].p=game::missions[5].goals[0];game::player=game::vehicles[car].p;
    game::update(1.0f/60.0f);
    assert(game::missionStep==1);
    game::occupied=-1;
    auto beachTarget=game::missions[5].goals[1];
    game::bullets.push_back({{beachTarget.x,20,beachTarget.z-20},{0,0,1200},1,12,0});
    game::update(1.0f/60.0f);
    assert(game::missionStep==2);
    int boat=int(game::vehicles.size())-1;game::occupied=boat;
    game::vehicles[boat].p=game::missions[5].goals[2];game::player=game::vehicles[boat].p;
    game::update(1.0f/60.0f);
    assert(game::activeMission==-1&&game::missionDone[5]);
    game::occupied=-1;game::reset();

    game::buildings.clear();game::peds.clear();game::props.clear();
    game::cameraYaw=0;game::cameraPitch=0;
    auto pose=camera::compute(game::player,0,true,-1);
    game::Vec3 ray=game::norm(pose.target-pose.eye);
    game::Vec3 pedPoint=pose.eye+ray*200;
    game::Ped target{};target.p={pedPoint.x,pedPoint.z};target.alive=true;
    game::peds.push_back(target);
    game::Vec3 hit=camera::traceReticle(pose,650);
    assert(game::len(hit-pose.eye)<200&&hit.y>2&&hit.y<37);
    game::Vec3 wallPoint=pose.eye+ray*100;
    game::buildings.push_back({wallPoint.x-5,wallPoint.z-5,10,10,80,{1,1,1}});
    hit=camera::traceReticle(pose,650);
    assert(game::len(hit-pose.eye)<150);
    game::buildings={{-5,-5,10,10,80,{1,1,1}}};game::peds.clear();
    hit=camera::traceReticle({{0,10,0},{100,10,0}},100);
    assert(hit.x>4.9f&&hit.x<5.1f);
    game::peds.clear();game::buildings.clear();
    game::Ped armed{};armed.p={game::player.x+50,game::player.z};armed.armed=true;
    game::Ped fleeing{};fleeing.p={game::player.x+60,game::player.z};fleeing.style=1;
    game::Ped investigating{};investigating.p={game::player.x+70,game::player.z};
    game::peds={armed,fleeing,investigating};
    ai::notifyGunshot(game::player);
    assert(game::peds[0].state==game::PedState::Attack);
    assert(game::peds[1].state==game::PedState::Flee);
    assert(game::peds[2].state==game::PedState::Investigate);
    ai::reactToHit(game::peds[2],game::player);
    assert(game::peds[2].state==game::PedState::Flee);
    game::reset();
    game::buildings.clear();game::peds.clear();game::props.clear();
    game::bullets.push_back({{game::player.x-20,18,game::player.z},{1200,0,0},1,12,0,
        0,650,0,true});
    float healthBefore=game::health;
    game::update(1.0f/60.0f);
    assert(game::health<healthBefore);
    game::reset();

    game::weapon=1;game::unlocked[1]=true;game::ammo[1]=50;game::magazine[1]=0;
    game::startReload();
    assert(game::reloadRemaining>0);
    for(int i=0;i<120;++i)game::update(1.0f/60.0f);
    assert(game::magazine[1]==weapons::stats(1).magazine);
    assert(game::ammo[1]==20);

    auto& crate=game::props.front();
    int oldHealth=crate.health;
    assert(props::hit({crate.p.x,10,crate.p.z},{200,0,0},25));
    assert(crate.health==oldHealth-25);
#ifdef MINI_CITY_JOLT
    game::Vec2 crateStart=game::props.front().p;
    jolt_world::impulse(0,{120,0,0});
    for(int tick=0;tick<30;++tick)props::update(1.0f/60.0f);
    assert(game::len(game::props.front().p-crateStart)>1.0f);
    game::Ped fallen{};fallen.p={240,game::BEACH_START+120};fallen.style=2;
    jolt_world::spawnRagdoll(fallen,{400,0,100});
    props::update(1.0f/60.0f);
    assert(game::ragdollParts.size()==6);
    for(int tick=0;tick<380;++tick)props::update(1.0f/60.0f);
    assert(game::ragdollParts.empty());
#endif

    game::money=1234;game::health=68;game::weapon=1;game::missionDone[5]=true;
    assert(savegame::save());
    game::money=0;game::health=100;game::weapon=0;game::missionDone[5]=false;
    assert(savegame::load());
    assert(game::money==1234&&game::health==68&&game::weapon==1);
    assert(game::missionDone[5]);
    ui::shadowQuality=2;ui::save();ui::shadowQuality=0;ui::load();
    assert(ui::shadowQuality==2);
    std::puts("simulation smoke passed");
}
