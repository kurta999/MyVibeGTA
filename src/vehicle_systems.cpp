#include "vehicle_systems.h"
#include "physics.h"
#include "jolt_world.h"
#include "camera.h"
#include "audio.h"
#include "game_internal.h"
#include "police.h"
#include "data_file.h"
#include <sstream>
#include <map>
namespace vehicle_systems {
using namespace game;
const char* name(Kind k){
    static const char* names[]={"CAR","SPORTS CAR","MOTORCYCLE","BOAT","HELICOPTER",
        "SKATEBOARD","BICYCLE","TRACTOR","COMBINE","TANK","TRUCK","TRAILER","AIRPLANE"};
    return names[std::clamp(int(k),0,12)];
}
bool aircraft(Kind k){return k==Kind::Helicopter||k==Kind::Airplane;}
bool pedal(Kind k){return k==Kind::Skateboard||k==Kind::Bicycle;}
std::vector<Part> parts(const Vehicle& v){
    static std::map<std::string,std::vector<Part>> catalog;
    const char* names[]={"range-rover","mazda","motorcycle","boat","helicopter","skateboard",
        "bicycle","tractor","combine","tank","truck","trailer","airplane"};
    std::string name=names[std::clamp(int(v.kind),0,12)];
    auto found=catalog.find(name);if(found!=catalog.end())return found->second;
    data_file::Ini file;std::vector<Part> result;int count=0;
    if(file.load(data_file::resourcePath("vehicle-models.ini"))&&file.integer(name,"Count",count,1,32)){
        for(int n=0;n<count;++n){std::string value;
            if(!file.string(name,"Part"+std::to_string(n),value))break;
            std::istringstream fields(value);Part part;
            if(!(fields>>part.mesh>>part.center.x>>part.center.y>>part.center.z>>part.size.x>>part.size.y>>part.size.z>>part.spin)||
               part.mesh.rfind("vehicles/expansion-",0)!=0||part.mesh.find("..")!=std::string::npos||
               !std::isfinite(part.center.x)||!std::isfinite(part.center.y)||!std::isfinite(part.center.z)||
               !std::isfinite(part.size.x)||!std::isfinite(part.size.y)||!std::isfinite(part.size.z)||
               part.size.x<=0||part.size.y<=0||part.size.z<=0||part.spin<0||part.spin>6)break;
            // Only these expanded vehicles use a runtime size correction.
            // Scale their pivots and collision/debris extents with the mesh.
            if(v.kind==Kind::Combine||v.kind==Kind::Airplane){
                float scale=physics::vehicleScale(v.kind);
                part.center=part.center*scale;part.size=part.size*scale;
            }
            result.push_back(std::move(part));
        }
    }
    if(int(result.size())!=count)result.clear();
    catalog.emplace(name,result);return result;
}
RagdollPart partRotation(const Vehicle& v,const Part& part){
    float angle=part.spin==1?v.wheelAngle:v.rotorAngle;
    float x=0,y=0,z=0,w=1;
    if(part.spin>=5){
        float sy=std::sin(v.turretYaw*.5f),cy=std::cos(v.turretYaw*.5f);
        float sx=part.spin==6?std::sin(-v.turretPitch*.5f):0;
        float cx=part.spin==6?std::cos(v.turretPitch*.5f):1;
        x=cy*sx;y=sy*cx;z=-sy*sx;w=cy*cx;
    }else if(part.spin){w=std::cos(angle*.5f);float a=std::sin(angle*.5f);
        if(part.spin==1||part.spin==4)x=a;else if(part.spin==2)y=a;else z=a;}
    RagdollPart q{};
    q.qx=v.qw*x+v.qx*w+v.qy*z-v.qz*y;
    q.qy=v.qw*y-v.qx*z+v.qy*w+v.qz*x;
    q.qz=v.qw*z+v.qx*y-v.qy*x+v.qz*w;
    q.qw=v.qw*w-v.qx*x-v.qy*y-v.qz*z;
    return q;
}
namespace {
Vec3 rotate(const RagdollPart& q,Vec3 p){
    Vec3 a{q.qx,q.qy,q.qz};
    auto cross=[](Vec3 x,Vec3 y){return Vec3{x.y*y.z-x.z*y.y,x.z*y.x-x.x*y.z,x.x*y.y-x.y*y.x};};
    Vec3 c=cross(a,p);return p+(c*q.qw+cross(a,c))*2;
}
RagdollPart hull(const Vehicle& v){RagdollPart q{};q.qx=v.qx;q.qy=v.qy;q.qz=v.qz;q.qw=v.qw;return q;}
void aimTank(Vehicle& v){
    Vec2 f=forward(cameraYaw);float cp=std::cos(cameraPitch);
    auto q=hull(v);q.qx=-q.qx;q.qy=-q.qy;q.qz=-q.qz;
    Vec3 aim=rotate(q,{f.x*cp,std::sin(cameraPitch),f.z*cp});
    v.turretYaw=std::atan2(aim.x,aim.z);
    v.turretPitch=std::clamp(std::atan2(aim.y,std::sqrt(aim.x*aim.x+aim.z*aim.z)),-.2f,.6f);
}
}
Vec3 partPosition(const Vehicle& v,const Part& part){
    Vec3 local=part.center;
    if(part.spin==6){
        for(const auto& joint:parts(v))if(joint.spin==5){
            float c=std::cos(v.turretYaw),s=std::sin(v.turretYaw);Vec3 p=local-joint.center;
            local=joint.center+Vec3{c*p.x+s*p.z,p.y,-s*p.x+c*p.z};break;
        }
    }
    return Vec3{v.p.x,v.rideHeight+physics::vehicleRestHeight(v.kind),v.p.z}+rotate(hull(v),local);
}
void fireTank(){
    if(occupied<0||occupied>=int(vehicles.size())||vehicles[occupied].kind!=Kind::Tank||fireCooldown>0||health<=0)return;
    auto& v=vehicles[occupied];aimTank(v);
    Part barrel;bool found=false;for(const auto& part:parts(v))if(part.spin==6){barrel=part;found=true;break;}
    if(!found)return;
    static float gunLength=[](){data_file::Ini file;float length=65;
        if(file.load(data_file::resourcePath("vehicle-models.ini")))file.real("tank","GunLength",length,1,150);return length;}();
    Vec3 direction=norm(rotate(partRotation(v,barrel),{0,0,1}));
    Vec3 muzzle=partPosition(v,barrel)+direction*(gunLength+2);
    bullets.push_back({muzzle,direction*1000,3.0f,400,60,0,3000,0,false,true,120,450});
    lastMuzzle=muzzle;muzzleFlash=.18f;fireCooldown=1.2f;
    audio::playAt(audio::Effect::Shot,v.p.x,v.p.z,20);
    jolt_world::vehicleImpulse(occupied,direction*(-physics::tuning(v.kind).mass*20));
    police::report(police::Crime::Gunfire,v.p,false);
}
void toggleTrailer(){
    if(occupied<0||vehicles[occupied].kind!=Kind::Truck)return;
    bool ok=jolt_world::toggleTrailer(occupied);
    announce(ok?(vehicles[occupied].trailer>=0?"TRAILER ATTACHED":"TRAILER DETACHED"):
        "Back the truck hitch close to the trailer. Stop before coupling.",3);
}
void update(float dt){
    if(occupied>=0&&occupied<int(vehicles.size())&&vehicles[occupied].kind==Kind::Tank&&!vehicles[occupied].exploded&&health>0)
        aimTank(vehicles[occupied]);
    for(auto& v:vehicles){
        v.wheelAngle=std::fmod(v.wheelAngle+dt*v.speed/std::max(1.0f,physics::tuning(v.kind).wheelRadius*physics::vehicleScale(v.kind)),PI*2);
        if(v.kind==Kind::Combine||v.kind==Kind::Airplane)
            v.rotorAngle=std::fmod(v.rotorAngle+dt*(v.exploded?0:v.kind==Kind::Airplane?45*v.flightThrottle:std::abs(v.speed)*.15f),PI*2);
    }
}
}
