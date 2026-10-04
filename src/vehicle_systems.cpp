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
               part.size.x<=0||part.size.y<=0||part.size.z<=0||part.spin<0||part.spin>4)break;
            result.push_back(std::move(part));
        }
    }
    if(int(result.size())!=count)result.clear();
    catalog.emplace(name,result);return result;
}
RagdollPart partRotation(const Vehicle& v,const Part& part){
    float angle=part.spin==1?v.wheelAngle:v.rotorAngle;
    float x=0,y=0,z=0,w=1;
    if(part.spin){w=std::cos(angle*.5f);float a=std::sin(angle*.5f);
        if(part.spin==1||part.spin==4)x=a;else if(part.spin==2)y=a;else z=a;}
    RagdollPart q{};
    q.qx=v.qw*x+v.qx*w+v.qy*z-v.qz*y;
    q.qy=v.qw*y-v.qx*z+v.qy*w+v.qz*x;
    q.qz=v.qw*z+v.qx*y-v.qy*x+v.qz*w;
    q.qw=v.qw*w-v.qx*x-v.qy*y-v.qz*z;
    return q;
}
void fireTank(){
    if(occupied<0||vehicles[occupied].kind!=Kind::Tank||fireCooldown>0||health<=0)return;
    auto& v=vehicles[occupied];Vec2 f=forward(v.angle);
    Vec3 muzzle{v.p.x+f.x*71,v.rideHeight+23,v.p.z+f.z*71};
    Vec3 direction=norm(Vec3{f.x,std::tan(std::clamp(cameraPitch,-.2f,.6f)),f.z});
    bullets.push_back({muzzle,direction*1000,3.0f,400,60,0,3000,0,false,true,120,450});
    lastMuzzle=muzzle;muzzleFlash=.18f;fireCooldown=1.2f;
    audio::playAt(audio::Effect::Shot,v.p.x,v.p.z,20);
    jolt_world::vehicleImpulse(occupied,{ -f.x*physics::tuning(v.kind).mass*20,0,-f.z*physics::tuning(v.kind).mass*20 });
    police::report(police::Crime::Gunfire,v.p,false);
}
void toggleTrailer(){
    if(occupied<0||vehicles[occupied].kind!=Kind::Truck)return;
    bool ok=jolt_world::toggleTrailer(occupied);
    announce(ok?(vehicles[occupied].trailer>=0?"TRAILER ATTACHED":"TRAILER DETACHED"):
        "Back the truck hitch close to the trailer. Stop before coupling.",3);
}
void update(float dt){
    for(auto& v:vehicles){
        v.wheelAngle=std::fmod(v.wheelAngle+dt*v.speed/std::max(1.0f,physics::tuning(v.kind).wheelRadius*physics::vehicleScale(v.kind)),PI*2);
        if(v.kind==Kind::Combine||v.kind==Kind::Airplane)
            v.rotorAngle=std::fmod(v.rotorAngle+dt*(v.exploded?0:v.kind==Kind::Airplane?45*v.flightThrottle:std::abs(v.speed)*.15f),PI*2);
    }
}
}
