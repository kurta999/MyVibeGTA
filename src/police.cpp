#include "police.h"
#include "data_file.h"
#include "game_internal.h"
#include "weapons.h"
#include "regions.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace police {
namespace {
struct Tier {int weapon=0,officers=0,health=100,armor=0;float spread=0.08f;};
struct Pending {float delay;int stars;};
std::array<Tier,4> tiers{};
std::vector<game::Vec2> spawnPoints;
std::vector<Pending> pending;
std::array<float,8> cooldowns{};
std::string error;
float reportDelay=1.2f,decaySeconds=60,spawnCooldown=2;
float witnessSight=180,witnessHearing=260;
float quietTime=0,spawnTimer=0;
int wanted=0,serial=0;
game::Vec2 lastReport{};
int severity(Crime crime){
    switch(crime){
    case Crime::Murder:case Crime::Arson:case Crime::AttackOfficer:return 2;
    default:return 1;
    }
}
bool witnessed(game::Vec2 place,bool silent,bool directWitness){
    if(directWitness)return true;
    for(const auto& ped:game::peds)if(ped.alive&&!ped.police){
        float distance=game::len(ped.p-place);
        if(distance<witnessSight&&game::clearLine(ped.p,place))return true;
        if(!silent&&distance<witnessHearing&&distance<90)return true;
    }
    return false;
}
game::Vec2 chooseSpawn(){
    game::Vec2 best{};float bestScore=-1;
    for(game::Vec2 point:spawnPoints){
        float distance=game::len(point-game::player);
        if(distance<130||distance>900||game::solid(point,12))continue;
        game::Vec2 direction=game::norm(point-game::player);
        float facing=direction.x*std::cos(game::cameraYaw)+
            direction.z*std::sin(game::cameraYaw);
        bool visible=facing>0.1f&&distance<450&&game::clearLine(game::player,point);
        float score=500-std::abs(distance-350)-(visible?500:0);
        if(score>bestScore){best=point;bestScore=score;}
    }
    if(bestScore>=0)return best;
    for(int ring=0;ring<3;++ring)for(int side=0;side<8;++side){
        float angle=(side+ring*0.31f)*game::PI*0.25f;
        float distance=340+ring*110.0f;
        game::Vec2 point=game::player+game::Vec2{
            std::cos(angle)*distance,std::sin(angle)*distance};
        if(point.x<20||point.x>regions::WIDTH-20||
           point.z<20||point.z>regions::DEPTH-20||game::solid(point,12))continue;
        game::Vec2 toward=game::norm(point-game::player);
        float facing=toward.x*std::cos(game::cameraYaw)+
            toward.z*std::sin(game::cameraYaw);
        if(facing>0.2f&&game::clearLine(game::player,point))continue;
        return point;
    }
    for(game::Vec2 point:spawnPoints)
        if(game::len(point-game::player)>85&&!game::solid(point,12))return point;
    return {};
}
void summon(const Tier& tier){
    game::Vec2 place=chooseSpawn();
    if(place.x==0&&place.z==0)return;
    game::Ped officer{};
    officer.p=place;officer.target=game::player;
    officer.speed=57;officer.angle=std::atan2(game::player.z-place.z,game::player.x-place.x);
    officer.shirt=game::rgb(37,62,96);officer.style=1;
    officer.health=tier.health;officer.armor=officer.maxArmor=tier.armor;
    officer.armed=true;officer.police=true;officer.hostile=true;
    officer.state=game::PedState::Attack;officer.alertTime=999;
    officer.lastKnown=lastReport;officer.sightMemory=8;
    officer.weaponIndex=tier.weapon;officer.accuracy=tier.spread;
    officer.cash=0;
    int reuse=-1,total=0;
    for(int index=0;index<int(game::peds.size());++index)if(game::peds[index].police){
        ++total;
        if(!game::peds[index].alive&&reuse<0)reuse=index;
    }
    if(total>=8&&reuse>=0){
        officer.id=game::peds[reuse].id;
#ifdef MINI_CITY_JOLT
        jolt_world::removeRagdoll(officer.id);
#endif
        game::peds[reuse]=officer;
    }else if(total<8){
        officer.id="police-"+std::to_string(serial++);
        game::peds.push_back(officer);
#ifdef MINI_CITY_JOLT
        jolt_world::addPed();
#endif
    }
}
}
bool load(const char* path){
    error.clear();
    data_file::Ini file;
    if(!file.load(path?path:data_file::resourcePath("police.ini"))||!file.version(1)){
        error=file.lastError();return false;
    }
    int count=0;
    if(!file.real("Response","ReportDelay",reportDelay,0,30)||
       !file.real("Response","DecaySeconds",decaySeconds,5,600)||
       !file.real("Response","SpawnCooldown",spawnCooldown,0.1f,30)||
       !file.real("Response","WitnessSight",witnessSight,10,1000)||
       !file.real("Response","WitnessHearing",witnessHearing,10,1000)||
       !file.integer("Response","SpawnCount",count,1,64)){
        error=file.lastError();return false;
    }
    std::vector<game::Vec2> spawns;
    for(int index=0;index<count;++index){
        std::string section="Spawn"+std::to_string(index);game::Vec2 place{};
        if(!file.real(section,"X",place.x,0,regions::WIDTH)||
           !file.real(section,"Z",place.z,0,regions::DEPTH)||
           regions::waterAt(place)){
            error=file.lastError();return false;
        }
        spawns.push_back(place);
    }
    std::array<Tier,4> parsed{};
    for(int index=0;index<4;++index){
        std::string section="Tier"+std::to_string(index+1),weaponId;
        Tier& tier=parsed[index];
        if(!file.string(section,"WeaponId",weaponId)||
           !file.integer(section,"Officers",tier.officers,1,8)||
           !file.integer(section,"Health",tier.health,1,500)||
           !file.integer(section,"Armor",tier.armor,0,200)||
           !file.real(section,"Spread",tier.spread,0,0.5f)){
            error=file.lastError();return false;
        }
        tier.weapon=weapons::indexOf(weaponId);
        if(tier.weapon<0){error="Unknown ["+section+"] WeaponId: "+weaponId;return false;}
        if(index>0&&tier.officers<parsed[index-1].officers){
            error="["+section+"] Officers must not decrease";return false;
        }
    }
    tiers=parsed;spawnPoints=std::move(spawns);return true;
}
const std::string& lastError(){return error;}
void reset(){wanted=0;serial=0;quietTime=0;spawnTimer=0;lastReport={};
    pending.clear();cooldowns.fill(0);}
