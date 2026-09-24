#include "savegame.h"
#include "game.h"
#include "weapons.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace savegame {
namespace {
std::string path(){
    char filename[MAX_PATH]{};GetModuleFileNameA(nullptr,filename,MAX_PATH);
    std::string result(filename);auto slash=result.find_last_of("\\/");
    return result.substr(0,slash+1)+"savegame.ini";
}
void write(const char* section,const char* key,int value,const std::string& file){
    char number[32];std::snprintf(number,sizeof(number),"%d",value);
    WritePrivateProfileStringA(section,key,number,file.c_str());
}
int read(const char* section,const char* key,int fallback,const std::string& file){
    return int(GetPrivateProfileIntA(section,key,fallback,file.c_str()));
}
}
bool save(){
    const std::string file=path();
    write("Save","Version",1,file);
    write("Player","X",int(game::player.x),file);
    write("Player","Z",int(game::player.z),file);
    write("Player","Health",int(game::health),file);
    write("Player","Money",game::money,file);
    write("Player","Hour",int(game::gameHour*100),file);
    write("Player","Weapon",game::weapon,file);
    for(int i=0;i<5;++i){char key[32];
        std::snprintf(key,sizeof(key),"Unlocked%d",i);write("Weapons",key,game::unlocked[i]?1:0,file);
        std::snprintf(key,sizeof(key),"Reserve%d",i);write("Weapons",key,game::ammo[i],file);
        std::snprintf(key,sizeof(key),"Magazine%d",i);write("Weapons",key,game::magazine[i],file);
    }
    for(int i=0;i<int(game::missionDone.size());++i){char key[32];std::snprintf(key,sizeof(key),"Complete%d",i);
        write("Missions",key,game::missionDone[i]?1:0,file);}
    return GetFileAttributesA(file.c_str())!=INVALID_FILE_ATTRIBUTES;
}
bool load(){
    const std::string file=path();
    if(GetFileAttributesA(file.c_str())==INVALID_FILE_ATTRIBUTES||read("Save","Version",0,file)!=1)return false;
    game::Vec2 position{float(read("Player","X",300,file)),float(read("Player","Z",250,file))};
    bool safe=position.x>15&&position.x<game::WORLD_W-15&&position.z>15&&position.z<game::SHORE-15;
    for(const auto& b:game::buildings)if(position.x>b.x-12&&position.x<b.x+b.w+12&&
        position.z>b.z-12&&position.z<b.z+b.d+12)safe=false;
    if(safe)game::player=position;
    game::previousPlayer=game::player;
    game::occupied=-1;game::playerY=0;game::playerVerticalSpeed=0;game::playerVelocity={};
    game::health=std::clamp(float(read("Player","Health",100,file)),1.0f,100.0f);
    game::money=std::clamp(read("Player","Money",0,file),0,9999999);
    game::gameHour=std::clamp(read("Player","Hour",1650,file)/100.0f,0.0f,23.99f);
    for(int i=0;i<5;++i){char key[32];
        std::snprintf(key,sizeof(key),"Unlocked%d",i);
        game::unlocked[i]=i==0||read("Weapons",key,0,file)!=0;
        std::snprintf(key,sizeof(key),"Reserve%d",i);
        game::ammo[i]=i==0?-1:std::clamp(read("Weapons",key,0,file),0,9999);
        std::snprintf(key,sizeof(key),"Magazine%d",i);
        game::magazine[i]=std::clamp(read("Weapons",key,i==0?12:0,file),0,weapons::stats(i).magazine);
    }
    game::weapon=std::clamp(read("Player","Weapon",0,file),0,4);
    if(!game::unlocked[game::weapon])game::weapon=0;
    for(int i=0;i<int(game::missionDone.size());++i){char key[32];std::snprintf(key,sizeof(key),"Complete%d",i);
        game::missionDone[i]=read("Missions",key,0,file)!=0;}
    game::activeMission=-1;game::missionStep=0;
    return true;
}
}
