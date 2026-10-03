#include "grapple.h"
#include "camera.h"
#include "jolt_world.h"
#include "traversal.h"
#include "weapons.h"
#include "wildlife.h"
#include "debug_menu.h"
#include "ui.h"
#include "game_internal.h"
namespace grapple {
namespace {
bool attached=false,latched=false;
game::Vec3 target{},tip{},launch{},velocity{};
float remaining=0,rope=0,time=0,stuck=0;
}
void reset(){attached=latched=false;remaining=rope=time=stuck=0;velocity={};}
void release(){
    if(attached){game::playerVelocity={velocity.x,velocity.z};
        game::playerVerticalSpeed=velocity.y;remaining=0.5f;}
    attached=latched=false;
}
bool active(){return attached;}
game::Vec3 anchor(){return target;}
game::Vec3 hook(){return tip;}
float cooldown(){return remaining;}
bool fire(){
    using namespace game;
    if(attached||remaining>0||occupied>=0||enteringVehicle>=0||health<=0||
       carryingBody()||swimming||traversal::active()||wildlife::riding()||
       debug_menu::flyMode||!weapons::stats(weapon).grapple)return false;
    const auto pose=camera::compute(player,playerY,rightMouse,-1);
    Vec3 point;
    if(!jolt_world::staticAnchor(pose.eye,norm(pose.target-pose.eye),
        weapons::stats(weapon).range,point)){
        remaining=0.25f;announce("Aim the grapple at a building or tree within 700 units.",2);return false;
    }
    launch={player.x,playerY+25,player.z};
    // Check the shoulder/muzzle path as well as the reticle; no through-cover hooks.
    Vec3 visible;
    if(!jolt_world::staticAnchor(launch,norm(point-launch),len(point-launch)+3,visible)||
       len(visible-point)>18){remaining=0.25f;return false;}
    target=point;tip=launch;rope=len(target-launch);
    if(rope<35){remaining=0.25f;return false;}
    attached=true;latched=false;time=stuck=0;
    velocity={playerVelocity.x,std::max(80.0f,playerVerticalSpeed),playerVelocity.z};
    return true;
}
bool update(float dt){
    using namespace game;
    remaining=std::max(0.0f,remaining-dt);
    if(!attached)return false;
    if(!leftMouse||!weapons::stats(weapon).grapple||health<=0||occupied>=0||
       enteringVehicle>=0||traversal::active()||carryingBody()||debug_menu::flyMode){release();return false;}
    time+=dt;
    if(time>8){release();return false;}
    if(!latched){
        float travel=time*weapons::stats(weapon).projectileSpeed;
        tip=launch+norm(target-launch)*std::min(rope,travel);
        if(travel<rope)return false;
        latched=true;grounded=false;
    }
    Vec3 chest{player.x,playerY+25,player.z};Vec3 delta=target-chest;
    float distance=len(delta);
    Vec3 check;
    if(distance<30||!jolt_world::staticAnchor(chest,norm(delta),distance+4,check)||
       len(check-target)>20){release();return false;}
    Vec3 radial=norm(delta);
    rope=std::max(25.0f,rope-230*dt);
    Vec2 f=forward(cameraYaw),r{-f.z,f.x};
    Vec2 steering=f*(float(keys[ui::bindings[int(ui::Action::Forward)]])-float(keys[ui::bindings[int(ui::Action::Backward)]]))+
        r*(float(keys[ui::bindings[int(ui::Action::Right)]])-float(keys[ui::bindings[int(ui::Action::Left)]]));
    velocity=velocity+Vec3{steering.x*260,-550,steering.z*260}*dt;
    float toward=velocity.x*radial.x+velocity.y*radial.y+velocity.z*radial.z;
    float pull=std::clamp(220+(distance-rope)*8,0.0f,500.0f);
    velocity=velocity+radial*std::clamp(pull-toward,-1800*dt,1800*dt);
    float speed=len(velocity);if(speed>550)velocity=velocity*(550/speed);
    Vec3 before{player.x,playerY,player.z};
    jolt_world::moveGrappleCharacter(velocity,dt);
    Vec3 after{player.x,playerY,player.z};
    stuck=len(after-before)<0.15f?stuck+dt:0;
    // The resolved velocity prevents accumulating energy against an obstruction.
    velocity=(after-before)*(1/dt);playerVelocity={velocity.x,velocity.z};
    if(stuck>0.35f)release();
    return true;
}
}