void setWantedLevel(int level){wanted=std::clamp(level,0,4);quietTime=0;
    lastReport=game::player;pending.clear();}
int wantedLevel(){return wanted;}
bool report(Crime crime,game::Vec2 place,bool silent,bool directWitness){
    int index=int(crime);
    if(cooldowns[index]>0||!witnessed(place,silent,directWitness))return false;
    cooldowns[index]=0.7f;
    pending.push_back({reportDelay,severity(crime)});
    lastReport=place;
    quietTime=0;return true;
}
void update(float dt){
    dt=std::clamp(dt,0.0f,0.25f);
    spawnTimer=std::max(0.0f,spawnTimer-dt);
    for(float& cooldown:cooldowns)cooldown=std::max(0.0f,cooldown-dt);
    for(auto& item:pending)item.delay-=dt;
    for(const auto& item:pending)if(item.delay<=0){
        wanted=std::min(4,wanted+item.stars);quietTime=0;
        game::message="WANTED LEVEL "+std::to_string(wanted);
        game::messageTime=3;
    }
    pending.erase(std::remove_if(pending.begin(),pending.end(),
        [](const Pending& p){return p.delay<=0;}),pending.end());
    if(wanted>0&&pending.empty()){
        quietTime+=dt;
        if(quietTime>=decaySeconds){wanted=std::max(0,wanted-1);quietTime=0;}
    }
    if(wanted<=0){
        for(auto& ped:game::peds)if(ped.police&&ped.alive){
            ped.hostile=false;
            if(ped.state==game::PedState::Attack)ped.state=game::PedState::Wander;
        }
        return;
    }
    const Tier& tier=tiers[wanted-1];
    int active=0;
    for(auto& ped:game::peds)if(ped.police&&ped.alive){
        ++active;ped.weaponIndex=tier.weapon;ped.accuracy=tier.spread;
        if(!ped.hostile){ped.hostile=true;ped.state=game::PedState::Attack;
            ped.lastKnown=lastReport;ped.sightMemory=5;}
        ped.alertTime=999;
    }
    if(game::health>0&&active<tier.officers&&spawnTimer<=0){
        summon(tier);spawnTimer=spawnCooldown;
    }
}
}
