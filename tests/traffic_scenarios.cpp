#include "../src/game.h"
#include "../src/game_internal.h"
#include "../src/traffic.h"
#include "../src/ai.h"
#include "../src/jolt_world.h"
#include "../src/police.h"
#include "../src/fire.h"
#include "../src/content.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <set>

namespace {
using namespace game;
constexpr float dt=1.0f/60;
void fixture(bool driver=true){
    jolt_world::shutdown();
    vehicles.clear();peds.clear();buildings.clear();props.clear();
    bullets.clear();fire::reset();police::reset();
    occupied=-1;enteringVehicle=-1;health=PLAYER_MAX_HEALTH;playerY=0;
    player={400,400};playerVelocity={};
    Vehicle car{};car.kind=Kind::Car;car.p={450,273};car.angle=0;
    car.id=driver?"traffic-test":"parked-test";vehicles.push_back(car);
    traffic::reset();jolt_world::reset();
}
void ticks(int count){
    for(int n=0;n<count;++n){traffic::update(dt);ai::update(dt);jolt_world::step(dt);}
}
Ped victim(Vec2 p){Ped ped{};ped.id="victim";ped.p=p;ped.target=p;
    ped.speed=52;ped.health=100;return ped;}
}
void trafficScenarios(){
    using namespace game;
    std::setvbuf(stdout,nullptr,_IONBF,0);
    fixture();assert(traffic::roadNodeCount()>80&&vehicles[0].driver==0);
    Vec2 initial=vehicles[0].p;
    ticks(150);
    assert(len(vehicles[0].p-initial)>40);
    // A player on foot cuts in: brake to rest with a gap, hold, then resume.
    player=vehicles[0].p+forward(vehicles[0].angle)*95;
    jolt_world::teleportCharacter(player,0);
    ticks(180);
    std::printf("stop: speed %.2f gap %.2f\n",vehicles[0].speed,len(vehicles[0].p-player));
    assert(std::abs(vehicles[0].speed)<4&&len(vehicles[0].p-player)>25);
    Vec2 stopped=vehicles[0].p;ticks(120);
    assert(len(vehicles[0].p-stopped)<5);
    player={400,400};jolt_world::teleportCharacter(player,0);ticks(120);
    assert(len(vehicles[0].p-stopped)>20);

    // A stationary player vehicle and a queue of vehicles are also obstacles.
    fixture();Vehicle blocker{};blocker.kind=Kind::Car;blocker.p={620,273};
    vehicles.push_back(blocker);occupied=1;player=blocker.p;jolt_world::reset();
    ticks(300);
    assert(std::abs(vehicles[0].speed)<4&&len(vehicles[0].p-vehicles[1].p)>40);
    assert(!peds[0].hostile&&vehicles[0].damage==0);

    // Collision attribution comes from actual Jolt contacts, not mere proximity.
    fixture();Vehicle driven{};driven.kind=Kind::Car;driven.p={320,273};
    driven.angle=0;vehicles.push_back(driven);occupied=1;player=driven.p;
    jolt_world::reset();
    for(int n=0;n<240&&!peds[0].hostile;++n){
        jolt_world::driveVehicle(0,0,0,dt,true);
        jolt_world::driveVehicle(1,1,0,dt);jolt_world::step(dt);
    }
    assert(peds[0].hostile&&peds[0].grievance>0&&vehicles[0].damage>0);

    // A civilian bullet hitting a driver vehicle also enters the pursuit chain.
    fixture();player={370,273};
    bullets.push_back({{410,20,273},{3000,0,0},1,12,0,0,650,0,false});
    for(int n=0;n<10&&!peds[0].hostile;++n)game::update(dt);
    assert(peds[0].hostile&&vehicles[0].damage>0);

    // An angry driver deliberately closes on the player's car and the ram
    // causes real chassis damage instead of using the commuter stop gap.
    fixture();Vehicle targetCar{};targetCar.kind=Kind::Car;targetCar.p={670,273};
    vehicles.push_back(targetCar);occupied=1;player=targetCar.p;jolt_world::reset();
    damageVehicle(0,3,true);ticks(420);
    assert(vehicles[1].damage>0&&peds[0].grievance>0);

    // Routing continues through several junctions without teleporting.
    fixture();std::set<int> visited;
    Vec2 previous=vehicles[0].p;
    for(int n=0;n<2400;++n){
        player=vehicles[0].p+Vec2{140,140};
        jolt_world::teleportCharacter(player,0);ticks(1);
        visited.insert(vehicles[0].roadTo);
        assert(len(vehicles[0].p-previous)<8);previous=vehicles[0].p;
    }
    std::printf("junctions: %zu, position %.1f %.1f\n",visited.size(),previous.x,previous.z);
    assert(visited.size()>=4&&!vehicles[0].exploded);

    // Perpendicular commuters clear the intersection without colliding or
    // permanently waiting for one another.
    fixture();Vehicle crossing{};crossing.kind=Kind::Car;crossing.p={773,500};
    crossing.angle=-PI/2;crossing.id="traffic-crossing";vehicles.push_back(crossing);
    peds.clear();traffic::reset();jolt_world::reset();player={900,400};
    Vec2 startA=vehicles[0].p,startB=vehicles[1].p;ticks(900);
    for(int i=0;i<2;++i)std::printf("cross %d: %.1f %.1f speed %.1f wanted %.1f state %d road %d %d\n",i,vehicles[i].p.x,vehicles[i].p.z,vehicles[i].speed,vehicles[i].desiredSpeed,int(vehicles[i].trafficState),vehicles[i].roadFrom,vehicles[i].roadTo);
    assert(len(vehicles[0].p-startA)>300&&len(vehicles[1].p-startB)>300);
    assert(vehicles[0].damage<5&&vehicles[1].damage<5);

    // Damage only provokes the driver when explicitly attributed to the player.
    fixture();damageVehicle(0,8);assert(!peds[0].hostile);
    damageVehicle(0,8,true);
    assert(peds[0].grievance>0&&vehicles[0].trafficState==TrafficState::React);
    player={900,273};ticks(90);
    assert(peds[0].hostile&&vehicles[0].trafficState==TrafficState::Pursue);
    Vec2 lastKnown=peds[0].lastKnown;
    player={12000,12000};ticks(300);
    assert(len(peds[0].lastKnown-lastKnown)<1&&peds[0].grievance>0);
    ticks(500);
    std::printf("expired: hostile %d anger %.2f memory %.2f state %d alive %d knock %.2f\n",peds[0].hostile,peds[0].grievance,peds[0].pursuitMemory,int(peds[0].state),peds[0].alive,peds[0].knockedDown);
    assert(!peds[0].hostile&&peds[0].grievance==0);

    // Two victims compete for one parked car; only one can reserve and board.
    fixture(false);peds.push_back(victim({450,335}));peds.push_back(victim({470,335}));
    jolt_world::addPed();player={800,273};
    ai::reactToHit(peds[0],player);ai::reactToHit(peds[1],player);
    ai::update(dt);
    assert(peds[0].seekingVehicle==0&&peds[1].seekingVehicle==-1&&vehicles[0].reservedBy==0);
    ticks(180);
    assert(vehicles[0].driver==0&&peds[0].drivingVehicle==0&&vehicles[0].reservedBy==-1);
    Vec2 chaseStart=vehicles[0].p;ticks(120);
    assert(len(vehicles[0].p-chaseStart)>20);
    // A dead driver releases the car immediately; a wreck cannot be reserved.
    peds[0].alive=false;ai::update(dt);
    assert(vehicles[0].driver==-1&&peds[0].drivingVehicle==-1);
    vehicles[0].exploded=true;peds[1].carSearchCooldown=0;ai::update(dt);
    assert(peds[1].seekingVehicle==-1);

    // Recovery from a nonlethal vehicle hit triggers acquisition and pursuit.
    fixture(false);peds.push_back(victim({450,335}));jolt_world::addPed();
    Vehicle impact{};impact.kind=Kind::Car;impact.p=peds[0].p;
    impact.speed=45;impact.velocity={45,0};player={800,273};
    assert(ai::vehicleImpact(peds[0],impact)&&peds[0].alive&&peds[0].knockedDown>0);
    for(int n=0;n<360&&peds[0].drivingVehicle<0;++n)ticks(1);
    assert(peds[0].drivingVehicle==0);

    // If the player takes a reserved car before boarding completes, the victim
    // releases it and keeps the grievance, without creating a second occupant.
    fixture(false);peds.push_back(victim({450,315}));jolt_world::addPed();
    player={800,273};ai::reactToHit(peds[0],player);ai::update(dt);
    assert(vehicles[0].reservedBy==0);
    player={450,245};game::enterExit();
    assert(enteringVehicle==0&&peds[0].seekingVehicle==-1&&vehicles[0].driver==-1);

    // A noticed pickpocket attempt uses the same retaliation chain.
    fixture(false);peds.push_back(victim({800,273}));peds[0].cash=20;
    peds[0].angle=0;player={780,273};jolt_world::addPed();
    bool noticed=false;
    for(int n=0;n<100&&!noticed;++n){peds[0].cash=20;game::interact();noticed=peds[0].hostile;}
    assert(noticed&&peds[0].grievance>0);
    player={950,273};ai::update(dt);assert(peds[0].seekingVehicle==0);

    fixture();player=vehicles[0].p+Vec2{0,55};
    traffic::carjacked(0);
    assert(vehicles[0].driver==-1&&peds[0].drivingVehicle==-1&&peds[0].grievance>0);
    // Once close and stopped, a pursuer exits and uses existing melee combat.
    fixture();player=vehicles[0].p+Vec2{70,0};damageVehicle(0,2,true);ticks(120);
    assert(peds[0].drivingVehicle==-1&&peds[0].hostile);
    player=peds[0].p+Vec2{15,0};jolt_world::teleportCharacter(player,0);
    float before=health;ticks(90);assert(health<before);

    // Loading releases reservations, grievances and drivers of owned cars.
    fixture();vehicles[0].owned=true;traffic::afterLoad();
    assert(vehicles[0].driver==-1&&peds[0].drivingVehicle==-1&&peds[0].grievance==0);

    // Exercise the authored city, its buildings and the full population, not
    // only an empty test road. At least three local cars must keep circulating.
    game::reset();player={750,640};jolt_world::teleportCharacter(player,0);
    std::vector<Vec2> starts;for(const auto& car:vehicles)starts.push_back(car.p);
    for(int n=0;n<1800;++n)game::update(dt);
    int circulating=0;
    for(int i=0;i<int(vehicles.size());++i){
        if(vehicles[i].driver>=0&&len(starts[i]-Vec2{750,640})<850)
            std::printf("city %s moved %.1f at %.1f %.1f angle %.1f wanted %.1f state %d\n",vehicles[i].id.c_str(),len(vehicles[i].p-starts[i]),vehicles[i].p.x,vehicles[i].p.z,vehicles[i].angle,vehicles[i].desiredSpeed,int(vehicles[i].trafficState));
        if(vehicles[i].driver>=0&&len(starts[i]-Vec2{750,640})<850&&
           len(vehicles[i].p-starts[i])>200&&!vehicles[i].exploded)++circulating;
    }
    std::printf("populated city: %d circulating cars\n",circulating);
    assert(circulating>=3);
    jolt_world::shutdown();
    std::puts("traffic scenarios passed");
}
