#include "wildlife.h"
#include "game_internal.h"
#include "regions.h"
#include "audio.h"
#include <random>

namespace wildlife {
using namespace game;
std::vector<Animal> animals;
namespace {
int carried=-1,active=0;
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
        if(!walkable(trial,radius(a))||len(trial-a.home)>420)continue;
        a.p=trial;a.angle=base+turn;a.phase+=step*0.17f;return;
    }
    a.timer=0;
}
}
const std::vector<Species>& species(){return catalog;}
float radius(const Animal& a){return std::max(2.0f,catalog[a.species].width*0.55f);}
bool walkable(Vec2 p,float r){
    if(p.x<r||p.z<r||p.x>regions::WIDTH-r||p.z>regions::DEPTH-r||
       solid(p,r)||regions::waterAt(p)||regions::roadAt(p))return false;
    for(Vec2 offset:{Vec2{r,0},{-r,0},{0,r},{0,-r}})
        if(regions::waterAt(p+offset)||regions::roadAt(p+offset))return false;
    for(int index:regions::nearbyTreeIndices(p,r+30)){
        if(index<0||index>=int(trees.size()))continue;
        const auto& tree=trees[index];
        if(!tree.destroyed&&len(p-tree.p)<r+6*std::min(tree.scale,4.0f))return false;
    }
    for(const auto& prop:props)if(prop.alive&&len(p-prop.p)<r+14)return false;
    for(const auto& car:vehicles)if(len(p-car.p)<r+26)return false;
    return true;
}
void reset(){
    animals.clear();carried=-1;active=0;
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
            if(regions::biomeAt(a.p)!=regions::Biome::Countryside||!walkable(a.p,radius(a)))continue;
            bool overlaps=false;for(const auto& other:animals)
                if(len(other.p-a.p)<radius(a)+radius(other)+5)overlaps=true;
            if(!overlaps){placed=true;break;}
        }
        if(!placed)continue;
        a.home=a.p;a.target=a.p;a.health=catalog[a.species].health;
        a.angle=unit(rng)*2*PI;a.phase=unit(rng)*6;a.timer=unit(rng)*3;
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
        if(impacts.size()<128)impacts.push_back({a.p,8,true});}
    if(hitFlashes.size()<128)hitFlashes.push_back({{a.p.x,s.height*0.5f,a.p.z},0.18f,{0.7f,0.08f,0.05f},true});
    // Herd members see the attack and flee; they do not gain player omniscience.
    for(int j=0;j<int(animals.size());++j){auto& other=animals[j];
        if(j!=i&&other.health>0&&other.species==a.species&&len(other.p-a.p)<120){
            other.threat=attacker;other.alert=5;other.state=State::Flee;
            other.playerThreat=false;other.peer=-1;}}
}
void scare(Vec2 origin,float range){
    for(auto& a:animals)if(a.health>0&&len(a.p-origin)<range&&a.state!=State::Attack){
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
    if(carried>=0){if(health<=0||occupied>=0||swimming)drop();
        else {auto& a=animals[carried];a.p=player+forward(cameraYaw)*18;a.angle=cameraYaw;}}
    for(int i=0;i<int(animals.size());++i){auto& a=animals[i];
        a.hit=std::max(0.0f,a.hit-dt);a.attackTime=std::max(0.0f,a.attackTime-dt);
        a.cooldown=std::max(0.0f,a.cooldown-dt);
        if(a.health<=0||len(a.p-player)>850)continue;
        ++active;const auto& s=catalog[a.species];
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
            float reach=radius(a)+(a.playerThreat?11:radius(animals[a.peer]))+6;
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
                if(j==i||other.health<=0||!clearLine(a.p,other.p))continue;
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
    if(health<=0||occupied>=0)return -1;
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
