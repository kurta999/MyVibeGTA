#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "weapons.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace weapons {
namespace {
constexpr std::array<Stats,5> defaults{{
    {"PISTOL",12,1.15f,0.35f,52,760,0.005f,1,12,650,0.35f,0.012f,0},
    {"SMG",30,1.55f,0.09f,22,900,0.055f,1,15,610,0.45f,0.006f,120},
    {"SHOTGUN",8,1.9f,0.55f,24,690,0.11f,7,38,250,0.65f,0.055f,35},
    {"RIFLE",24,1.65f,0.14f,36,1100,0.023f,1,8,950,0.2f,0.012f,90},
    {"SNIPER",5,2.4f,0.9f,100,1600,0.002f,1,2,1200,0.05f,0.08f,18}
}};
std::array<Stats,5> entries=defaults;

float readFloat(const char* section,const char* key,float fallback,const char* path){
    char fallbackText[32]{},value[64]{};
    std::snprintf(fallbackText,sizeof(fallbackText),"%.6f",fallback);
    GetPrivateProfileStringA(section,key,fallbackText,value,sizeof(value),path);
    char* end=nullptr;float result=std::strtof(value,&end);
    return end!=value&&std::isfinite(result)?result:fallback;
}
int readInt(const char* section,const char* key,int fallback,const char* path){
    return int(GetPrivateProfileIntA(section,key,fallback,path));
}
}

bool load(const char* file){
    entries=defaults;
    std::string defaultPath;
    if(!file){
        char executable[MAX_PATH]{};
        if(!GetModuleFileNameA(nullptr,executable,MAX_PATH))return false;
        defaultPath=executable;
        size_t slash=defaultPath.find_last_of("\\/");
        defaultPath=defaultPath.substr(0,slash+1)+"assets\\weapons.ini";
        file=defaultPath.c_str();
    }
    if(GetFileAttributesA(file)==INVALID_FILE_ATTRIBUTES)return false;
    for(auto& item:entries){
        const char* section=item.name;
        item.magazine=std::clamp(readInt(section,"Magazine",item.magazine,file),1,200);
        item.reservePickup=std::clamp(readInt(section,"ReservePickup",item.reservePickup,file),0,9999);
        item.reloadSeconds=std::clamp(readFloat(section,"ReloadSeconds",item.reloadSeconds,file),0.1f,10.0f);
        item.secondsBetweenShots=std::clamp(readFloat(section,"FireInterval",item.secondsBetweenShots,file),0.03f,5.0f);
        item.damage=std::clamp(readInt(section,"Damage",item.damage,file),1,1000);
        item.projectileSpeed=std::clamp(readFloat(section,"ProjectileSpeed",item.projectileSpeed,file),100.0f,10000.0f);
        item.spread=std::clamp(readFloat(section,"Spread",item.spread,file),0.0f,0.5f);
        item.pellets=std::clamp(readInt(section,"Pellets",item.pellets,file),1,32);
        item.gravity=std::clamp(readFloat(section,"Gravity",item.gravity,file),0.0f,1000.0f);
        item.range=std::clamp(readFloat(section,"Range",item.range,file),20.0f,5000.0f);
        item.falloff=std::clamp(readFloat(section,"Falloff",item.falloff,file),0.0f,1.0f);
        item.recoilKick=std::clamp(readFloat(section,"RecoilKick",item.recoilKick,file),0.0f,0.5f);
    }
    return true;
}
const Stats& stats(int index){return entries[std::clamp(index,0,4)];}
}
