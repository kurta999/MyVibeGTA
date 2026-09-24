#include "physics.h"
#include "data_file.h"
#include "fire.h"
#include "weather.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace physics {
using namespace game;
namespace {
const std::array<VehicleTuning,4> defaults{{
    {245,315,115,7.0f,0.95f,2.05f,5.5f,430,1100,70000,3.0f,0.78f,5.0f,0.43f,250000,180000,250,45,1,1,75,125,12,42,0.8f,120,7,45},
    {370,425,155,8.2f,0.75f,2.25f,7.0f,590,850,95000,3.0f,0.78f,5.0f,0.43f,250000,180000,200,45,1.15f,1,95,145,12,42,0.8f,130,7,60},
    {330,375,125,6.2f,0.90f,2.8f,8.0f,460,220,32000,3.4f,0.78f,5.5f,0.48f,250000,85000,130,50,1.3f,1,55,120,10,38,0.8f,115,7,35},
    {140,205,75,1.15f,0.80f,1.25f,2.5f,155,550,0,0,0,0,0,0,0,220,45,1,1,65,999,12,42,0.8f,120,7,0}
}};
std::array<VehicleTuning,4> entries=defaults;
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
    const char* names[]={"car","sport-car","bike","boat"};
    std::array<VehicleTuning,4> parsed{};
    for(int i=0;i<4;++i){
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
    return entries[std::clamp(int(kind),0,3)];
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
