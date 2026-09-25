#include "props.h"
#include "regions.h"
#include <algorithm>
#include <cmath>
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif

namespace game { std::vector<Prop> props; }
namespace props {
using namespace game;
namespace {
bool blocked(Vec2 p){
    if(p.x<12||p.x>WORLD_W-12||p.z<12||p.z>SHORE-12)return true;
    for(const auto& b:buildings)if(p.x>b.x-16&&p.x<b.x+b.w+16&&
        p.z>b.z-16&&p.z<b.z+b.d+16)return true;
    return false;
}
void fragments(const Prop& prop){
    if(debris.size()>110)debris.erase(debris.begin(),debris.begin()+30);
    for(int i=0;i<5;++i){float angle=i*2*PI/5;
        debris.push_back({{prop.p.x,11+prop.y,prop.p.z},
            {std::cos(angle)*75,85+float(i%3)*30,std::sin(angle)*75},2.2f,0,130.0f+float(i*28),
            5,5,5,prop.barrel?11:4});}
}
}
void reset(){
    game::props.clear();
    for(int i=0;i<22;++i){
        Vec2 p{150.0f+i*95.0f,BEACH_START+80.0f+float(i%3)*42.0f};
        if(regions::causewayAt(p))continue;
        game::props.push_back({p,{},0,0,0,0,80,i%4==0,true});
    }
    for(int i=0;i<12;++i){
        Vec2 p{355.0f+float(i%4)*450.0f,95.0f+float(i/4)*390.0f};
        if(!blocked(p))game::props.push_back({p,{},0,0,0,0,80,i%3==0,true});
    }
#ifdef MINI_CITY_JOLT
    jolt_world::reset();
#endif
}
void update(float dt){
#ifdef MINI_CITY_JOLT
    for(std::size_t index=0;index<game::props.size();++index){
        auto& prop=game::props[index];if(!prop.alive)continue;
        float radius=prop.barrel?12.0f:14.0f;
        if(occupied<0&&len(player-prop.p)<radius+11&&len(playerVelocity)>25){
            Vec2 push=norm(prop.p-player)*std::min(130.0f,len(playerVelocity))*dt*5;
            jolt_world::impulse(index,{push.x,0,push.z});
        }
        if(occupied>=0&&len(player-prop.p)<radius+22){
            Vec2 strike=vehicles[occupied].velocity;
            jolt_world::impulse(index,{strike.x*dt*5,0,strike.z*dt*5});
            if(len(strike)>90){prop.health-=int(len(strike)*dt*2);
                damageVehicle(occupied,dt*5);}
        }
        if(prop.health<=0){prop.alive=false;jolt_world::remove(index);fragments(prop);}
    }
    jolt_world::step(dt);
#else
    for(auto& prop:game::props){if(!prop.alive)continue;
        float radius=prop.barrel?12.0f:14.0f;
        if(occupied<0&&len(player-prop.p)<radius+11&&len(playerVelocity)>25)
            prop.v=prop.v+norm(prop.p-player)*std::min(130.0f,len(playerVelocity))*dt*5;
        if(occupied>=0&&len(player-prop.p)<radius+22){
            Vec2 strike=vehicles[occupied].velocity;
            prop.v=prop.v+strike*dt*5.0f;
            if(len(strike)>90){prop.health-=int(len(strike)*dt*2);
                damageVehicle(occupied,dt*5);}
        }
        prop.v=prop.v*std::exp(-2.0f*dt);
        Vec2 candidate=prop.p+prop.v*dt;
        if(blocked(candidate)){prop.v=prop.v*-0.3f;}else prop.p=candidate;
        prop.vy-=700*dt;prop.y+=prop.vy*dt;
        if(prop.y<0){prop.y=0;prop.vy=std::abs(prop.vy)*0.18f;if(prop.vy<10)prop.vy=0;}
        prop.spin*=std::exp(-3.0f*dt);prop.rotation+=prop.spin*dt;
        if(prop.health<=0){prop.alive=false;fragments(prop);}
    }
#endif
}
bool hit(Vec3 point,Vec3 impulse,int damage){
    for(auto& prop:game::props)if(prop.alive&&len(Vec2{prop.p.x-point.x,prop.p.z-point.z})<
        (prop.barrel?12.0f:14.0f)&&point.y>=prop.y&&point.y<prop.y+25){
        prop.health-=damage;
        Vec2 push=norm(Vec2{impulse.x,impulse.z});
#ifdef MINI_CITY_JOLT
        std::size_t index=static_cast<std::size_t>(&prop-game::props.data());
        jolt_world::impulse(index,{push.x*std::min(175.0f,len(impulse)*0.15f),
            std::min(150.0f,len(impulse)*0.1f),push.z*std::min(175.0f,len(impulse)*0.15f)});
#else
        prop.v=prop.v+push*std::min(175.0f,len(impulse)*0.15f);
        prop.vy+=std::min(150.0f,len(impulse)*0.1f);
#endif
        prop.spin+=90;
        if(prop.health<=0){prop.alive=false;
#ifdef MINI_CITY_JOLT
            jolt_world::remove(static_cast<std::size_t>(&prop-game::props.data()));
#endif
            fragments(prop);}
        return true;
    }
    return false;
}
}
