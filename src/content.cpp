#include "content.h"
#include "data_file.h"
#include "game_internal.h"
#include "weapons.h"
#include "regions.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <set>
#include <string>
#include <vector>

namespace content {
using namespace game;
namespace {
std::string error;
std::vector<std::string> missionNames;
int civilianCashMin=5,civilianCashMax=35,armedCashMin=45,armedCashMax=125,noticePercent=30;
bool bad(const std::string& section,const std::string& key){
    error="Invalid ["+section+"] "+key;return false;
}
bool integer(data_file::Ini& file,const std::string& s,const std::string& k,int& v,int lo,int hi){
    if(file.integer(s,k,v,lo,hi))return true;error=file.lastError();return false;
}
bool real(data_file::Ini& file,const std::string& s,const std::string& k,float& v,float lo,float hi){
    if(file.real(s,k,v,lo,hi))return true;error=file.lastError();return false;
}
bool string(data_file::Ini& file,const std::string& s,const std::string& k,std::string& v){
    if(file.string(s,k,v))return true;error=file.lastError();return false;
}
bool id(data_file::Ini& file,const std::string& s,std::string& v,std::set<std::string>& used){
    if(!string(file,s,"Id",v))return false;
    if(!data_file::validId(v))return bad(s,"Id");
    if(!used.insert(v).second){error="Duplicate Id: "+v;return false;}
    return true;
}
bool point(data_file::Ini& file,const std::string& s,const std::string& k,Vec2& p){
    std::string v;if(!string(file,s,k,v))return false;
    std::istringstream in(v);char comma=0;
    if(!(in>>p.x>>comma>>p.z)||comma!=',')return bad(s,k);
    in>>std::ws;
    if(!in.eof()||
       !std::isfinite(p.x)||!std::isfinite(p.z)||
       p.x<0||p.x>regions::WIDTH||p.z<0||p.z>regions::DEPTH)return bad(s,k);
    return true;
}
bool color(data_file::Ini& file,const std::string& s,const std::string& k,Color& c){
    std::string v;if(!string(file,s,k,v))return false;
    int r=0,g=0,b=0;char comma1=0,comma2=0;
    std::istringstream in(v);
    if(!(in>>r>>comma1>>g>>comma2>>b)||comma1!=','||comma2!=',')return bad(s,k);
    in>>std::ws;
    if(!in.eof()||
       r<0||r>255||g<0||g>255||b<0||b>255)return bad(s,k);
    c=rgb(r,g,b);return true;
}
bool kind(const std::string& v,Kind& out){
    if(v=="car")out=Kind::Car;
    else if(v=="sport-car")out=Kind::SportCar;
    else if(v=="bike")out=Kind::Bike;
    else if(v=="boat")out=Kind::Boat;
    else return false;
    return true;
}
bool missionKind(const std::string& v,MissionKind& out){
    if(v=="drive")out=MissionKind::Drive;
    else if(v=="collect")out=MissionKind::Collect;
    else if(v=="boat")out=MissionKind::Boat;
    else if(v=="targets")out=MissionKind::Targets;
    else if(v=="bike")out=MissionKind::Bike;
    else if(v=="finale")out=MissionKind::Finale;
    else return false;
    return true;
}
struct MissionRow {
    std::string id,name;Vec2 start;MissionKind kind;std::vector<Vec2> goals;
    float seconds=0;int reward=0;
};
}
const std::string& lastError(){return error;}
int rollPedCash(bool armed){
    int low=armed?armedCashMin:civilianCashMin;
    int high=armed?armedCashMax:civilianCashMax;
    return low+randi(high-low+1);
}
int pickpocketNoticePercent(){return noticePercent;}

bool populate(const char* worldPath,const char* missionsPath){
    error.clear();
    data_file::Ini world,missionFile;
    if(!world.load(worldPath?worldPath:data_file::resourcePath("world.ini"))||
       !world.version(1)){error=world.lastError();return false;}
    if(!missionFile.load(missionsPath?missionsPath:data_file::resourcePath("missions.ini"))||
       !missionFile.version(1)){error=missionFile.lastError();return false;}
    int seed=0;
    if(!integer(world,"Schema","Seed",seed,0,2147483647))return false;

    int rows=0,columns=0,sideCount=0,pedCount=0,armedEvery=0,armoredEvery=0,armor=0;
    int civilianMin=0,civilianMax=0,armedMin=0,armedMax=0,notice=0;
    int trafficCount=0,laneCount=0,bikeEvery=0,vehicleCount=0,pickupCount=0,missionCount=0;
    int treeColumns=0,treeRows=0,palmCount=0;
    float treeX=0,treeZ=0,treeColumnSpacing=0,treeRowSpacing=0,palmX=0,palmSpacing=0,palmZ=0;
    float startX=0,startZ=0,columnSpacing=0,rowSpacing=0,roadWidth=0,inset=0,beachLimit=0;
    float minHeight=0,maxHeight=0,sideX=0,sideZ=0,sideSpacing=0,sideWidth=0,sideDepth=0;
    float sideMin=0,sideMax=0,pedMin=0,pedMax=0,laneX=0,laneSpacing=0,laneJitter=0,minZ=0,beachMargin=0;
    Color facades[5]{};
    const std::string grid="BuildingGrid",side="SideBuildings",ped="Pedestrians",traffic="Traffic";
    if(!integer(world,grid,"Rows",rows,1,20)||
       !integer(world,grid,"Columns",columns,1,20)||
       !real(world,grid,"StartX",startX,0,WORLD_W)||
       !real(world,grid,"StartZ",startZ,0,BEACH_START)||
       !real(world,grid,"ColumnSpacing",columnSpacing,50,1000)||
       !real(world,grid,"RowSpacing",rowSpacing,50,1000)||
       !real(world,grid,"RoadWidth",roadWidth,10,250)||
       !real(world,grid,"Inset",inset,0,100)||
       !real(world,grid,"BeachLimit",beachLimit,0,200)||
       !real(world,grid,"MinHeight",minHeight,20,1000)||
       !real(world,grid,"MaxHeight",maxHeight,minHeight,1000)||
       !integer(world,side,"Count",sideCount,0,20)||
       !real(world,side,"X",sideX,0,WORLD_W)||
       !real(world,side,"StartZ",sideZ,0,BEACH_START)||
       !real(world,side,"RowSpacing",sideSpacing,10,1000)||
       !real(world,side,"Width",sideWidth,10,500)||
       !real(world,side,"Depth",sideDepth,10,500)||
       !real(world,side,"MinHeight",sideMin,20,1000)||
       !real(world,side,"MaxHeight",sideMax,sideMin,1000))return false;
    for(int i=0;i<5;++i)if(!color(world,grid,"Facade"+std::to_string(i),facades[i]))return false;
    if(startX+columns*columnSpacing>WORLD_W+columnSpacing||
       startZ+rows*rowSpacing>BEACH_START+rowSpacing||
       sideX+sideWidth>WORLD_W||
       sideZ+(sideCount-1)*sideSpacing+sideDepth>BEACH_START)return bad(grid,"bounds");
    if(!integer(world,"Trees","StreetColumns",treeColumns,0,100)||
       !integer(world,"Trees","StreetRows",treeRows,0,100)||
       !real(world,"Trees","StreetStartX",treeX,0,WORLD_W)||
       !real(world,"Trees","StreetStartZ",treeZ,0,BEACH_START)||
       !real(world,"Trees","StreetColumnSpacing",treeColumnSpacing,1,1000)||
       !real(world,"Trees","StreetRowSpacing",treeRowSpacing,1,1000)||
       !integer(world,"Trees","PalmCount",palmCount,0,200)||
       !real(world,"Trees","PalmStartX",palmX,0,WORLD_W)||
       !real(world,"Trees","PalmSpacing",palmSpacing,1,1000)||
       !real(world,"Trees","PalmZ",palmZ,BEACH_START,SHORE))return false;
    if(treeX+std::max(0,treeColumns-1)*treeColumnSpacing>WORLD_W||
       treeZ+std::max(0,treeRows-1)*treeRowSpacing>BEACH_START||
       palmX+std::max(0,palmCount-1)*palmSpacing>WORLD_W)return bad("Trees","bounds");
    if(!integer(world,ped,"Count",pedCount,0,1000)||
       !real(world,ped,"MinSpeed",pedMin,1,300)||
       !real(world,ped,"MaxSpeed",pedMax,pedMin,300)||
       !integer(world,ped,"ArmedEvery",armedEvery,1,1000)||
       !integer(world,ped,"ArmoredEvery",armoredEvery,1,1000)||
       !integer(world,ped,"Armor",armor,0,100)||
       !integer(world,ped,"CivilianCashMin",civilianMin,0,10000)||
       !integer(world,ped,"CivilianCashMax",civilianMax,civilianMin,10000)||
       !integer(world,ped,"ArmedCashMin",armedMin,civilianMax+1,10000)||
       !integer(world,ped,"ArmedCashMax",armedMax,armedMin,10000)||
       !integer(world,"Interactions","PickpocketNoticePercent",notice,0,100))return false;
    if(!integer(world,traffic,"Count",trafficCount,0,1000)||
       !integer(world,traffic,"LaneCount",laneCount,1,50)||
       !real(world,traffic,"LaneStartX",laneX,0,WORLD_W)||
       !real(world,traffic,"LaneSpacing",laneSpacing,10,1000)||
       !real(world,traffic,"LaneJitter",laneJitter,0,100)||
       !real(world,traffic,"MinZ",minZ,0,BEACH_START)||
       !real(world,traffic,"BeachMargin",beachMargin,0,500)||
       !integer(world,traffic,"BikeEvery",bikeEvery,1,1000))return false;
    if(laneX+(laneCount-1)*laneSpacing+laneJitter>WORLD_W||
       minZ>=BEACH_START-beachMargin)return bad(traffic,"bounds");
    if(!integer(world,"Vehicles","Count",vehicleCount,1,200)||
       !integer(world,"Pickups","Count",pickupCount,0,200)||
       !integer(missionFile,"Missions","Count",missionCount,6,10))return false;

    std::set<std::string> vehicleIds,pickupIds,missionIds;
    std::vector<Vehicle> placed;placed.reserve(vehicleCount);
    for(int i=0;i<vehicleCount;++i){
        const std::string s="Vehicle"+std::to_string(i);std::string value;
        Vehicle v{};
        if(!id(world,s,v.id,vehicleIds)||!string(world,s,"Kind",value))return false;
        if(!kind(value,v.kind))return bad(s,"Kind");
        if(!real(world,s,"X",v.p.x,0,WORLD_W)||
           !real(world,s,"Z",v.p.z,0,WORLD_D)||
           !real(world,s,"Angle",v.angle,-PI,PI)||
           !color(world,s,"Color",v.c))return false;
        if((v.kind==Kind::Boat)!=(v.p.z>=SHORE))return bad(s,"Z for vehicle kind");
        placed.push_back(v);
    }
    std::vector<Pickup> placedPickups;placedPickups.reserve(pickupCount);
    for(int i=0;i<pickupCount;++i){
        const std::string s="Pickup"+std::to_string(i);std::string weaponId;
        Pickup p{};
        if(!id(world,s,p.id,pickupIds)||!string(world,s,"WeaponId",weaponId))return false;
        p.weapon=weapons::indexOf(weaponId);
        if(p.weapon<0)return bad(s,"WeaponId");
        if(!real(world,s,"X",p.p.x,0,WORLD_W)||
           !real(world,s,"Z",p.p.z,0,SHORE))return false;
        placedPickups.push_back(p);
    }
    std::vector<MissionRow> missionRows;missionRows.reserve(missionCount);
    const char* legacyIds[]={"sunset-circuit","beach-cache","harbor-run",
        "boardwalk-range","neon-sprint","coastline-finale"};
    for(int i=0;i<missionCount;++i){
        const std::string s="Mission"+std::to_string(i);std::string value;
        MissionRow m{};
        if(!id(missionFile,s,m.id,missionIds))return false;
        if(i<6&&m.id!=legacyIds[i])return bad(s,"Id must retain save order");
        if(!string(missionFile,s,"Name",m.name)||!string(missionFile,s,"Kind",value))return false;
        if(!missionKind(value,m.kind))return bad(s,"Kind");
        if(!point(missionFile,s,"Start",m.start)||
           !real(missionFile,s,"Seconds",m.seconds,1,3600)||
           !integer(missionFile,s,"Reward",m.reward,0,1000000))return false;
        int goalCount=0;
        if(!integer(missionFile,s,"GoalCount",goalCount,1,30))return false;
        for(int g=0;g<goalCount;++g){
            Vec2 destination{};
            if(!point(missionFile,s,"Goal"+std::to_string(g),destination))return false;
            m.goals.push_back(destination);
        }
        missionRows.push_back(m);
    }

    std::srand(unsigned(seed));
    civilianCashMin=civilianMin;civilianCashMax=civilianMax;
    armedCashMin=armedMin;armedCashMax=armedMax;noticePercent=notice;
    buildings.clear();trees.clear();peds.clear();vehicles.clear();pickups.clear();missions.clear();
    for(int row=0;row<rows;++row)for(int col=0;col<columns;++col){
        float x=startX+col*columnSpacing+roadWidth/2+inset;
        float z=startZ+row*rowSpacing+roadWidth/2+inset;
        float right=startX+(col+1)*columnSpacing-roadWidth/2-inset;
        float bottom=std::min(startZ+(row+1)*rowSpacing-roadWidth/2-inset,BEACH_START-beachLimit);
        if(right>x+50&&bottom>z+50)buildings.push_back({x,z,right-x,bottom-z,
            randf(minHeight,maxHeight),facades[(row*2+col)%5],
            "grid-"+std::to_string(row)+"-"+std::to_string(col)});
    }
    for(int row=0;row<sideCount;++row)buildings.push_back({sideX,sideZ+row*sideSpacing,
        sideWidth,sideDepth,randf(sideMin,sideMax),facades[row%5],"side-"+std::to_string(row)});
    for(int i=0;i<palmCount;++i){
        Tree tree{};tree.id="palm-"+std::to_string(i);
        tree.p={palmX+i*palmSpacing,palmZ};tree.palm=true;tree.variant=i%3;
        tree.scale=0.9f+0.05f*float(i%5);trees.push_back(tree);
    }
    for(int row=0;row<treeRows;++row)for(int col=0;col<treeColumns;++col){
        Tree tree{};tree.id="street-tree-"+std::to_string(row)+"-"+std::to_string(col);
        tree.p={treeX+col*treeColumnSpacing,treeZ+row*treeRowSpacing};
        tree.variant=(row+col)%3;tree.scale=0.88f+0.05f*float((row*3+col)%5);
        trees.push_back(tree);
    }
    for(int i=0;i<pedCount;++i){
        Vec2 p=randomWalkable();
        peds.push_back({p,p,randf(pedMin,pedMax),0,0,true,
            rgb(70+randi(165),75+randi(155),75+randi(160)),100,0,i%4,i%armedEvery==0});
        peds.back().armor=i%armoredEvery==0?armor:0;
        peds.back().maxArmor=peds.back().armor;
        peds.back().cash=rollPedCash(peds.back().armed);
        peds.back().id="ped-"+std::to_string(i);
    }
    for(int i=0;i<trafficCount;++i){
        int lane=randi(laneCount);
        Vec2 p{laneX+lane*laneSpacing+randf(-laneJitter,laneJitter),
            randf(minZ,BEACH_START-beachMargin)};
        vehicles.push_back({p,randf(-PI,PI),0,i%bikeEvery==0?Kind::Bike:Kind::Car,
            rgb(70+randi(175),70+randi(140),75+randi(170))});
        vehicles.back().id="traffic-"+std::to_string(i);
    }
    vehicles.insert(vehicles.end(),placed.begin(),placed.end());
    pickups=std::move(placedPickups);
    missionNames.clear();missionNames.reserve(missionRows.size());
    for(const auto& row:missionRows)missionNames.push_back(row.name);
    for(int i=0;i<int(missionRows.size());++i)missions.push_back({missionNames[i].c_str(),
        missionRows[i].start,missionRows[i].kind,missionRows[i].goals,
        missionRows[i].seconds,missionRows[i].reward,missionRows[i].id});
    return true;
}
}
