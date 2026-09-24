#include "camera.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace camera {
using namespace game;
Pose compute(Vec2 focus,float playerHeight,bool aiming,int occupied){
    Vec2 f=forward(cameraYaw),right{-f.z,f.x};
    float distance=aiming?95.0f:occupied>=0?175.0f:125.0f;
    float height=(aiming?52.0f:occupied>=0?85.0f:68.0f)+playerHeight;
    float shoulder=aiming?23.0f:0.0f;
    float lookDistance=aiming?240.0f:55.0f;
    float lookHeight=(aiming?16.0f+std::tan(cameraPitch)*240.0f:
        (occupied>=0?12.0f:17.0f)+std::sin(cameraPitch)*55.0f)+playerHeight;
    Vec3 anchor{focus.x,playerHeight+(occupied>=0?35.0f:25.0f),focus.z};
    Vec3 desired{focus.x-f.x*distance+right.x*shoulder,height,
        focus.z-f.z*distance+right.z*shoulder};
    Vec3 safe=desired;
    for(int i=1;i<=28;++i){
        float t=i/28.0f;
        Vec3 point=anchor+(desired-anchor)*t;
        bool blocked=point.y<5;
        for(const auto& b:buildings)if(point.x>b.x-5&&point.x<b.x+b.w+5&&
            point.z>b.z-5&&point.z<b.z+b.d+5&&point.y<b.h+5){blocked=true;break;}
        if(blocked){safe=anchor+(desired-anchor)*std::max(0.08f,(i-2)/28.0f);break;}
    }
    return {safe,{focus.x+f.x*lookDistance,lookHeight,focus.z+f.z*lookDistance}};
}
namespace {
float boxHit(Vec3 origin,Vec3 direction,Vec3 low,Vec3 high,float limit){
    float entryDistance=-std::numeric_limits<float>::infinity(),exitDistance=limit;
    const float starts[]={origin.x,origin.y,origin.z};
    const float slopes[]={direction.x,direction.y,direction.z};
    const float minimum[]={low.x,low.y,low.z};
    const float maximum[]={high.x,high.y,high.z};
    for(int axis=0;axis<3;++axis){
        if(std::abs(slopes[axis])<0.00001f){
            if(starts[axis]<minimum[axis]||starts[axis]>maximum[axis])return limit;
            continue;
        }
        float first=(minimum[axis]-starts[axis])/slopes[axis];
        float second=(maximum[axis]-starts[axis])/slopes[axis];
        if(first>second)std::swap(first,second);
        entryDistance=std::max(entryDistance,first);
        exitDistance=std::min(exitDistance,second);
        if(entryDistance>exitDistance)return limit;
    }
    float hit=entryDistance>0.001f?entryDistance:exitDistance;
    return hit>0.001f&&hit<limit?hit:limit;
}
}
Vec3 traceReticle(const Pose& pose,float maximumDistance){
    Vec3 direction=norm(pose.target-pose.eye);
    float best=maximumDistance;
    for(const auto& building:buildings)
        best=std::min(best,boxHit(pose.eye,direction,
            {building.x,0,building.z},{building.x+building.w,building.h,building.z+building.d},best));
    for(const auto& ped:peds)if(ped.alive)
        best=std::min(best,boxHit(pose.eye,direction,
            {ped.p.x-10,2,ped.p.z-10},{ped.p.x+10,37,ped.p.z+10},best));
    for(const auto& prop:props)if(prop.alive){
        float radius=prop.barrel?12.0f:14.0f;
        best=std::min(best,boxHit(pose.eye,direction,
            {prop.p.x-radius,prop.y,prop.p.z-radius},
            {prop.p.x+radius,prop.y+25,prop.p.z+radius},best));
    }
    if(activeMission>=0&&(missions[activeMission].kind==MissionKind::Targets||
       (missions[activeMission].kind==MissionKind::Finale&&missionStep==1))&&
       missionStep<int(missions[activeMission].goals.size())){
        Vec2 goal=missions[activeMission].goals[missionStep];
        best=std::min(best,boxHit(pose.eye,direction,
            {goal.x-18,0,goal.z-18},{goal.x+18,45,goal.z+18},best));
    }
    if(direction.y<-0.00001f){
        float groundDistance=-pose.eye.y/direction.y;
        if(groundDistance>=0)best=std::min(best,groundDistance);
    }
    return pose.eye+direction*best;
}
}
