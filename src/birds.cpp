#include "birds.h"
#include "regions.h"
#include <random>

namespace birds {
using namespace game;
std::vector<Bird> flock;
namespace {
const std::array<Species,5> catalog{{
    {"seagull",32,8,20,68,24},{"crow",23,9,18,62,22},
    {"sparrow",14,6,11,75,12},{"parrot",27,12,25,56,28},
    {"dove",22,8,16,60,18}
}};
float maximumCrown=0;int active=0;
bool intersects(Vec3 a,Vec3 b,Vec3 low,Vec3 high){
    float first=0,last=1;Vec3 d=b-a;
    auto axis=[&](float p,float v,float lo,float hi){
        if(std::abs(v)<0.00001f)return p>=lo&&p<=hi;
        float t0=(lo-p)/v,t1=(hi-p)/v;if(t0>t1)std::swap(t0,t1);
        first=std::max(first,t0);last=std::min(last,t1);return first<=last;
    };
    return axis(a.x,d.x,low.x,high.x)&&axis(a.y,d.y,low.y,high.y)&&axis(a.z,d.z,low.z,high.z);
}
float crown(const Tree& tree){return std::max(8.0f,tree.crownWidth*tree.scale*0.5f);}
template<class Visitor> bool obstacles(Vec3 from,Vec3 to,float r,Visitor visit){
    for(const auto& b:buildings)
        if(visit({b.x-r,-r,b.z-r},{b.x+b.w+r,b.h+r,b.z+b.d+r}))return true;
    Vec2 middle{(from.x+to.x)*0.5f,(from.z+to.z)*0.5f};
    float distance=len(Vec2{to.x-from.x,to.z-from.z})*0.5f;
    for(int index:regions::nearbyTreeIndices(middle,distance+r+maximumCrown)){
        if(index<0||index>=int(trees.size()))continue;
        const auto& tree=trees[index];if(tree.destroyed)continue;
        float width=crown(tree)+r,height=tree.height*tree.scale+r;
        if(visit({tree.p.x-width,-r,tree.p.z-width},{tree.p.x+width,height,tree.p.z+width}))return true;
    }
    for(const auto& v:vehicles){
        float width=v.kind==Kind::Bike?18.0f:40.0f;
        if(visit({v.p.x-width-r,v.rideHeight-r,v.p.z-width-r},
                 {v.p.x+width+r,v.rideHeight+44+r,v.p.z+width+r}))return true;
    }
    for(const auto& p:props)if(p.alive&&visit({p.p.x-15-r,p.y-r,p.p.z-15-r},
            {p.p.x+15+r,p.y+26+r,p.p.z+15+r}))return true;
    return false;
}
Vec3 chooseVelocity(Bird& bird){
    const auto& kind=catalog[bird.species];
    float orbit=bird.turn;
    Vec3 target=bird.home+Vec3{std::cos(orbit)*170,std::sin(orbit*0.7f)*25,std::sin(orbit)*170};
    Vec3 direction=norm(target-bird.p);
    float angle=std::atan2(direction.z,direction.x);
    float vertical=std::clamp(direction.y,-0.45f,0.45f);
    // Look several body lengths ahead, then try climbing and both turn directions.
    for(float rise:{vertical,0.65f,-0.45f})for(float turn:{0.0f,0.6f,-0.6f,1.2f,-1.2f,2.4f}){
        Vec3 candidate=norm(Vec3{std::cos(angle+turn),rise,std::sin(angle+turn)});
        if(clearFlight(bird.p,bird.p+candidate*(kind.speed*1.4f),radius(bird)))return candidate*kind.speed;
    }
    if(clearFlight(bird.p,bird.p+Vec3{0,90,0},radius(bird)))return {0,kind.speed*0.6f,0};
    return {}; // Hover if temporarily boxed in; never advance into geometry.
}
}
const std::array<Species,5>& species(){return catalog;}
float radius(const Bird& b){return catalog[b.species].span*0.52f;}
bool segmentHit(const Bird& b,Vec3 from,Vec3 to,float& entry,bool swept){
    if(b.health<=0)return false;
    const auto& kind=catalog[b.species];
    Vec2 f=forward(b.angle),side{-f.z,f.x};
    auto local=[&](Vec3 p){return Vec3{p.x*side.x+p.z*side.z,p.y,p.x*f.x+p.z*f.z};};
    from=local(from-(swept?b.previous:b.p));to=local(to-b.p);
    Vec3 d=to-from;
    // Includes every flap pose, with a small allowance for thin feathers.
    Vec3 half{kind.span*0.52f,kind.span*0.40f,kind.length*0.60f};
    float first=0,last=1;
    auto axis=[&](float p,float v,float h){
        if(std::abs(v)<0.00001f)return std::abs(p)<=h;
        float a=(-h-p)/v,c=(h-p)/v;if(a>c)std::swap(a,c);
        first=std::max(first,a);last=std::min(last,c);return first<=last;
    };
    if(!axis(from.x,d.x,half.x)||!axis(from.y,d.y,half.y)||!axis(from.z,d.z,half.z))return false;
    entry=first;return true;
}
bool clearFlight(Vec3 a,Vec3 b,float r){
    for(Vec3 p:{a,b})if(p.x<r||p.x>regions::WIDTH-r||p.z<r||p.z>regions::DEPTH-r||p.y<r||p.y>1800-r)return false;
    return !obstacles(a,b,r,[&](Vec3 low,Vec3 high){return intersects(a,b,low,high);});
}
float landingHeight(Vec3 p,float r){
    float top=r;
    obstacles(p,p,r,[&](Vec3 low,Vec3 high){
        if(p.x>=low.x&&p.x<=high.x&&p.z>=low.z&&p.z<=high.z&&high.y<=p.y+0.1f)
            top=std::max(top,high.y);
        return false;
    });
    return top;
}
void reset(){
    flock.clear();maximumCrown=0;active=0;
    for(const auto& tree:trees)if(!tree.destroyed)maximumCrown=std::max(maximumCrown,crown(tree));
    std::vector<Vec2> centers{{550,600},{1300,1100},{1600,1900},{750,250},{1500,600},
        {7900,8900},{12000,8500},{12000,14000},{4000,14000},{12000,3000}};
    for(float z:{1300.0f,3900.0f,6400.0f,9000.0f})for(float x:{3000.0f,4000.0f,5000.0f,6000.0f,7000.0f})centers.push_back({x,z});
    std::mt19937 rng(928435);std::uniform_real_distribution<float> unit(0,1);
    for(int group=0;group<int(centers.size());++group)for(int i=0;i<5;++i){
        Bird b;b.species=i;b.id="flock-"+std::to_string(group)+"-"+std::to_string(i);
        b.health=catalog[i].health;b.phase=unit(rng)*2*PI;b.turn=b.phase;
        bool placed=false;
        for(int attempt=0;attempt<100;++attempt){
            b.p={centers[group].x+(unit(rng)-0.5f)*450,90+unit(rng)*180,
                 centers[group].z+(unit(rng)-0.5f)*450};
            if(clearFlight(b.p,b.p,radius(b))){placed=true;break;}
        }
        if(!placed)continue;
        b.home=b.p;b.velocity=chooseVelocity(b);flock.push_back(b);
    }
}
void hurt(int i,int damage){
    if(i<0||i>=int(flock.size())||flock[i].health<=0||damage<=0)return;
    auto& b=flock[i];b.health=std::max(0,b.health-damage);b.hit=0.25f;
    if(!b.health){b.velocity=b.velocity*0.2f;b.velocity.y=0;b.settled=false;}
    else {b.turn+=PI;b.planTime=0;}
    if(hitFlashes.size()<128)hitFlashes.push_back({b.p,0.18f,{0.7f,0.08f,0.05f},true});
}
bool hit(Vec3 point,int damage){
    for(int i=0;i<int(flock.size());++i){const auto& b=flock[i];
        float entry=0;
        if(segmentHit(b,point,point,entry)){
            hurt(i,damage);return true;
        }
    }
    return false;
}
void update(float dt){
    active=0;
    for(auto& b:flock){
        b.previous=b.p;
        b.hit=std::max(0.0f,b.hit-dt);
        if(b.health<=0){
            if(b.settled)continue;
            // Corpses keep falling even when the player leaves the activation range.
            const float bodyRadius=2.0f;
            Vec3 next=b.p+Vec3{b.velocity.x*dt,0,b.velocity.z*dt};
            if(clearFlight(b.p,next,bodyRadius))b.p=next;
            else b.velocity.x=b.velocity.z=0;
            float floor=landingHeight(b.p,bodyRadius);
            b.velocity.y-=100*dt;b.p.y+=b.velocity.y*dt;
            b.phase+=dt*7;
            if(b.p.y<=floor){b.p.y=floor;b.velocity={};b.settled=true;}
            continue;
        }
        if(len(Vec2{b.p.x,b.p.z}-player)>1100)continue;
        ++active;b.turn+=dt*0.25f;b.planTime-=dt;
        b.phase+=dt*(b.species==2?13.0f:7.0f);
        if(b.planTime<=0){b.velocity=chooseVelocity(b);b.planTime=0.25f;}
        Vec3 next=b.p+b.velocity*dt;
        if(clearFlight(b.p,next,radius(b)))b.p=next;
        else {b.velocity={};b.planTime=0;b.turn+=0.4f;}
        if(len(Vec2{b.velocity.x,b.velocity.z})>1)b.angle=std::atan2(b.velocity.z,b.velocity.x);
    }
}
int activeCount(){return active;}
}
