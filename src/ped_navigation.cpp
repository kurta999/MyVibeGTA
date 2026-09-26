#include "ped_navigation.h"
#include "regions.h"
#include <queue>
#include <limits>

namespace ped_navigation {
using namespace game;
namespace {
constexpr float radius=10,spacing=24;
constexpr int maxExpanded=3000,plansPerFrame=4;
int plansLeft=plansPerFrame;
float dot(Vec2 a,Vec2 b){return a.x*b.x+a.z*b.z;}
float segmentDistance(Vec2 p,Vec2 a,Vec2 b){
    Vec2 d=b-a;float t=std::clamp(dot(p-a,d)/std::max(0.001f,dot(d,d)),0.0f,1.0f);
    return len(p-(a+d*t));
}
struct Box {Vec2 center,half;float angle=0;};
struct Circle {Vec2 center;float radius;};
Vec2 local(Vec2 p,const Box& b){
    Vec2 d=p-b.center,f=forward(b.angle);return {dot(d,f),-d.x*f.z+d.z*f.x};
}
bool intersects(Vec2 a,Vec2 b,const Box& box){
    a=local(a,box);b=local(b,box);Vec2 d=b-a;float entry=0,exit=1;
    auto slab=[&](float p,float v,float half){
        if(std::abs(v)<0.0001f)return std::abs(p)<=half;
        float lo=(-half-p)/v,hi=(half-p)/v;if(lo>hi)std::swap(lo,hi);
        entry=std::max(entry,lo);exit=std::min(exit,hi);return entry<=exit;
    };
    return slab(a.x,d.x,box.half.x)&&slab(a.z,d.z,box.half.z);
}
// The same expanded geometry is used by grid edges, smoothing and steering.
// This prevents a smoothed path or a separation correction cutting a corner.
struct Obstacles {
    std::vector<Box> boxes;std::vector<Circle> circles;
    Obstacles(Vec2 from,Vec2 to){
        float x0=std::min(from.x,to.x)-400,x1=std::max(from.x,to.x)+400;
        float z0=std::min(from.z,to.z)-400,z1=std::max(from.z,to.z)+400;
        for(const auto& b:buildings){
            if(b.x>x1||b.x+b.w<x0||b.z>z1||b.z+b.d<z0)continue;
            boxes.push_back({{b.x+b.w/2,b.z+b.d/2},{b.w/2+radius+2,b.d/2+radius+2}});
        }
        for(const auto& car:vehicles){
            if(car.p.x<x0||car.p.x>x1||car.p.z<z0||car.p.z>z1)continue;
            float halfLength=car.kind==Kind::Bike?18.0f:26.0f;
            float halfWidth=car.kind==Kind::Bike?7.0f:14.0f;
            boxes.push_back({car.p,{halfLength+radius+2,halfWidth+radius+2},car.angle});
        }
        for(const auto& prop:props)if(prop.alive&&prop.y<35&&
            prop.p.x>=x0&&prop.p.x<=x1&&prop.p.z>=z0&&prop.p.z<=z1)
            circles.push_back({prop.p,(prop.barrel?12.0f:14.0f)+radius+2});
    }
    bool clear(Vec2 a,Vec2 b)const{
        for(const auto& box:boxes)if(intersects(a,b,box))return false;
        for(const auto& circle:circles)
            if(segmentDistance(circle.center,a,b)<=circle.radius)return false;
        int samples=std::max(1,int(std::ceil(len(b-a)/8)));
        for(int n=0;n<=samples;++n){
            Vec2 p=a+(b-a)*(float(n)/samples);
            if(p.x<radius||p.z<radius||p.x>regions::WIDTH-radius||p.z>regions::DEPTH-radius||
                regions::waterAt(p)||regions::waterAt(p+Vec2{radius,0})||
                regions::waterAt(p-Vec2{radius,0})||regions::waterAt(p+Vec2{0,radius})||
                regions::waterAt(p-Vec2{0,radius}))return false;
        }
        return true;
    }
};
std::vector<Vec2> search(Vec2 start,Vec2 goal,const Obstacles& obstacles){
    // A bounded local grid covers a chase and detours around city blocks.
    // Longer trips advance in stages rather than allocating a world-sized grid.
    Vec2 target=start+norm(goal-start)*std::min(960.0f,len(goal-start));
    float x0=std::floor((std::min(start.x,target.x)-288)/spacing)*spacing;
    float z0=std::floor((std::min(start.z,target.z)-288)/spacing)*spacing;
    int width=int(std::ceil((std::max(start.x,target.x)+288-x0)/spacing))+1;
    int height=int(std::ceil((std::max(start.z,target.z)+288-z0)/spacing))+1;
    int count=width*height;
    auto point=[&](int i){return Vec2{x0+(i%width)*spacing,z0+(i/width)*spacing};};
    auto cell=[&](Vec2 p){return int(std::round((p.z-z0)/spacing))*width+
        int(std::round((p.x-x0)/spacing));};
    std::vector<float> cost(count,1e30f);std::vector<int> parent(count,-1);
    std::vector<signed char> passable(count,-1);std::vector<bool> closed(count,false);
    auto openCell=[&](int i){
        if(passable[i]<0)passable[i]=obstacles.clear(point(i),point(i))?1:0;
        return passable[i]==1;
    };
    using Entry=std::pair<float,int>;
    std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;
    int center=cell(start);
    // Connect the exact start to nearby cells; never tunnel to the nearest cell.
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){
        int x=center%width+dx,z=center/width+dz;if(x<0||x>=width||z<0||z>=height)continue;
        int i=z*width+x;
        if(!openCell(i)||!obstacles.clear(start,point(i)))continue;
        cost[i]=len(start-point(i));queue.push({cost[i]+len(point(i)-target),i});
    }
    int finish=-1,best=-1,expanded=0;float bestDistance=len(start-target);
    while(!queue.empty()&&expanded<maxExpanded){
        int i=queue.top().second;queue.pop();if(closed[i])continue;
        closed[i]=true;++expanded;Vec2 p=point(i);float distance=len(p-target);
        if(distance<bestDistance){bestDistance=distance;best=i;}
        if(distance<=spacing*1.5f&&obstacles.clear(p,target)){finish=i;break;}
        for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){
            if(dx==0&&dz==0)continue;
            int x=i%width+dx,z=i/width+dz;if(x<0||x>=width||z<0||z>=height)continue;
            int j=z*width+x;if(closed[j]||!openCell(j))continue;
            // Diagonals require both adjoining cells and a swept clear edge.
            if(dx!=0&&dz!=0&&(!openCell(i+dx)||!openCell(i+dz*width)))continue;
            if(!obstacles.clear(p,point(j)))continue;
            float next=cost[i]+len(point(j)-p);
            if(next<cost[j]){cost[j]=next;parent[j]=i;queue.push({next+len(point(j)-target),j});}
        }
    }
    // If the destination is enclosed or underwater, approach only a reachable
    // point. Never fall back to walking straight through the obstacle.
    int end=finish>=0?finish:best;if(end<0)return {};
    std::vector<Vec2> path;
    for(int i=end;i>=0;i=parent[i])path.push_back(point(i));
    std::reverse(path.begin(),path.end());if(finish>=0)path.push_back(target);
    std::vector<Vec2> smooth;Vec2 from=start;
    for(std::size_t i=0;i<path.size();){
        std::size_t next=i;
        while(next+1<path.size()&&obstacles.clear(from,path[next+1]))++next;
        smooth.push_back(path[next]);from=path[next];i=next+1;
    }
    return smooth;
}
Vec2 avoidPeople(const Ped& ped,Vec2 preferred,float speed,float dt,const Obstacles& obstacles){
    if(speed<=0)return {};
    Vec2 direction=norm(preferred),separation{};
    for(const auto& other:peds){
        if(&other==&ped||!other.alive||other.drivingVehicle>=0)continue;
        Vec2 away=ped.p-other.p;float distance=len(away);
        if(distance>46)continue;
        if(distance<0.01f){
            // Stable order breaks perfectly coincident starts symmetrically.
            away=(&ped<&other)?Vec2{0,1}:Vec2{0,-1};distance=0.01f;
        }
        separation=separation+norm(away)*std::max(0.0f,(34-distance)/34)*speed;
        float ahead=dot(other.p-ped.p,direction);
        float lateral=std::abs((other.p.x-ped.p.x)*direction.z-(other.p.z-ped.p.z)*direction.x);
        if(ahead>0&&ahead<44&&lateral<22){
            Vec2 right{-direction.z,direction.x};
            separation=separation+right*speed*0.8f;
        }
    }
    Vec2 wanted=preferred+separation;
    if(len(wanted)>speed)wanted=norm(wanted)*speed;
    // Sample locally feasible velocities. Stopping is always an option. Hard
    // short-horizon clearance stops crowds from overlapping in narrow doors.
    Vec2 best{};float bestScore=1e30f;
    for(int option=0;option<17;++option){
        Vec2 candidate{};
        if(option<16){float turn=(option%8-3)*PI/8;
            Vec2 f=norm(wanted);float co=std::cos(turn),si=std::sin(turn);
            candidate={f.x*co-f.z*si,f.x*si+f.z*co};
            candidate=candidate*(len(wanted)*(option<8?1.0f:0.45f));}
        float horizon=std::max(dt,0.18f);
        Vec2 end=ped.p+candidate*horizon;
        if(!obstacles.clear(ped.p,end))continue;
        bool safe=true;
        for(const auto& other:peds){
            if(&other==&ped||!other.alive||other.drivingVehicle>=0||len(other.p-ped.p)>65)continue;
            float now=len(other.p-ped.p),next=segmentDistance(other.p,ped.p,end);
            if((now>=19&&next<19)||(now<19&&len(other.p-end)<now+0.01f&&len(candidate)>0.01f)){
                safe=false;break;
            }
        }
        for(const auto& car:vehicles)if(len(car.velocity)>8&&len(car.p-ped.p)<160){
            Vec2 relativeEnd=end-car.velocity*horizon;
            if(segmentDistance(car.p,ped.p,relativeEnd)<38){safe=false;break;}
        }
        if(!safe)continue;
        if(occupied<0&&health>0&&playerY<35){
            float now=len(player-ped.p),next=segmentDistance(player,ped.p,end);
            if((now>=19&&next<19)||(now<19&&len(player-end)<now+0.01f&&len(candidate)>0.01f))continue;
        }
        float score=len(candidate-wanted)+len(candidate-preferred)*0.25f;
        if(score<bestScore){bestScore=score;best=candidate;}
    }
    return best;
}
}
void beginFrame(){plansLeft=plansPerFrame;}
void clear(Ped& ped){ped.navigation={};}
Vec2 velocity(Ped& ped,Vec2 goal,float speed,float dt){
    if(dt<=0||speed<=0||!ped.alive||ped.drivingVehicle>=0)return {};
    auto& nav=ped.navigation;nav.repath=std::max(0.0f,nav.repath-dt);
    if(len(ped.p-nav.previous)>80)nav={}; // respawn, exit, load or teleport
    if(nav.planned&&len(ped.p-nav.previous)<speed*dt*0.08f&&len(goal-ped.p)>20)
        nav.stuck+=dt;
    else nav.stuck=0;
    nav.previous=ped.p;
    Obstacles obstacles(ped.p,goal);
    Vec2 waypoint=goal;
    if(obstacles.clear(ped.p,goal)){
        nav.path.clear();nav.next=0;nav.goal=goal;nav.planned=true;
    }else{
        while(nav.next<nav.path.size()&&len(nav.path[nav.next]-ped.p)<7)++nav.next;
        bool changed=!nav.planned||len(goal-nav.goal)>36;
        bool blocked=nav.next<nav.path.size()&&!obstacles.clear(ped.p,nav.path[nav.next]);
        if((changed||blocked||nav.next>=nav.path.size()||nav.stuck>0.7f)&&nav.repath<=0&&plansLeft>0){
            --plansLeft;nav.path=search(ped.p,goal,obstacles);nav.next=0;nav.goal=goal;
            nav.planned=true;nav.repath=0.55f;nav.stuck=0;
        }
        if(nav.next>=nav.path.size()||!obstacles.clear(ped.p,nav.path[nav.next]))return {};
        // Shorten a cached route only when the capsule has clear passage.
        while(nav.next+1<nav.path.size()&&obstacles.clear(ped.p,nav.path[nav.next+1]))++nav.next;
        waypoint=nav.path[nav.next];
    }
    float distance=len(waypoint-ped.p);
    Vec2 preferred=norm(waypoint-ped.p)*std::min(speed,distance/dt);
    return avoidPeople(ped,preferred,speed,dt,obstacles);
}
}
