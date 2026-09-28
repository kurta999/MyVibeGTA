#include "wildlife.h"
#include "game_internal.h"
#include "regions.h"
#include "audio.h"
#include "jolt_world.h"
#include "traversal.h"
#include "debug_menu.h"
#include "ui.h"
#include <random>

namespace wildlife {
using namespace game;
std::vector<Animal> animals;
namespace {
int carried=-1,active=0,mounted=-1;
// World units follow the existing 37-unit-tall pedestrian scale.
const std::vector<Species> catalog={
    {"tiger","Tiger",20,25,55,76,180,24,90,true,true,45},
    {"elephant","Elephant",58,75,105,37,650,48,180,true,false,0},
    {"cat","Cat",7,10,21,65,35,5,8,false,true,7},
    {"dog","Dog",12,20,32,70,75,12,18,true,false,0},
    {"pig","Pig",17,21,35,45,95,9,30,true,false,0},
    {"cow","Cow",26,40,60,35,210,18,65,true,false,0},
    {"capybara","Capybara",14,16,29,37,65,4,20,false,false,0},
    {"bear","Bear",33,40,64,66,310,32,110,true,true,38},
    {"goat","Goat",13,25,30,57,70,10,25,true,false,0},
    {"donkey","Donkey",20,41,49,58,160,16,45,true,false,0},
    {"roe-deer","Roe deer",14,30,34,92,75,6,30,false,false,0},
    {"deer","Deer",21,49,48,86,125,13,50,false,false,0},
    {"weasel","Weasel",4,5,17,72,22,4,6,false,true,4},
    {"beaver","Beaver",11,12,30,38,50,6,18,false,false,0},
    {"mouse","Mouse",3,4,10,52,12,1,3,false,false,0}
};
bool living(int i){return i>=0&&i<int(animals.size())&&animals[i].health>0;}
float randomUnit(Animal& a){
    // No global rand() calls: wildlife must not change traffic/mission randomness.
    float v=std::sin(a.phase*12.9898f+a.p.x*0.078f+a.p.z*0.123f)*43758.5453f;
    return v-std::floor(v);
}
void drop(){
    if(carried<0)return;
    auto& a=animals[carried];a.carried=false;
    Vec2 candidate=player+forward(cameraYaw)*(radius(a)+20);
    if(walkable(candidate,radius(a)))a.p=candidate;
    else if(walkable(player,radius(a)))a.p=player;
    else a.p=a.home;
    carried=-1;
}
void steer(Animal& a,Vec2 destination,float speed,float dt){
    Vec2 delta=destination-a.p;float distance=len(delta);
    if(distance<1)return;
    Vec2 direction=norm(delta),separation{};
    for(const auto& other:animals){
        if(&a==&other||other.carried)continue;
        float gap=len(a.p-other.p),desired=(radius(a)+radius(other))*0.75f;
        if(gap>0.1f&&gap<desired)separation=separation+norm(a.p-other.p)*((desired-gap)/desired);
    }
    direction=norm(direction+separation*1.5f);
    float step=std::min(distance,speed*dt),base=std::atan2(direction.z,direction.x);
    for(float turn:{0.0f,0.65f,-0.65f,1.3f,-1.3f}){
        Vec2 trial=a.p+forward(base+turn)*step;
        if(!bodyWalkable(a,trial,base+turn)||len(trial-a.home)>420)continue;
        a.p=trial;a.angle=base+turn;a.phase+=step*0.17f;return;
    }
    a.timer=0;
}
}
const std::vector<Species>& species(){return catalog;}
float radius(const Animal& a){return std::max(2.0f,catalog[a.species].width*0.55f);}
bool walkable(Vec2 p,float r,bool allowRoad){
    if(p.x<r||p.z<r||p.x>regions::WIDTH-r||p.z>regions::DEPTH-r||
       solid(p,r)||regions::waterAt(p)||(!allowRoad&&regions::roadAt(p)))return false;
    for(Vec2 offset:{Vec2{r,0},{-r,0},{0,r},{0,-r}})
        if(regions::waterAt(p+offset)||(!allowRoad&&regions::roadAt(p+offset)))return false;
    for(int index:regions::nearbyTreeIndices(p,r+30)){
        if(index<0||index>=int(trees.size()))continue;
        const auto& tree=trees[index];
        if(!tree.destroyed&&len(p-tree.p)<r+jolt_world::treeRadius(tree))return false;
    }
    for(const auto& prop:props)if(prop.alive&&len(p-prop.p)<r+14)return false;
    for(const auto& car:vehicles){
        Vec2 f=forward(car.angle),side{-f.z,f.x},d=p-car.p;
        float x=d.x*f.x+d.z*f.z,z=d.x*side.x+d.z*side.z;
        float halfLength=car.kind==Kind::Bike?13.0f:24.0f;
        float halfWidth=car.kind==Kind::Bike?5.0f:13.0f;
        Vec2 outside{std::max(0.0f,std::abs(x)-halfLength),std::max(0.0f,std::abs(z)-halfWidth)};
        if(len(outside)<r)return false;
    }
    return true;
}
bool bodyWalkable(const Animal& a,Vec2 p,float angle,bool allowRoad){
    float r=radius(a),halfSegment=std::max(0.0f,catalog[a.species].length*0.5f-r);
    Vec2 facing=forward(angle);
    int samples=std::max(1,int(std::ceil(2*halfSegment/r)));
    for(int i=0;i<=samples;++i){
        Vec2 point=p+facing*(-halfSegment+2*halfSegment*float(i)/samples);
        if(!walkable(point,r,allowRoad))return false;
        for(const auto& other:animals){
            if(&other==&a||other.carried||other.health<=0)continue;
            float otherR=radius(other);
            float segment=std::max(0.0f,catalog[other.species].length*0.5f-otherR);
            Vec2 axis=forward(other.angle),offset=point-other.p;
            float along=std::clamp(offset.x*axis.x+offset.z*axis.z,-segment,segment);
            if(len(offset-axis*along)<r+otherR)return false;
        }
        if(!riding()&&health>0&&occupied<0&&playerY<catalog[a.species].height&&
           len(point-player)<r+10)return false;
    }
    return true;
}
bool riding(){return mounted>=0&&mounted<int(animals.size());}
int mountedIndex(){return riding()?mounted:-1;}
namespace {
bool canMount(int i){
    if(!living(i)||riding()||health<=0||occupied>=0||enteringVehicle>=0||
       carryingBody()||traversal::active()||debug_menu::flyMode||swimming||std::abs(playerY)>8)return false;
    const auto& a=animals[i];
    return (a.species==0||a.species==1)&&
        len(player-a.p)<std::max(radius(a),catalog[a.species].length*0.5f)+30&&clearLine(player,a.p);
}
void riderPose(){
    const auto& a=animals[mounted];
    // These meshes' backs reach almost to their full height. Align the rider's
    // hip (half of the 37-unit skin) to the back, rather than burying the torso.
    player=a.p;playerY=catalog[a.species].height*(a.species==1?1.0f:0.94f)+1-18.5f;
    playerVelocity={};playerVerticalSpeed=0;airTime=0;
    swimming=false;grounded=true;crouched=false;
    jolt_world::teleportCharacter(player,playerY);
}
}
int nearbyMount(){
    int best=-1;float distance=100;
    for(int i=0;i<int(animals.size());++i)if(canMount(i)&&len(player-animals[i].p)<distance){
        best=i;distance=len(player-animals[i].p);
    }
    return best;
}
bool mount(int i){
    if(!canMount(i))return false;
    mounted=i;auto& a=animals[i];
    // Old saves placed wildlife using width-only clearance. Recover a mount
    // whose head/tail overlaps a trunk before enabling its movement controls.
    if(!bodyWalkable(a,a.p,a.angle,true)){
        Vec2 origin=a.p;bool placed=false;
        for(float distance=4;distance<=80&&!placed;distance+=4)
            for(int direction=0;direction<16;++direction){
                Vec2 candidate=origin+forward(float(direction)*2*PI/16)*distance;
                if(!bodyWalkable(a,candidate,a.angle,true))continue;
                a.p=candidate;placed=true;break;
            }
        if(!placed){mounted=-1;announce("No clear space to ride this animal.",2);return false;}
    }
    a.state=State::Idle;a.peer=-1;a.alert=0;a.playerThreat=false;a.attackTime=0;
    telescopeActive=false;cameraYaw=a.angle;riderPose();previousPlayer=player;
    announce(std::string("Riding ")+catalog[a.species].name+". WASD move, Shift run, E dismount.",4);
    return true;
}
bool dismount(bool force){
    if(!riding())return false;
    auto& a=animals[mounted];
    float clearance=std::max(radius(a),catalog[a.species].length*0.5f)+18;
    for(float turn:{PI/2,-PI/2,PI,-PI/4,PI/4,0.0f}){
        Vec2 out=a.p+forward(a.angle+turn)*clearance;
        if(!walkable(out,12,true)||!clearLine(a.p,out))continue;
        player=previousPlayer=out;playerY=0;playerVerticalSpeed=0;playerVelocity={};
        grounded=true;swimming=false;airTime=0;
        a.home=a.target=a.p;a.state=a.health>0?State::Idle:State::Dead;a.timer=2;
        mounted=-1;jolt_world::teleportCharacter(player,0);return true;
    }
    if(force){mounted=-1;grounded=false;playerVelocity={};return true;}
    announce("No clear space to dismount.",2);return false;
}
void updateRider(float dt){
    if(!riding())return;
    if(!living(mounted)||health<=0||occupied>=0||debug_menu::flyMode){dismount(true);return;}
    auto& a=animals[mounted];const auto& s=catalog[a.species];
    Vec2 facing=forward(cameraYaw),side{-facing.z,facing.x};
    Vec2 input=facing*(float(keys[ui::bindings[int(ui::Action::Forward)]])-float(keys[ui::bindings[int(ui::Action::Backward)]]))+
        side*(float(keys[ui::bindings[int(ui::Action::Right)]])-float(keys[ui::bindings[int(ui::Action::Left)]]));
    Vec2 delta=norm(input)*(s.speed*(keys[ui::bindings[int(ui::Action::Sprint)]]?1.65f:1.0f)*dt);
    Vec2 before=a.p;
    // Sweep the actual long, narrow footprint; a length-sized circle traps
    // mounts in gaps that comfortably fit their bodies.
    int steps=std::max(1,int(std::ceil(len(delta)/4)));
    for(int step=0;step<steps;++step){
        if(len(input)<0.001f)break;
        float desired=std::atan2(delta.z,delta.x);
        float turn=std::atan2(std::sin(desired-a.angle),std::cos(desired-a.angle));
        // Rotation also needs clearance, particularly for elephants.
        int turns=std::max(1,int(std::ceil(std::abs(turn)/0.08f)));
        for(int i=0;i<turns;++i){
            float nextAngle=a.angle+turn/turns;
            if(!bodyWalkable(a,a.p,nextAngle,true))break;
            a.angle=nextAngle;
        }
        Vec2 stepDelta=delta*(1.0f/steps),next=a.p+stepDelta;
        if(bodyWalkable(a,next,a.angle,true))a.p=next;
        else for(Vec2 slide:{Vec2{stepDelta.x,0},{0,stepDelta.z}})
            if(bodyWalkable(a,a.p+slide,a.angle,true))a.p=a.p+slide;
    }
    float moved=len(a.p-before);
    if(moved>0.001f)a.phase+=moved*0.17f;
    a.state=moved>0.001f?State::Wander:State::Idle;
    a.home=a.target=a.p;a.peer=-1;a.alert=0;a.playerThreat=false;a.attackTime=0;
    riderPose();
}
void reset(){
    animals.clear();carried=-1;active=0;mounted=-1;
    std::mt19937 rng(914271);std::uniform_real_distribution<float> unit(0,1);
    const float xs[]={3000,4000,5000,6000,7000},zs[]={1300,3900,6400,9000};
    for(int grove=0;grove<20;++grove)for(int slot=0;slot<8;++slot){
        Animal a;a.species=(grove*3+slot/2)%int(catalog.size());
        a.id="forest-"+std::to_string(grove)+"-"+std::to_string(slot);
        Vec2 center{xs[grove%5],zs[grove/5]};
        bool placed=false;
        for(int attempt=0;attempt<180;++attempt){
            float angle=unit(rng)*2*PI,distance=40+unit(rng)*200;
            a.p=center+forward(angle)*distance;
            a.angle=unit(rng)*2*PI;
            if(regions::biomeAt(a.p)!=regions::Biome::Countryside||!bodyWalkable(a,a.p,a.angle))continue;
            bool overlaps=false;for(const auto& other:animals)
                if(len(other.p-a.p)<radius(a)+radius(other)+5)overlaps=true;
            if(!overlaps){placed=true;break;}
        }
        if(!placed)continue;
        a.home=a.p;a.target=a.p;a.health=catalog[a.species].health;
        a.phase=unit(rng)*6;a.timer=unit(rng)*3;
        animals.push_back(a);
    }
}
void hurt(int i,int damage,Vec2 attacker,bool playerCaused,int source){
    if(!living(i)||damage<=0)return;
    auto& a=animals[i];const auto& s=catalog[a.species];
    a.health=std::max(0,a.health-damage);a.hit=0.3f;
    a.threat=attacker;a.playerThreat=playerCaused;a.peer=source;a.alert=9;
    a.state=s.defensive?State::Attack:State::Flee;
    if(!a.health){a.state=State::Dead;a.alert=0;a.peer=-1;a.attackTime=0;
        if(i==mounted)dismount(true);
        if(impacts.size()<128)impacts.push_back({a.p,8,true});}
    if(hitFlashes.size()<128)hitFlashes.push_back({{a.p.x,s.height*0.5f,a.p.z},0.18f,{0.7f,0.08f,0.05f},true});
    // Herd members see the attack and flee; they do not gain player omniscience.
    for(int j=0;j<int(animals.size());++j){auto& other=animals[j];
        if(j!=i&&j!=mounted&&other.health>0&&other.species==a.species&&len(other.p-a.p)<120){
            other.threat=attacker;other.alert=5;other.state=State::Flee;
            other.playerThreat=false;other.peer=-1;}}
}
void scare(Vec2 origin,float range){
    for(auto& a:animals)if(&a-animals.data()!=mounted&&a.health>0&&len(a.p-origin)<range&&a.state!=State::Attack){
        a.threat=origin;a.alert=4;a.state=State::Flee;a.peer=-1;a.playerThreat=false;}
}
int meleeTarget(float range){
    if(playerY>45)return -1;
    int best=-1;Vec2 facing=forward(cameraYaw);
    for(int i=0;i<int(animals.size());++i){const auto& a=animals[i];Vec2 d=a.p-player;
        float distance=len(d);
        if(a.health>0&&distance<range&&playerY<catalog[a.species].height+25&&
           facing.x*d.x+facing.z*d.z>distance*0.15f&&clearLine(player,a.p)){
            range=distance;best=i;}}
    return best;
}
bool hit(Vec3 p,int damage,Vec2 origin,bool playerCaused){
    for(int i=0;i<int(animals.size());++i){auto& a=animals[i];
        if(a.health>0&&p.y>=0&&p.y<=catalog[a.species].height&&len(Vec2{p.x,p.z}-a.p)<radius(a)){
            hurt(i,damage,origin,playerCaused);return true;}}
    return false;
}
void update(float dt){
    active=0;
    if(riding()&&(!living(mounted)||health<=0||occupied>=0))dismount(true);
    if(carried>=0){if(health<=0||occupied>=0||swimming)drop();
        else {auto& a=animals[carried];a.p=player+forward(cameraYaw)*18;a.angle=cameraYaw;}}
    for(int i=0;i<int(animals.size());++i){auto& a=animals[i];
        a.hit=std::max(0.0f,a.hit-dt);a.attackTime=std::max(0.0f,a.attackTime-dt);
        a.cooldown=std::max(0.0f,a.cooldown-dt);
        a.impactCooldown=std::max(0.0f,a.impactCooldown-dt);
        if(a.health<=0||len(a.p-player)>850)continue;
        ++active;const auto& s=catalog[a.species];
        if(i==mounted)continue;
        a.timer-=dt;a.alert=std::max(0.0f,a.alert-dt);
        if(a.state==State::Flee){
            if(a.alert<=0){a.state=State::Idle;a.timer=1;continue;}
            Vec2 escape=norm(a.p-a.threat);if(len(escape)<0.01f)escape=forward(a.angle);
            steer(a,a.p+escape*100,s.speed*1.35f,dt);continue;
        }
        if(a.state==State::Attack){
            bool validTarget=a.playerThreat?health>0&&occupied<0&&playerY<s.height+25:living(a.peer);
            Vec2 target=a.playerThreat?player:living(a.peer)?animals[a.peer].p:a.threat;
            float distance=len(target-a.p);
            if(!validTarget||distance>240||len(target-a.home)>400||a.alert<=0){
                a.state=State::Idle;a.timer=2;a.peer=-1;a.playerThreat=false;continue;}
            if(clearLine(a.p,target))a.threat=target;
            float reach=std::max(radius(a),s.length*0.5f)+
                (a.playerThreat?11:radius(animals[a.peer]))+6;
            if(distance>reach)steer(a,a.threat,s.speed,dt);
            else if(a.cooldown<=0&&clearLine(a.p,target)){
                a.angle=std::atan2(target.z-a.p.z,target.x-a.p.x);
                a.cooldown=1.2f;a.attackTime=0.35f;
                if(a.playerThreat)applyDamage(float(s.damage));
                else hurt(a.peer,s.damage,a.p,false,i);
            }
            continue;
        }
        if(a.state==State::Play){
            if(!living(a.peer)||animals[a.peer].state==State::Flee||animals[a.peer].state==State::Attack||a.timer<=0){
                a.state=State::Idle;a.timer=2;a.peer=-1;continue;}
            Vec2 center=(a.home+animals[a.peer].home)*0.5f;
            steer(a,center+forward(worldTime*1.2f+float(i%2)*PI)*35,s.speed*0.75f,dt);continue;
        }
        if(a.timer<=0){
            int prey=-1,friendIndex=-1;float nearest=140;
            for(int j=0;j<int(animals.size());++j){const auto& other=animals[j];
                if(j==i||j==mounted||other.health<=0||!clearLine(a.p,other.p))continue;
                float distance=len(other.p-a.p);
                if(distance<100&&other.species==a.species&&
                   (other.state==State::Idle||other.state==State::Wander))friendIndex=j;
                if(s.predator&&other.species!=a.species&&catalog[other.species].height<=s.preySize&&distance<nearest){
                    prey=j;nearest=distance;}
            }
            if(prey>=0){a.state=State::Attack;a.peer=prey;a.playerThreat=false;
                a.alert=10;a.threat=animals[prey].p;continue;}
            if(friendIndex>=0&&randomUnit(a)<0.55f){
                a.state=State::Play;a.peer=friendIndex;a.timer=5;
                auto& b=animals[friendIndex];b.state=State::Play;b.peer=i;b.timer=5;continue;
            }
            a.target=a.home+forward(randomUnit(a)*2*PI)*100;
            a.state=State::Wander;a.timer=3+randomUnit(a)*5;
        }
        if(a.state==State::Wander){
            if(len(a.p-a.target)<5){a.state=State::Idle;a.timer=1.5f;}
            else steer(a,a.target,s.speed*0.4f,dt);
        }
    }
}
int nearbyCorpse(bool unlooted){
    if(health<=0||occupied>=0||riding())return -1;
    int best=-1;float distance=38;
    for(int i=0;i<int(animals.size());++i){const auto& a=animals[i];float d=len(a.p-player);
        if(a.health==0&&!a.carried&&(!unlooted||!a.looted)&&d<distance&&
           std::abs(playerY)<15&&clearLine(player,a.p)){best=i;distance=d;}}
    return best;
}
void loot(int i){
    if(i<0||i>=int(animals.size()))return;
    auto& a=animals[i];if(a.health>0||a.looted||a.carried)return;
    a.looted=true;creditMoney(catalog[a.species].loot);
    announce(std::string("Looted ")+catalog[a.species].name+": supplies +$"+
        std::to_string(catalog[a.species].loot),3);
}
bool carryDrop(){
    if(carried>=0){drop();announce("Animal dropped.",2);return true;}
    int i=nearbyCorpse(false);if(i<0)return false;
    carried=i;animals[i].carried=true;
    announce(std::string("Carrying ")+catalog[animals[i].species].name+". G to drop.",3);return true;
}
bool carrying(){return carried>=0;}
void clearCarry(){carried=-1;for(auto& a:animals)a.carried=false;}
int activeCount(){return active;}
}
