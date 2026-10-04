#include "physics.h"
#include "data_file.h"
#include "fire.h"
#include "weather.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace physics {
using namespace game;
float vehicleScale(Kind kind){
#ifdef MINI_CITY_JOLT
    return kind==Kind::Car||kind==Kind::SportCar||kind==Kind::Helicopter?2.0f:1.0f;
#else
    return 1.0f;
#endif
}
Vec3 chassisHalf(Kind k){
    switch(k){
    case Kind::Skateboard:return {5,1,15};case Kind::Bicycle:return {5,4,17};
    case Kind::Tractor:return {19,8,27};case Kind::Combine:return {31,12,42};
    case Kind::Tank:return {30,10,43};case Kind::Truck:return {26,10,58};
    case Kind::Trailer:return {27,8,54};case Kind::Airplane:return {74,8,52};
    default:return Vec3{k==Kind::Helicopter?16.0f:k==Kind::Bike?5.0f:k==Kind::Boat?12.0f:13.0f,
        k==Kind::Helicopter?12.0f:k==Kind::Boat?10.0f:k==Kind::Bike?4.0f:5.0f,
        k==Kind::Helicopter?30.0f:k==Kind::Bike?13.0f:24.0f}*vehicleScale(k);
    }
}
float vehicleRestHeight(Kind kind){
    if(int(kind)>=5)return kind==Kind::Skateboard?4:kind==Kind::Bicycle?12:
        kind==Kind::Tractor?22:kind==Kind::Combine?26:kind==Kind::Tank?18:
        kind==Kind::Truck?24:kind==Kind::Trailer?22:16;
    return (kind==Kind::Helicopter?12.0f:kind==Kind::Boat?10.0f:
        kind==Kind::Bike?13.0f:14.0f)*vehicleScale(kind);
}
float vehicleRadius(Kind kind){
#ifndef MINI_CITY_JOLT
    return kind==Kind::Bike?14.0f:kind==Kind::Boat?25.0f:26.0f;
#else
    if(int(kind)>=5){auto half=chassisHalf(kind);return std::sqrt(half.x*half.x+half.z*half.z);}
    return (kind==Kind::Bike?14.0f:kind==Kind::Boat?25.0f:
        kind==Kind::Helicopter?34.0f:26.0f)*vehicleScale(kind);
#endif
}
namespace {
Vec3 hitSize(Kind kind){
    if(int(kind)>=5){auto half=chassisHalf(kind);return {half.x,kind==Kind::Skateboard?5.0f:
        kind==Kind::Bicycle?26.0f:kind==Kind::Tank?34.0f:kind==Kind::Airplane?29.0f:55.0f,half.z};}
    Vec3 size=kind==Kind::Helicopter?Vec3{13.25f,30.5f,40.75f}:
        kind==Kind::Bike?Vec3{7,42,15}:kind==Kind::Boat?Vec3{12,42,24}:
        Vec3{13,kind==Kind::SportCar?20.0f:26.0f,24};
    return size*vehicleScale(kind);
}
Vec3 vehicleLocal(const Vehicle& v,Vec3 point){
    Vec2 f=forward(v.angle),side{-f.z,f.x};
    Vec2 offset{point.x-v.p.x,point.z-v.p.z};
    return {offset.x*side.x+offset.z*side.z,point.y-v.rideHeight,
        offset.x*f.x+offset.z*f.z};
}
}
bool vehicleContains(const Vehicle& vehicle,Vec3 point){
    Vec3 p=vehicleLocal(vehicle,point),size=hitSize(vehicle.kind);
    return std::abs(p.x)<=size.x&&std::abs(p.z)<=size.z&&p.y>=2&&p.y<=size.y;
}
bool vehicleSegmentHit(const Vehicle& vehicle,Vec3 start,Vec3 end,float& entry){
    Vec3 p=vehicleLocal(vehicle,start),d=vehicleLocal(vehicle,end)-p,size=hitSize(vehicle.kind);
    float first=0,last=1;
    const float origins[]={p.x,p.y,p.z},directions[]={d.x,d.y,d.z};
    const float lows[]={-size.x,2,-size.z},highs[]={size.x,size.y,size.z};
    for(int axis=0;axis<3;++axis){
        if(std::abs(directions[axis])<.000001f){
            if(origins[axis]<lows[axis]||origins[axis]>highs[axis])return false;
        }else{
            float a=(lows[axis]-origins[axis])/directions[axis],b=(highs[axis]-origins[axis])/directions[axis];
            if(a>b)std::swap(a,b);first=std::max(first,a);last=std::min(last,b);
            if(first>last)return false;
        }
    }
    entry=first;return true;
}
namespace {
const std::array<VehicleTuning,13> defaults{{
    {245,315,115,7.0f,0.95f,2.05f,5.5f,430,1100,70000,3.0f,0.78f,5.0f,0.43f,250000,180000,250,45,1,1,75,125,12,42,0.8f,120,7,45},
    {370,425,155,8.2f,0.75f,2.25f,7.0f,590,850,95000,3.0f,0.78f,5.0f,0.43f,250000,180000,200,45,1.15f,1,95,145,12,42,0.8f,130,7,60},
    {330,375,125,6.2f,0.90f,2.8f,8.0f,460,220,32000,3.4f,0.78f,5.5f,0.48f,250000,85000,130,50,1.3f,1,55,120,10,38,0.8f,115,7,35},
    {140,205,75,1.15f,0.80f,1.25f,2.5f,155,550,0,0,0,0,0,0,0,220,45,1,1,65,999,12,42,0.8f,120,7,0},
    {330,330,180,1.6f,0.8f,1.25f,4,500,900,0,0,0,0,0,0,400,55,1,1,140,999,12,42,0.8f,120,7,0},
    {100,135,35,6,.8f,2.8f,5,200,80,2500,4,.8f,2,.6f,15000,12000,80,65,.4f,1,10,999,12,42,.8f,120,7,0},
    {120,190,40,7,.8f,2.5f,6,260,100,3500,3,.8f,8,.5f,22000,12000,100,65,.5f,1,15,999,12,42,.8f,120,7,0},
    {100,160,60,9,1,1.3f,4,380,2500,145000,3,.8f,12,.5f,360000,250000,550,50,.7f,1,100,999,12,42,.8f,120,7,0},
    {80,125,40,8,1,1,4,380,5000,230000,3,.8f,12,.45f,550000,450000,700,50,.6f,1,150,999,12,42,.8f,120,7,0},
    {110,175,70,12,1,1.7f,5,600,9500,400000,3,.9f,8,.55f,850000,750000,1800,60,.4f,.25f,200,999,12,42,.8f,120,7,0},
    {120,240,70,8,1,1.1f,4,440,4200,260000,3,.8f,9,.45f,550000,450000,800,50,.7f,1,150,999,12,42,.8f,120,7,0},
    {1,240,70,8,1,1,4,400,2500,0,3,.8f,8,0,400000,300000,650,50,.7f,1,100,999,12,42,.8f,120,7,0},
    {550,600,1,2,.05f,1.2f,4,300,1300,0,3,.8f,5,.4f,250000,180000,450,55,2,1,150,999,12,42,.8f,120,7,0}
}};
std::array<VehicleTuning,13> entries=defaults;
std::array<float,4> surfaceGrip{1.0f,0.78f,0.62f,0.45f};
float wetGrip=0.72f;
std::string error;
}
bool load(const char* path){
    entries=defaults;error.clear();
    data_file::Ini file;
    if(!file.load(path?path:data_file::resourcePath("vehicles.ini"))||!file.version(1)){
        error=file.lastError();return false;
    }
    const char* names[]={"car","sport-car","bike","boat","helicopter","skateboard","bicycle","tractor","combine","tank","truck","trailer","airplane"};
    std::array<VehicleTuning,13> parsed{};
    for(int i=0;i<13;++i){
        std::string section=std::string("Vehicle.")+names[i];auto& v=parsed[i];
        if(!file.real(section,"Acceleration",v.acceleration,1,2000)||
           !file.real(section,"MaxSpeed",v.maxSpeed,1,2000)||
           !file.real(section,"ReverseSpeed",v.reverseSpeed,1,1000)||
           !file.real(section,"Grip",v.grip,0.01f,30)||
           !file.real(section,"Drag",v.drag,0,10)||
           !file.real(section,"TurnRate",v.turnRate,0.01f,10)||
           !file.real(section,"TurnResponse",v.turnResponse,0.01f,30)||
           !file.real(section,"Brake",v.brake,1,2000)||
           !file.real(section,"Mass",v.mass,1,10000)||
           !file.real(section,"EngineTorque",v.engineTorque,0,500000)||
           !file.real(section,"SuspensionFrequency",v.suspensionFrequency,0,20)||
           !file.real(section,"SuspensionDamping",v.suspensionDamping,0,5)||
           !file.real(section,"WheelRadius",v.wheelRadius,0,20)||
           !file.real(section,"SteerAngle",v.steerAngle,0,2)||
           !file.real(section,"BrakeTorque",v.brakeTorque,0,1000000)||
           !file.real(section,"HandbrakeTorque",v.handbrakeTorque,0,1000000)||
           !file.real(section,"MaxHealth",v.maxHealth,20,2000)||
           !file.real(section,"SmokeThreshold",v.smokeThreshold,5,95)||
           !file.real(section,"CollisionDamageScale",v.collisionDamageScale,0.01f,10)||
           !file.real(section,"BulletDamageScale",v.bulletDamageScale,0.01f,10)||
           !file.integer(section,"RepairCost",v.repairCost,0,100000)||
           !file.real(section,"DriftMinSpeed",v.driftMinSpeed,0,1000)||
           !file.real(section,"DriftMinSlip",v.driftMinSlip,0,90)||
           !file.real(section,"DriftMaxSlip",v.driftMaxSlip,0,90)||
           !file.real(section,"DriftMinDuration",v.driftMinDuration,0,10)||
           !file.real(section,"DriftMinDistance",v.driftMinDistance,0,2000)||
           !file.real(section,"DriftCooldown",v.driftCooldown,0,120)||
           !file.integer(section,"DriftReward",v.driftReward,0,10000)){
            error=file.lastError();return false;
        }
        if(v.driftMaxSlip<=v.driftMinSlip){error="Invalid ["+section+"] drift slip range";return false;}
    }
    std::array<float,4> parsedGrip{};
    const char* groundNames[]={"Asphalt","Grass","Sand","Snow"};
    for(int index=0;index<4;++index)
        if(!file.real("Traction",groundNames[index],parsedGrip[index],0.15f,1.5f)){
            error=file.lastError();return false;
        }
    float parsedWet=0;
    if(!file.real("Traction","WetMultiplier",parsedWet,0.2f,1.0f)){
        error=file.lastError();return false;
    }
    entries=parsed;surfaceGrip=parsedGrip;wetGrip=parsedWet;return true;
}
const std::string& lastError(){return error;}
VehicleTuning tuning(Kind kind){
    return entries[std::clamp(int(kind),0,12)];
}
float tractionAt(game::Vec2 point){
    float grip=surfaceGrip[0];
    switch(fire::groundAt(point)){
    case fire::Material::Grass:grip=surfaceGrip[1];break;
    case fire::Material::Sand:grip=surfaceGrip[2];break;
    case fire::Material::Snow:grip=surfaceGrip[3];break;
    default:break;
    }
    float precipitation=std::clamp(weather::current().precipitation,0.0f,1.0f);
    return grip*(1.0f-precipitation*(1.0f-wetGrip));
}
Vec2 stepVehicle(Vehicle& v,float throttle,float steering,float dt){
    const VehicleTuning t=tuning(v.kind);
    Vec2 f=forward(v.angle),right{-f.z,f.x};
    float longitudinal=v.velocity.x*f.x+v.velocity.z*f.z;
    float lateral=v.velocity.x*right.x+v.velocity.z*right.z;
    float power=std::max(0.35f,1.0f-v.damage/145.0f);
    float accel=throttle*power*t.acceleration;
    if(throttle*longitudinal<0)accel=throttle*t.brake;
    longitudinal+=accel*dt;
    longitudinal*=std::exp(-t.drag*dt*(throttle==0?2.2f:0.5f));
    longitudinal=std::clamp(longitudinal,-t.reverseSpeed,t.maxSpeed*power);
    lateral*=std::exp(-t.grip*dt);
    float speedRatio=std::min(1.0f,std::abs(longitudinal)/90.0f);
    float targetTurn=steering*t.turnRate*speedRatio*(longitudinal>=0?1.0f:-1.0f);
    v.yawRate+=(targetTurn-v.yawRate)*std::min(1.0f,t.turnResponse*dt);
    v.angle+=v.yawRate*dt;
    Vec2 newF=forward(v.angle),newRight{-newF.z,newF.x};
    v.velocity=newF*longitudinal+newRight*lateral;
    v.speed=longitudinal;
    float leanTarget=v.kind==Kind::Bike?-steering*speedRatio*0.28f:v.kind==Kind::Boat?steering*speedRatio*0.06f:0;
    v.lean+=(leanTarget-v.lean)*std::min(1.0f,dt*5.0f);
    return v.velocity*dt;
}
int updateDrift(Vehicle& v,float steering,bool handbrake,float dt){
    const VehicleTuning t=tuning(v.kind);
    v.driftCooldown=std::max(0.0f,v.driftCooldown-dt);
    if(!v.driftTracking){v.driftLastAngle=v.angle;v.driftTracking=true;}
    float yaw=std::abs(std::atan2(std::sin(v.angle-v.driftLastAngle),
        std::cos(v.angle-v.driftLastAngle)));
    v.driftLastAngle=v.angle;
    Vec2 f=forward(v.angle),side{-f.z,f.x};
    float longitudinal=v.velocity.x*f.x+v.velocity.z*f.z;
    float lateral=v.velocity.x*side.x+v.velocity.z*side.z;
    float speed=len(v.velocity);
    float slip=std::atan2(std::abs(lateral),std::max(0.1f,std::abs(longitudinal)))*180.0f/PI;
    bool controlled=!v.exploded&&v.driftCooldown<=0&&
        t.driftReward>0&&speed>=t.driftMinSpeed&&
        slip>=t.driftMinSlip&&slip<=t.driftMaxSlip&&
        std::abs(steering)>0.3f&&(handbrake||v.driftTime>0);
    if(controlled){
        v.driftTime+=dt;v.driftDistance+=speed*dt;v.driftTurn+=yaw;
        return 0;
    }
    int earned=v.driftTime>=t.driftMinDuration&&
        v.driftDistance>=t.driftMinDistance&&v.driftTurn>=0.35f?
        t.driftReward:0;
    v.driftTime=v.driftDistance=v.driftTurn=0;
    if(earned>0)v.driftCooldown=t.driftCooldown;
    return earned;
}
void stepCharacterVertical(float dt,bool jumpPressed){
    if(jumpPressed&&grounded){playerVerticalSpeed=230;grounded=false;}
    if(!grounded){
        playerVerticalSpeed-=900*dt;
        playerY+=playerVerticalSpeed*dt;
        if(playerY<=0){playerY=0;playerVerticalSpeed=0;grounded=true;}
    }
}
}
