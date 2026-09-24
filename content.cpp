#include "content.h"
#include "game_internal.h"
#include <algorithm>

namespace content {
using namespace game;
void populate(){
    Color facades[]={rgb(184,162,137),rgb(152,174,179),rgb(209,177,151),rgb(176,170,161),rgb(201,193,144)};
    for(int row=0;row<4;++row)for(int col=0;col<5;++col){
        float x=300+col*450+ROAD_W/2+23;
        float z=250+row*390+ROAD_W/2+23;
        float right=300+(col+1)*450-ROAD_W/2-23;
        float bottom=std::min(250+(row+1)*390-ROAD_W/2-23.0f,BEACH_START-35.0f);
        if(right>x+50&&bottom>z+50)buildings.push_back({x,z,right-x,bottom-z,randf(140,300),facades[(row*2+col)%5]});
    }
    for(int row=0;row<3;++row)buildings.push_back({45.0f,330.0f+row*390,160,235,randf(120,225),facades[row]});
    for(int i=0;i<85;++i){Vec2 p=randomWalkable();peds.push_back({p,p,randf(28,54),0,0,true,
        rgb(70+randi(165),75+randi(155),75+randi(160)),100,0,i%4,i%7==0});
        peds.back().armor=i%14==0?25:0;}
    for(int i=0;i<16;++i){
        int lane=randi(5);Vec2 p{300.0f+lane*450+randf(-23,23),randf(90,BEACH_START-90)};
        vehicles.push_back({p,randf(-PI,PI),0,i%5==0?Kind::Bike:Kind::Car,
            rgb(70+randi(175),70+randi(140),75+randi(170))});
    }
    vehicles.push_back({{345,280},0,0,Kind::Car,rgb(228,88,75)});
    vehicles.push_back({{770,640},0,0,Kind::SportCar,rgb(69,177,222)});
    vehicles.push_back({{300,335},PI/2,0,Kind::Bike,rgb(245,205,67)});
    vehicles.push_back({{300,1900},-PI/2,0,Kind::Boat,rgb(239,220,180)});
    vehicles.push_back({{800,1900},-PI/2,0,Kind::Boat,rgb(100,210,225)});
    vehicles.push_back({{1490,1900},-PI/2,0,Kind::Boat,rgb(245,145,91)});
    pickups={{ {750,250},1 },{{1200,1420},2},{{1650,640},3},{{300,1740},4}};
    missions={
        {"Sunset Circuit",{330,250},MissionKind::Drive,{{750,250},{750,640},{1200,640},{1650,1030}},150,600},
        {"Beach Cache",{750,1450},MissionKind::Collect,{{520,1710},{1150,1770},{1900,1730}},180,450},
        {"Harbor Run",{300,1710},MissionKind::Boat,{{300,1930},{1000,2010},{1850,1930}},170,700},
        {"Boardwalk Range",{1200,1030},MissionKind::Targets,{{1300,1030},{1450,1030},{1600,1030}},120,350},
        {"Neon Sprint",{340,350},MissionKind::Bike,{{300,640},{750,640},{1200,1030},{1650,1420}},110,550},
        {"Coastline Finale",{1650,1420},MissionKind::Finale,{{1600,1520},{1800,1730},{1490,1960}},330,1500}
    };
}
}
