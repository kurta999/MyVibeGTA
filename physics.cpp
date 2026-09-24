#include "physics.h"
#include <algorithm>
#include <cmath>

namespace physics {
using namespace game;
VehicleTuning tuning(Kind kind){
    switch(kind){
    case Kind::SportCar:return {370,425,155,8.2f,0.75f,2.25f,7.0f,590};
    case Kind::Bike:return {330,375,125,6.2f,0.90f,2.8f,8.0f,460};
    case Kind::Boat:return {140,205,75,1.15f,0.80f,1.25f,2.5f,155};
    default:return {245,315,115,7.0f,0.95f,2.05f,5.5f,430};
    }
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
void stepCharacterVertical(float dt,bool jumpPressed){
    if(jumpPressed&&grounded){playerVerticalSpeed=230;grounded=false;}
    if(!grounded){
        playerVerticalSpeed-=900*dt;
        playerY+=playerVerticalSpeed*dt;
        if(playerY<=0){playerY=0;playerVerticalSpeed=0;grounded=true;}
    }
}
}
