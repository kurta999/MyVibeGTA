#include "ordnance.h"
#include "game_internal.h"
#include "physics.h"
#include "terrain.h"
#include "camera.h"
#include "destruction.h"
#include "fire.h"
#include "police.h"
#include "debug_menu.h"
#include "audio.h"
#include "traversal.h"
#include "regions.h"
#include <limits>

namespace ordnance {
using namespace game;
using weapons::Payload;
std::vector<Device> devices;
std::vector<Smoke> smoke;
float flash=0;
namespace {
bool enteringTimer=false;
std::string seconds="30";
bool replaceSeconds=true;
int timerWeapon=-1;
Vec3 rotate(const Vehicle& car,Vec3 v,bool inverse=false){
    Vec3 q{car.qx,car.qy,car.qz};if(inverse)q=q*-1;
    Vec3 c{q.y*v.z-q.z*v.y,q.z*v.x-q.x*v.z,q.x*v.y-q.y*v.x};
    Vec3 d{q.y*c.z-q.z*c.y,q.z*c.x-q.x*c.z,q.x*c.y-q.y*c.x};
    return v+(c*car.qw+d)*2;
}
Vec3 origin(const Vehicle& car){return {car.p.x,car.rideHeight+physics::vehicleRestHeight(car.kind),car.p.z};}
Vec3 boxNormal(Vec3 p,Vec3 low,Vec3 high){
    float distances[]={std::abs(p.x-low.x),std::abs(p.x-high.x),std::abs(p.y-low.y),
        std::abs(p.y-high.y),std::abs(p.z-low.z),std::abs(p.z-high.z)};
    Vec3 normals[]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
    int best=int(std::min_element(distances,distances+6)-distances);return normals[best];
}
Vec3 staticNormal(Vec3 p,Vec3 travel){
    if(p.y<=terrain::height({p.x,p.z})+1)return terrain::normal({p.x,p.z});
    for(const auto& b:buildings)for(const auto& box:destruction::boxes(b)){
        Vec3 nearest{std::clamp(p.x,box.low.x,box.high.x),std::clamp(p.y,box.low.y,box.high.y),
            std::clamp(p.z,box.low.z,box.high.z)};
        if(len(nearest-p)<1)return boxNormal(p,box.low,box.high);
    }
    return norm(travel)*-1;
}
bool consume(int index){
    if(index<0||index>=int(magazine.size())||!unlocked[index])return false;
    if(debug_menu::infiniteAmmo)return true;
    if(magazine[index]<=0){if(ammo[index]!=0)startReload();else announce("Out of ammo.",2);return false;}
    --magazine[index];return true;
}
void addSmoke(Vec3 p,float radius,bool dark){
    if(smoke.size()>=32)smoke.erase(smoke.begin());
    smoke.push_back({p,dark?36.0f:24.0f,radius,dark});
}
void igniteArea(Vec3 center,float radius){
    for(int x=-4;x<=4;++x)for(int z=-4;z<=4;++z){
        Vec2 p{center.x+x*radius/4,center.z+z*radius/4};
        if(!regions::waterAt(p)&&len(p-Vec2{center.x,center.z})<=radius)fire::ignite(p,fire::Material::Wood);
    }
    for(auto& ped:peds)if(ped.alive&&len(ped.p-Vec2{center.x,center.z})<radius)fire::ignitePed(ped);
    for(auto& car:vehicles)if(!car.exploded&&len(car.p-Vec2{center.x,center.z})<radius)fire::igniteVehicle(car);
}
void burst(const Device& d){
    Vec2 center{d.p.x,d.p.z};
    if(d.kind==Payload::Smoke){addSmoke(d.p,90,false);audio::playAt(audio::Effect::Water,center.x,center.z);return;}
    if(d.kind==Payload::Flashbang){
        hitFlashes.push_back({d.p,0.35f,rgb(255,255,245),false});
        for(auto& ped:peds)if(ped.alive&&len(ped.p-center)<150&&clearLine(ped.p,center))
            ped.stunRemaining=5;
        if(len(Vec3{player.x,playerY+25,player.z}-d.p)<150&&clearLine(player,center))flash=2.5f;
        audio::playAt(audio::Effect::Explosion,center.x,center.z);
        police::report(police::Crime::Gunfire,center);return;
    }
    if(d.kind==Payload::Molotov){igniteArea(d.p,65);addSmoke(d.p,45,true);
        police::report(police::Crime::Arson,center);return;}
    explodeAt(d.p,d.radius,d.damage,true);
    police::report(police::Crime::Gunfire,center);
    if(d.kind==Payload::TimedBomb){igniteArea(d.p,d.radius*.8f);addSmoke(d.p,d.radius*.6f,true);}
}
}
void reset(){devices.clear();smoke.clear();flash=0;enteringTimer=false;seconds="30";timerWeapon=-1;}
int c4Count(){return int(std::count_if(devices.begin(),devices.end(),[](const Device& d){return d.kind==Payload::C4;}));}
bool timerOpen(){return enteringTimer;}
const std::string& timerText(){return seconds;}
bool use(const weapons::Stats& stats){
    if(stats.payload==Payload::None)return false;
    if(occupied>=0||enteringVehicle>=0||swimming||playerY<0||traversal::active())return true;
    if(stats.payload==Payload::Remote){detonateRemote();fireCooldown=.4f;return true;}
    if(stats.payload==Payload::C4&&c4Count()>=MAX_C4){announce("C4 limit: 40 active charges. Detonate first.",3);fireCooldown=.4f;return true;}
    if(devices.size()>=96){announce("Too many active devices.",2);return true;}
    if(stats.payload==Payload::TimedBomb){
        if(!debug_menu::infiniteAmmo&&magazine[weapon]<=0){if(ammo[weapon]!=0)startReload();return true;}
        enteringTimer=true;seconds="30";replaceSeconds=true;timerWeapon=weapon;
        leftMouse=rightMouse=false;std::fill(std::begin(keys),std::end(keys),false);return true;
    }
    if(!consume(weapon)){fireCooldown=.35f;return true;}
    auto pose=camera::compute(player,playerY,rightMouse,occupied);
    Vec3 target=camera::traceReticle(pose,stats.range);
    Vec3 start{player.x,playerY+25,player.z};
    Vec3 direction=norm(target-start);
    // Launch from outside the capsule; sweep the hand-to-release segment too.
    Vec3 release=start+direction*17,hit{};
    if(bulletSolidSegment(start,release,hit))release=hit-direction*.3f;
    devices.push_back({stats.payload,release,direction*stats.projectileSpeed+Vec3{0,45,0},
        {0,1,0},{},{0,1,0},stats.fuseSeconds,stats.explosionRadius,stats.explosionDamage});
    fireCooldown=stats.secondsBetweenShots;shotVisualTime=.32f;
    if(stats.payload==Payload::C4){int remote=weapons::indexOf("remote-trigger");if(remote>=0)unlocked[remote]=true;}
    return true;
}
void timerKey(int key){
    if(!enteringTimer)return;
    if(key==VK_ESCAPE){enteringTimer=false;timerWeapon=-1;return;}
    if(key==VK_BACK){replaceSeconds=false;if(!seconds.empty())seconds.pop_back();return;}
    if(key>='0'&&key<='9'){if(replaceSeconds){seconds.clear();replaceSeconds=false;}if(seconds.size()<3)seconds+=char(key);return;}
    if(key>=VK_NUMPAD0&&key<=VK_NUMPAD9){timerKey('0'+key-VK_NUMPAD0);return;}
    if(key!=VK_RETURN)return;
    int delay=seconds.empty()?0:std::stoi(seconds);
    if(delay<1||delay>600){announce("Enter a timer from 1 to 600 seconds.",3);return;}
    Vec3 placement{player.x,playerY+1.5f,player.z};
    if(health<=0||occupied>=0||swimming||!grounded||devices.size()>=96){
        announce("Place a timed bomb while standing on solid ground.",3);enteringTimer=false;return;
    }
    if(!consume(timerWeapon)){enteringTimer=false;return;}
    const auto& stats=weapons::stats(timerWeapon);
    devices.push_back({Payload::TimedBomb,placement,{}, {0,1,0},{},{0,1,0},float(delay),
        stats.explosionRadius,stats.explosionDamage,true});
    enteringTimer=false;timerWeapon=-1;fireCooldown=.6f;
    announce("TIMED BOMB ARMED: "+std::to_string(delay)+" SECONDS",4);
}
void detonateRemote(){
    if(health<=0||c4Count()==0)return;
    // Remove charges before damage callbacks can alter vehicle state.
    std::vector<Device> charges;
    for(const auto& d:devices)if(d.kind==Payload::C4)charges.push_back(d);
    devices.erase(std::remove_if(devices.begin(),devices.end(),[](const Device& d){return d.kind==Payload::C4;}),devices.end());
    for(auto d:charges){
        if(d.vehicle>=0&&d.vehicle<int(vehicles.size()))d.p=origin(vehicles[d.vehicle])+rotate(vehicles[d.vehicle],d.local);
        burst(d);
    }
    announce("C4 DETONATED",3);
}
bool obscured(Vec2 start,Vec2 end){
    Vec2 delta=end-start;float squared=delta.x*delta.x+delta.z*delta.z;
    for(const auto& cloud:smoke){
        Vec2 c{cloud.p.x,cloud.p.z};float t=squared>0?
            std::clamp(((c-start).x*delta.x+(c-start).z*delta.z)/squared,0.0f,1.0f):0;
        float coverage=cloud.radius*std::min(1.0f,(cloud.dark?36.0f:24.0f)-cloud.life);
        if(cloud.life>2&&len(start+delta*t-c)<coverage)return true;
    }
    return false;
}
void update(float dt){
    flash=std::max(0.0f,flash-dt);
    for(auto& cloud:smoke)cloud.life-=dt;
    smoke.erase(std::remove_if(smoke.begin(),smoke.end(),[](const Smoke& s){return s.life<=0;}),smoke.end());
    std::vector<Device> expired;
    for(auto& d:devices){
        if(d.kind!=Payload::C4)d.fuse-=dt;
        if(d.vehicle>=0&&d.vehicle<int(vehicles.size())){
            const auto& car=vehicles[d.vehicle];
            d.p=origin(car)+rotate(car,d.local);d.normal=rotate(car,d.localNormal);
            if(car.exploded){d.vehicle=-1;d.settled=false;d.v={car.velocity.x,0,car.velocity.z};}
        }
        if(d.kind==Payload::C4&&d.settled&&d.vehicle<0&&d.p.y>1){Vec3 anchor{};
            if(!bulletSolidSegment(d.p,d.p-d.normal*1.5f,anchor))d.settled=false;
        }
        if(!d.settled){
            d.v.y-=170*dt;Vec3 end=d.p+d.v*dt,hit{};
            bool collided=bulletSolidSegment(d.p,end,hit);float best=collided?len(hit-d.p):len(end-d.p)+1;
            int vehicle=-1;float entry=0;
            for(int i=0;i<int(vehicles.size());++i)if(!vehicles[i].exploded&&
                physics::vehicleSegmentHit(vehicles[i],d.p,end,entry)&&len(end-d.p)*entry<best){
                best=len(end-d.p)*entry;hit=d.p+(end-d.p)*entry;collided=true;vehicle=i;
            }
            for(const auto& prop:props)if(prop.alive&&bulletCylinderSegment(d.p,end,prop.p,
                prop.barrel?12.0f:14.0f,prop.y,prop.y+25,entry)&&len(end-d.p)*entry<best){
                best=len(end-d.p)*entry;hit=d.p+(end-d.p)*entry;collided=true;vehicle=-1;
            }
            if(collided){
                Vec3 normal=staticNormal(hit,end-d.p);
                if(vehicle>=0){auto& car=vehicles[vehicle];Vec3 half=physics::chassisHalf(car.kind);
                    normal=rotate(car,boxNormal(rotate(car,hit-origin(car),true),half*-1,half));}
                d.p=hit+normal*.4f;d.normal=normal;
                if(d.kind==Payload::C4){
                    d.settled=true;d.v={};d.vehicle=vehicle;
                    if(vehicle>=0){d.local=rotate(vehicles[vehicle],d.p-origin(vehicles[vehicle]),true);
                        d.localNormal=rotate(vehicles[vehicle],normal,true);}
                }else if(d.kind==Payload::Molotov)d.fuse=0;
                else {float dot=d.v.x*normal.x+d.v.y*normal.y+d.v.z*normal.z;
                    d.v=(d.v-normal*(1.5f*std::min(0.0f,dot)))*.65f;
                    if(len(d.v)<18&&normal.y>.5f){d.settled=true;d.v={};}}
            }else d.p=end;
        }
        if(d.kind!=Payload::C4&&d.fuse<=0)expired.push_back(d);
    }
    devices.erase(std::remove_if(devices.begin(),devices.end(),[](const Device& d){return d.kind!=Payload::C4&&d.fuse<=0;}),devices.end());
    for(const auto& d:expired)burst(d);
}
}
