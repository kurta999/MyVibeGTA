#include "savegame.h"
#include "game.h"
#include "weapons.h"
#include "police.h"
#include "commerce.h"
#include "weather.h"
#include "regions.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>

namespace savegame {
namespace {
std::string path(){
    char filename[MAX_PATH]{};GetModuleFileNameA(nullptr,filename,MAX_PATH);
    std::string result(filename);auto slash=result.find_last_of("\\/");
    return result.substr(0,slash+1)+"savegame.ini";
}
bool writeText(const std::string& section,const char* key,const std::string& value,const std::string& file){
    return WritePrivateProfileStringA(section.c_str(),key,value.c_str(),file.c_str())!=0;
}
bool write(const std::string& section,const char* key,int value,const std::string& file){
    return writeText(section,key,std::to_string(value),file);
}
int read(const std::string& section,const char* key,int fallback,const std::string& file){
    return int(GetPrivateProfileIntA(section.c_str(),key,fallback,file.c_str()));
}
std::string readText(const std::string& section,const char* key,const std::string& file){
    char value[128]{};
    GetPrivateProfileStringA(section.c_str(),key,"",value,sizeof(value),file.c_str());
    return value;
}
}
bool save(){
    const std::string file=path(),temporary=file+".tmp";
    {std::ofstream clear(temporary,std::ios::trunc);if(!clear)return false;}
    bool ok=true;
    ok&=write("Save","Version",game::PLAYER_MAX_HEALTH>100?3:2,temporary);
    ok&=write("Player","X",int(game::player.x),temporary);
    ok&=write("Player","Z",int(game::player.z),temporary);
    ok&=write("Player","Y",int(game::playerY),temporary);
    ok&=write("Player","Health",int(game::health),temporary);
    ok&=write("Player","Armor",int(game::armor),temporary);
    ok&=write("Player","RepairKits",game::repairKits,temporary);
    ok&=write("Player","Wanted",police::wantedLevel(),temporary);
    ok&=write("Player","Money",game::money,temporary);
    ok&=write("Player","Hour",int(game::gameHour*100),temporary);
    ok&=writeText("Weather","Id",weather::current().id,temporary);
    ok&=write("Weather","Remaining",int(weather::remaining()),temporary);
    ok&=writeText("Player","WeaponId",weapons::stats(game::weapon).id,temporary);
    for(int i=0;i<weapons::count();++i){
        std::string section="Weapon."+weapons::stats(i).id;
        ok&=write(section,"Unlocked",game::unlocked[i]?1:0,temporary);
        ok&=write(section,"Reserve",game::ammo[i],temporary);
        ok&=write(section,"Magazine",game::magazine[i],temporary);
        ok&=write(section,"ArmedKills",game::armedKills[i],temporary);
    }
    for(int i=0;i<int(game::missions.size());++i)
        ok&=write("Mission."+game::missions[i].id,"Complete",game::missionDone[i]?1:0,temporary);
    for(const auto& ped:game::peds){
        if(ped.alive&&ped.cash>0&&!ped.looted)continue;
        std::string section="Ped."+ped.id;
        ok&=write(section,"Dead",ped.alive?0:1,temporary);
        ok&=write(section,"Cash",ped.cash,temporary);
        ok&=write(section,"Looted",ped.looted?1:0,temporary);
        if(!ped.alive){
            ok&=write(section,"Pinned",ped.pinned?1:0,temporary);
            if(ped.pinned){
                ok&=write(section,"PinX",int(ped.pinAnchor.x),temporary);
                ok&=write(section,"PinZ",int(ped.pinAnchor.z),temporary);
            }
            ok&=write(section,"X",int(ped.p.x),temporary);
            ok&=write(section,"Z",int(ped.p.z),temporary);
            ok&=write(section,"Respawn",int(ped.respawn),temporary);
        }
    }
    for(const auto& house:commerce::houses)
        ok&=write("House."+house.id,"Owned",house.owned?1:0,temporary);
    for(const auto& vehicle:game::vehicles)if(vehicle.damage>0||vehicle.exploded||vehicle.owned){
        std::string section="Vehicle."+vehicle.id;
        ok&=write(section,"Damage",int(vehicle.damage),temporary);
        ok&=write(section,"Exploded",vehicle.exploded?1:0,temporary);
        ok&=write(section,"Owned",vehicle.owned?1:0,temporary);
        if(vehicle.owned){
            ok&=writeText(section,"GarageHouseId",
                vehicle.garageHouseId.empty()?"none":vehicle.garageHouseId,temporary);
            ok&=write(section,"X",int(vehicle.p.x),temporary);
            ok&=write(section,"Z",int(vehicle.p.z),temporary);
            ok&=write(section,"Angle",int(vehicle.angle*1000),temporary);
        }
    }
    for(const auto& tree:game::trees)if(tree.health<100||tree.destroyed){
        std::string section="Tree."+tree.id;
        ok&=write(section,"Health",tree.health,temporary);
        ok&=write(section,"Destroyed",tree.destroyed?1:0,temporary);
    }
    WritePrivateProfileStringA(nullptr,nullptr,nullptr,temporary.c_str());
    if(!ok||!MoveFileExA(temporary.c_str(),file.c_str(),
            MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileA(temporary.c_str());return false;
    }
    return true;
}
bool load(){
    const std::string file=path();
    if(GetFileAttributesA(file.c_str())==INVALID_FILE_ATTRIBUTES)return false;
    int version=read("Save","Version",0,file);
    if(version!=1&&version!=2&&version!=3)return false;
    game::reset();
    game::Vec2 position{float(read("Player","X",300,file)),float(read("Player","Z",250,file))};
    float height=version>=2?float(std::clamp(read("Player","Y",0,file),0,1000)):0;
    bool safe=position.x>15&&position.x<regions::WIDTH-15&&
        position.z>15&&position.z<regions::DEPTH-15&&
        !regions::waterAt(position);
    bool onRoof=false;
    for(const auto& b:game::buildings)if(position.x>b.x-12&&position.x<b.x+b.w+12&&
        position.z>b.z-12&&position.z<b.z+b.d+12){
        bool withinRoof=position.x>b.x+12&&position.x<b.x+b.w-12&&
            position.z>b.z+12&&position.z<b.z+b.d-12;
        if(withinRoof&&height>=b.h-3&&height<=b.h+10)onRoof=true;
        else safe=false;
    }
    if(safe)game::player=position;
    game::previousPlayer=game::player;
    game::occupied=-1;game::playerY=safe&&onRoof?height:0;
    game::grounded=!onRoof;game::playerVerticalSpeed=0;game::playerVelocity={};
    game::clearCarry();
#ifdef MINI_CITY_JOLT
    jolt_world::clearRagdolls();
    jolt_world::teleportCharacter(game::player,game::playerY);
#endif
    float savedHealth=float(read("Player","Health",version>=3?400:100,file));
    game::health=std::clamp(savedHealth*(version>=3?1.0f:
        game::PLAYER_MAX_HEALTH/100.0f),
        1.0f,game::PLAYER_MAX_HEALTH);
    game::armor=version==1?0.0f:
        std::clamp(float(read("Player","Armor",0,file)),0.0f,100.0f);
    game::repairKits=version==1?1:
        std::clamp(read("Player","RepairKits",1,file),0,99);
    police::setWantedLevel(version==1?0:read("Player","Wanted",0,file));
    game::money=std::clamp(read("Player","Money",0,file),0,9999999);
    game::gameHour=std::clamp(read("Player","Hour",1650,file)/100.0f,0.0f,23.99f);
    if(version>=2)weather::set(readText("Weather","Id",file),
        float(read("Weather","Remaining",0,file)));
    game::unlocked.assign(weapons::count(),false);
    game::ammo.assign(weapons::count(),0);
    game::magazine.assign(weapons::count(),0);
    for(int i=0;i<weapons::count();++i){
        if(version==1&&i<5){
            char key[32]{};
            std::snprintf(key,sizeof(key),"Unlocked%d",i);
            game::unlocked[i]=i==0||read("Weapons",key,0,file)!=0;
            std::snprintf(key,sizeof(key),"Reserve%d",i);
            game::ammo[i]=i==0?-1:std::clamp(read("Weapons",key,0,file),0,9999);
            std::snprintf(key,sizeof(key),"Magazine%d",i);
            game::magazine[i]=std::clamp(read("Weapons",key,i==0?12:0,file),
                0,weapons::stats(i).magazine);
        }else if(version>=2){
            std::string section="Weapon."+weapons::stats(i).id;
            game::unlocked[i]=i==0||read(section,"Unlocked",0,file)!=0;
            game::ammo[i]=i==0?-1:std::clamp(read(section,"Reserve",0,file),0,9999);
            game::magazine[i]=std::clamp(read(section,"Magazine",i==0?12:0,file),
                0,weapons::stats(i).magazine);
            if(weapons::stats(i).melee&&game::unlocked[i]){
                game::ammo[i]=-1;game::magazine[i]=1;
            }
            game::armedKills[i]=std::clamp(read(section,"ArmedKills",0,file),0,100000);
        }
    }
    game::weapon=version==1?
        std::clamp(read("Player","Weapon",0,file),0,std::min(4,weapons::count()-1)):
        weapons::indexOf(readText("Player","WeaponId",file));
    if(game::weapon<0||!game::unlocked[game::weapon])game::weapon=0;
    for(int i=0;i<int(game::missionDone.size());++i){
        if(version==1){
            char key[32]{};
            std::snprintf(key,sizeof(key),"Complete%d",i);
            game::missionDone[i]=read("Missions",key,0,file)!=0;
        }else game::missionDone[i]=read("Mission."+game::missions[i].id,"Complete",0,file)!=0;
    }
    if(version>=2)for(auto& ped:game::peds){
        std::string section="Ped."+ped.id;
        int dead=read(section,"Dead",-1,file);
        if(dead<0)continue;
        ped.cash=std::clamp(read(section,"Cash",ped.cash,file),0,10000);
        ped.looted=read(section,"Looted",0,file)!=0;
        ped.alive=dead==0;
        ped.carried=false;ped.corpseVisualDelay=0;
        ped.pinned=false;ped.pinAnchor={};
        if(!ped.alive){
            ped.p.x=float(read(section,"X",int(ped.p.x),file));
            ped.p.z=float(read(section,"Z",int(ped.p.z),file));
            ped.respawn=float(std::clamp(read(section,"Respawn",45,file),1,45));
            ped.pinned=read(section,"Pinned",0,file)!=0;
            if(ped.pinned){
                ped.pinAnchor={float(read(section,"PinX",int(ped.p.x),file)),
                    float(read(section,"PinZ",int(ped.p.z),file))};
                ped.corpseVisualDelay=0;
#ifdef MINI_CITY_JOLT
                if(game::len(ped.p-game::player)<500){
                    jolt_world::spawnRagdoll(ped,{0,0,0},&ped.pinAnchor);
                    ped.corpseVisualDelay=std::min(15.0f,ped.respawn);
                }
#endif
            }
        }
    }
    if(version>=2)for(auto& house:commerce::houses)
        house.owned=read("House."+house.id,"Owned",0,file)!=0;
    if(version>=2)for(int index=0;index<int(game::vehicles.size());++index){
        auto& vehicle=game::vehicles[index];
        std::string section="Vehicle."+vehicle.id;
        vehicle.damage=float(std::clamp(read(section,"Damage",0,file),0,100));
        vehicle.exploded=read(section,"Exploded",0,file)!=0;
        if(vehicle.exploded)vehicle.damage=100;
        vehicle.owned=read(section,"Owned",0,file)!=0;
        if(vehicle.owned){
            std::string garage=readText(section,"GarageHouseId",file);
            vehicle.garageHouseId=garage=="none"?"":garage;
            bool validGarage=vehicle.garageHouseId.empty();
            for(const auto& house:commerce::houses)
                if(house.id==vehicle.garageHouseId&&house.owned)validGarage=true;
            if(!validGarage)vehicle.garageHouseId.clear();
            game::Vec2 position{float(read(section,"X",int(vehicle.p.x),file)),
                float(read(section,"Z",int(vehicle.p.z),file))};
            bool safe=position.x>15&&position.x<regions::WIDTH-15&&
                position.z>15&&position.z<regions::DEPTH-15&&
                (vehicle.kind==game::Kind::Boat)==regions::waterAt(position);
            if(safe){
                float angle=std::clamp(read(section,"Angle",int(vehicle.angle*1000),file)
                    /1000.0f,-game::PI,game::PI);
#ifdef MINI_CITY_JOLT
                jolt_world::teleportVehicle(index,position,angle);
#else
                vehicle.p=position;vehicle.angle=angle;
#endif
            }
        }
    }
    if(version>=2)for(auto& tree:game::trees){
        std::string section="Tree."+tree.id;
        tree.health=std::clamp(read(section,"Health",100,file),0,100);
        tree.destroyed=read(section,"Destroyed",0,file)!=0||tree.health==0;
        tree.burning=false;
    }
    game::activeMission=-1;game::missionStep=0;
    return true;
}
}
