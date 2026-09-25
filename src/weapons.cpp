#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "weapons.h"
#include "data_file.h"
#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>

namespace weapons {
namespace {
const std::array<Stats,5> defaults{{
    {"PISTOL",12,1.15f,0.35f,52,760,0.005f,1,12,650,0.35f,0.012f,0,"pistol",true},
    {"SMG",30,1.55f,0.09f,22,900,0.055f,1,15,610,0.45f,0.006f,120,"smg",true},
    {"SHOTGUN",8,1.9f,0.55f,24,690,0.11f,7,38,250,0.65f,0.055f,35,"shotgun",false},
    {"RIFLE",24,1.65f,0.14f,36,1100,0.023f,1,8,950,0.2f,0.012f,90,"rifle",false},
    {"SNIPER",5,2.4f,0.9f,100,1600,0.002f,1,2,1200,0.05f,0.08f,18,"sniper",false}
}};
std::vector<Stats> entries(defaults.begin(),defaults.end());
std::string error;
}

bool load(const char* file){
    entries.assign(defaults.begin(),defaults.end());error.clear();
    std::string path=file?file:data_file::resourcePath("weapons.ini");
    data_file::Ini data;
    if(!data.load(path)||!data.version(1)){error=data.lastError();return false;}
    int total=0;
    if(!data.integer("Catalog","Count",total,1,64)){error=data.lastError();return false;}
    std::vector<Stats> parsed;parsed.reserve(total);
    std::set<std::string> ids;
    for(int index=0;index<total;++index){
        std::string section;
        if(!data.string("Catalog","Entry"+std::to_string(index),section))break;
        Stats item{};
        item.name=section;
        int driveBy=0,silenced=0,dual=0;std::string projectileType,streamType;
        if(!data.string(section,"Id",item.id)||!data_file::validId(item.id)){
            error=data.lastError().empty()?"Invalid ["+section+"] Id":data.lastError();return false;
        }
        if(!ids.insert(item.id).second){error="Duplicate weapon Id: "+item.id;return false;}
        if(!data.integer(section,"Magazine",item.magazine,1,200)||
           !data.integer(section,"ReservePickup",item.reservePickup,0,9999)||
           !data.real(section,"ReloadSeconds",item.reloadSeconds,0.1f,10.0f)||
           !data.real(section,"FireInterval",item.secondsBetweenShots,0.03f,5.0f)||
           !data.integer(section,"Damage",item.damage,1,1000)||
           !data.real(section,"ProjectileSpeed",item.projectileSpeed,100.0f,10000.0f)||
           !data.real(section,"Spread",item.spread,0.0f,0.5f)||
           !data.integer(section,"Pellets",item.pellets,1,32)||
           !data.real(section,"Gravity",item.gravity,0.0f,1000.0f)||
           !data.real(section,"Range",item.range,20.0f,5000.0f)||
           !data.real(section,"Falloff",item.falloff,0.0f,1.0f)||
           !data.real(section,"RecoilKick",item.recoilKick,0.0f,0.5f)||
           !data.integer(section,"DriveByAllowed",driveBy,0,1)||
           !data.string(section,"ProjectileType",projectileType)||
           !data.string(section,"StreamType",streamType)||
           !data.integer(section,"Silenced",silenced,0,1)||
           !data.real(section,"ExplosionRadius",item.explosionRadius,0,500)||
           !data.integer(section,"ExplosionDamage",item.explosionDamage,0,2000))break;
        if(data.has(section,"DualWieldAllowed")&&
            !data.integer(section,"DualWieldAllowed",dual,0,1))break;
        if(projectileType!="bullet"&&projectileType!="rocket"&&
            projectileType!="melee"&&projectileType!="arrow"){
            error="Invalid ["+section+"] ProjectileType";return false;
        }
        item.rocket=projectileType=="rocket";
        item.melee=projectileType=="melee";
        item.arrow=projectileType=="arrow";
        item.silenced=silenced!=0;
        if(streamType=="none")item.streamType=0;
        else if(streamType=="flame")item.streamType=1;
        else if(streamType=="foam")item.streamType=2;
        else if(streamType=="water")item.streamType=3;
        else{error="Invalid ["+section+"] StreamType";return false;}
        if(item.rocket&&item.streamType!=0){
            error="Rocket ["+section+"] cannot also be a stream";return false;
        }
        if(item.melee&&(item.streamType!=0||driveBy!=0||item.rocket||
            item.explosionRadius!=0||item.explosionDamage!=0||item.pellets!=1)){
            error="Invalid melee settings in ["+section+"]";return false;
        }
        if(item.rocket&&(item.explosionRadius<10||item.explosionDamage<1)){
            error="Rocket ["+section+"] needs explosion radius and damage";return false;
        }
        item.driveByAllowed=driveBy!=0;
        item.dualWieldAllowed=dual!=0;
        if(item.dualWieldAllowed&&(item.melee||item.arrow||item.rocket||
            item.streamType!=0||item.pellets!=1)){
            error="Invalid dual wield weapon ["+section+"]";return false;
        }
        parsed.push_back(item);
    }
    if(int(parsed.size())!=total){error=data.lastError();return false;}
    if(parsed.front().id!="pistol"||parsed.size()<5){error="First five weapon entries must preserve version 1 save order";return false;}
    const char* legacyIds[]={"pistol","smg","shotgun","rifle","sniper"};
    for(int i=0;i<5;++i)if(parsed[i].id!=legacyIds[i]){
        error="First five weapon entries must preserve version 1 save order";return false;}
    entries=std::move(parsed);
    return true;
}
const Stats& stats(int index){return entries[std::clamp(index,0,int(entries.size())-1)];}
int count(){return int(entries.size());}
int indexOf(const std::string& id){
    for(int i=0;i<int(entries.size());++i)if(entries[i].id==id)return i;
    return -1;
}
const std::string& lastError(){return error;}
}
