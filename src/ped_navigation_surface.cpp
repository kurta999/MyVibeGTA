#include "ped_navigation_surface.h"
#include "builder.h"
#include "destruction.h"
#include "excavation.h"
#include "jolt_world.h"
#include "physics.h"
#include "regions.h"
#include "game_internal.h"
#include <map>
#include <queue>
#include <limits>

namespace ped_navigation_surface {
using namespace game;
namespace {
constexpr float spacing=20,radius=9;
constexpr unsigned plansPerFrame=4,queriesPerFrame=18000,maxExpanded=3000,maxNodes=30000;
Stats frameStats{};
std::map<std::pair<float,float>,std::vector<float>> frameFloors;
std::uint64_t floorRevision=0;
std::uint64_t revision(){return builder::revision()^(excavation::revision()<<32);}
void currentFloors(){auto next=revision();if(next!=floorRevision){
    // Player edits can happen after beginFrame but before a threat callback.
    // Update live collision as well as floor samples before choosing a target.
    if(builder::active())jolt_world::preparePedNavigation();
    frameFloors.clear();floorRevision=next;
}}
float dot(Vec2 a,Vec2 b){return a.x*b.x+a.z*b.z;}
float segmentDistance(Vec2 p,Vec2 a,Vec2 b){auto d=b-a;float t=std::clamp(dot(p-a,d)/std::max(.001f,dot(d,d)),0.0f,1.0f);return len(p-a-d*t);}
Vec2 horizontal(Vec3 p){return {p.x,p.z};}
bool queryAvailable(){if(frameStats.physicsQueries>=queriesPerFrame)return false;++frameStats.physicsQueries;return true;}

struct Context {
    float referenceHeight;
    std::vector<BuildingPiece> boxes;
    std::map<std::pair<float,float>,std::vector<float>> floorCache;
    Context(Vec2 from,Vec2 goal,float height):referenceHeight(height){
        float x0=std::min(from.x,goal.x)-400,x1=std::max(from.x,goal.x)+400;
        float z0=std::min(from.z,goal.z)-400,z1=std::max(from.z,goal.z)+400;
        for(const auto& building:buildings)if(building.x<=x1&&building.x+building.w>=x0&&building.z<=z1&&building.z+building.d>=z0)
            for(auto box:destruction::boxes(building))boxes.push_back({box.low,box.high});
        for(const auto& block:builder::blocks()){auto low=builder::cellLow(block.first);
            if(low.x<=x1&&low.x+40>=x0&&low.z<=z1&&low.z+40>=z0)boxes.push_back({low,low+Vec3{40,40,40}});}
    }
    bool clearPoint(Vec3 p){
        if(p.x<radius||p.z<radius||p.x>regions::WIDTH-radius||p.z>regions::DEPTH-radius)return false;
        for(Vec2 offset:{Vec2{},Vec2{radius,0},Vec2{-radius,0},Vec2{0,radius},Vec2{0,-radius}})if(regions::waterAt(horizontal(p)+offset))return false;
        // Source bounds also protect routes at the edge of streamed collision.
        for(const auto& box:boxes)if(p.x+radius>box.low.x&&p.x-radius<box.high.x&&p.z+radius>box.low.z&&p.z-radius<box.high.z&&p.y+31>box.low.y+.1f&&p.y+1<box.high.y-.1f)return false;
        for(const auto& prop:props)if(prop.alive&&p.y+31>prop.y&&p.y<prop.y+(prop.barrel?25:23)&&len(horizontal(p)-prop.p)<(prop.barrel?12:14)+radius+2)return false;
        for(const auto& car:vehicles){if(p.y+31<car.rideHeight||p.y>car.rideHeight+42)continue;
            Vec2 d=horizontal(p)-car.p,f=forward(car.angle);float along=dot(d,f),across=-d.x*f.z+d.z*f.x;
            if(std::abs(along)<(car.kind==Kind::Bike?18:26)+radius+2&&std::abs(across)<(car.kind==Kind::Bike?7:14)+radius+2)return false;}
        auto known=frameFloors.find({p.x,p.z});
        if(known!=frameFloors.end())for(float floor:known->second)if(std::abs(floor-p.y)<.001f)return true;
        return queryAvailable()&&jolt_world::pedestrianClear(p);
    }
    const std::vector<float>& floors(Vec2 p){
        auto key=std::make_pair(p.x,p.z);auto found=floorCache.find(key);if(found!=floorCache.end())return found->second;
        auto raw=frameFloors.find(key);
        if(raw==frameFloors.end())raw=frameFloors.emplace(key,jolt_world::pedestrianFloors(p,&frameStats.physicsQueries,queriesPerFrame)).first;
        std::vector<float> result;
        for(float height:raw->second)if(clearPoint({p.x,height,p.z}))result.push_back(height);
        return floorCache.emplace(key,std::move(result)).first->second;
    }
    bool nearest(Vec2 p,float reference,float tolerance,Vec3& result){
        float difference=tolerance;bool found=false;
        for(float floor:floors(p)){float gap=std::abs(floor-reference);if(gap<=difference){result={p.x,floor,p.z};difference=gap;found=true;}}
        return found;
    }
    int unsupported(Vec3 p){
        int missing=0;
        constexpr float footprint=8;
        const float tolerance=5.1f+footprint*std::tan(PI*.28f);
        constexpr float diagonal=footprint*.70710678f;
        for(Vec2 offset:{Vec2{footprint,0},Vec2{-footprint,0},Vec2{0,footprint},Vec2{0,-footprint},
                        Vec2{diagonal,diagonal},Vec2{-diagonal,diagonal},Vec2{diagonal,-diagonal},Vec2{-diagonal,-diagonal}}){
            Vec3 support{};if(!nearest(horizontal(p)+offset,p.y,tolerance,support))++missing;}
        return missing;
    }
    bool clear(Vec3 from,Vec3 to){
        float distance=len(horizontal(to)-horizontal(from));int samples=std::max(1,int(std::ceil(distance/4)));
        Vec3 previous=from;if(!clearPoint(from))return false;int missing=unsupported(from);
        for(int n=1;n<=samples;++n){Vec2 point=horizontal(from)+(horizontal(to)-horizontal(from))*(float(n)/samples);Vec3 next{};
            if(!nearest(point,previous.y,5.1f,next)||!clearPoint(next))return false;
            int nextMissing=unsupported(next);if(nextMissing>missing)return false;missing=nextMissing;previous=next;}
        return std::abs(previous.y-to.y)<2.5f;
    }
    bool clear(Vec2 from,Vec2 to){Vec3 a{},b{};float tolerance=5.1f+len(to-from)*std::tan(PI*.28f);
        return nearest(from,referenceHeight,2.5f,a)&&nearest(to,referenceHeight,tolerance,b)&&clear(a,b);}
};

std::vector<Vec3> search(Vec3 start,Vec3 goal,Context& context){
    float distance=len(horizontal(goal)-horizontal(start));Vec3 target=goal;
    if(distance>960){auto p=horizontal(start)+norm(horizontal(goal)-horizontal(start))*960;
        if(!context.nearest(p,start.y,4000,target))return {};}
    float x0=std::floor((std::min(start.x,target.x)-288)/spacing)*spacing,z0=std::floor((std::min(start.z,target.z)-288)/spacing)*spacing;
    int width=int(std::ceil((std::max(start.x,target.x)+288-x0)/spacing))+1,height=int(std::ceil((std::max(start.z,target.z)+288-z0)/spacing))+1;
    struct Node {Vec3 p;float cost=1e30f;int parent=-1;bool closed=false;};
    std::vector<Node> nodes;std::map<int,std::vector<int>> columns;
    auto point=[&](int cell){return Vec2{x0+(cell%width)*spacing,z0+(cell/width)*spacing};};
    auto column=[&](int cell)->const std::vector<int>& {auto found=columns.find(cell);if(found!=columns.end())return found->second;
        std::vector<int> indices;auto p=point(cell);if(cell>=0&&cell<width*height)for(float floor:context.floors(p)){
            if(context.unsupported({p.x,floor,p.z})!=0)continue;
            if(nodes.size()>=maxNodes)break;indices.push_back(int(nodes.size()));nodes.push_back({{p.x,floor,p.z}});}
        return columns.emplace(cell,std::move(indices)).first->second;};
    using Entry=std::pair<float,int>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;
    int center=int(std::round((start.z-z0)/spacing))*width+int(std::round((start.x-x0)/spacing));
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){int x=center%width+dx,z=center/width+dz;if(x<0||z<0||x>=width||z>=height)continue;
        for(int i:column(z*width+x))if(context.clear(start,nodes[i].p)){nodes[i].cost=len(nodes[i].p-start);queue.push({nodes[i].cost+len(nodes[i].p-target),i});}}
    int finish=-1,best=-1;unsigned expanded=0;float bestDistance=len(start-target);
    while(!queue.empty()&&expanded<maxExpanded&&frameStats.physicsQueries<queriesPerFrame){
        int i=queue.top().second;queue.pop();if(nodes[i].closed)continue;nodes[i].closed=true;++expanded;++frameStats.expanded;
        auto p=nodes[i].p;float remaining=len(p-target);if(remaining<bestDistance){bestDistance=remaining;best=i;}
        if(remaining<=spacing*1.5f&&context.clear(p,target)){finish=i;break;}
        int cell=int(std::round((p.z-z0)/spacing))*width+int(std::round((p.x-x0)/spacing));
        for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){if(dx==0&&dz==0)continue;
            int x=cell%width+dx,z=cell/width+dz;if(x<0||z<0||x>=width||z>=height)continue;
            for(int j:column(z*width+x)){if(nodes[j].closed||std::abs(nodes[j].p.y-p.y)>spacing*2)continue;
                if(!context.clear(p,nodes[j].p))continue;float cost=nodes[i].cost+len(nodes[j].p-p);
                if(cost<nodes[j].cost){nodes[j].cost=cost;nodes[j].parent=i;queue.push({cost+len(nodes[j].p-target),j});}}}
    }
    int end=finish>=0?finish:best;if(end<0)return {};
    std::vector<Vec3> path;for(int i=end;i>=0;i=nodes[i].parent)path.push_back(nodes[i].p);std::reverse(path.begin(),path.end());if(finish>=0)path.push_back(target);
    std::vector<Vec3> smooth;Vec3 from=start;
    for(std::size_t i=0;i<path.size();){std::size_t next=i;while(next+1<path.size()&&context.clear(from,path[next+1]))++next;
        smooth.push_back(path[next]);from=path[next];i=next+1;}return smooth;
}

Vec2 steer(Ped& ped,Vec2 preferred,float speed,float dt,Context& context){
    Vec2 wanted=preferred,separation{};float height=jolt_world::pedHeight(ped);
    for(const auto& other:peds){if(&other==&ped||!other.alive||other.drivingVehicle>=0||std::abs(jolt_world::pedHeight(other)-height)>=25)continue;
        auto away=ped.p-other.p;float distance=len(away);if(distance>46)continue;
        if(distance<.01f){away=&ped<&other?Vec2{0,1}:Vec2{0,-1};distance=.01f;}
        separation=separation+norm(away)*std::max(0.0f,(34-distance)/34)*speed;}
    wanted=wanted+separation;if(len(wanted)>speed)wanted=norm(wanted)*speed;
    Vec2 best{};float bestScore=1e30f;
    // Test the intended velocity first; a perfect safe match needs no alternatives.
    constexpr int order[]={3,2,4,1,5,0,6,7,11,10,12,9,13,8,14,15,16};
    for(int option:order){Vec2 candidate{};
        if(option<16){float turn=(option%8-3)*PI/8;auto f=norm(wanted);candidate={f.x*std::cos(turn)-f.z*std::sin(turn),f.x*std::sin(turn)+f.z*std::cos(turn)};
            candidate=candidate*(len(wanted)*(option<8?1.0f:.45f));}
        float horizon=std::max(dt,.18f);auto end=ped.p+candidate*horizon;if(!context.clear(ped.p,end))continue;bool safe=true;
        for(const auto& other:peds){if(&other==&ped||!other.alive||other.drivingVehicle>=0||std::abs(jolt_world::pedHeight(other)-height)>=25||len(other.p-ped.p)>65)continue;
            float now=len(other.p-ped.p),next=segmentDistance(other.p,ped.p,end);
            if((now>=19&&next<19)||(now<19&&len(other.p-end)<now+.01f&&len(candidate)>.01f)){safe=false;break;}}
        if(!safe)continue;
        for(const auto& car:vehicles)if(std::abs(car.rideHeight-height)<42&&len(car.velocity)>8&&len(car.p-ped.p)<160&&
            segmentDistance(car.p,ped.p,end-car.velocity*horizon)<physics::vehicleRadius(car.kind)+12){safe=false;break;}
        if(!safe)continue;
        if(occupied<0&&health>0&&std::abs(playerY-height)<25){float now=len(player-ped.p),next=segmentDistance(player,ped.p,end);
            if((now>=19&&next<19)||(now<19&&len(player-end)<now+.01f&&len(candidate)>.01f))continue;}
        float score=len(candidate-wanted)+len(candidate-preferred)*.25f;if(score<bestScore){bestScore=score;best=candidate;}
        if(score<.001f)return candidate;}
    return best;
}
}
void beginFrame(){frameStats={};frameFloors.clear();floorRevision=revision();if(builder::active())jolt_world::preparePedNavigation();}
const Stats& stats(){return frameStats;}
bool hasDestination(const Ped& ped){
    const auto& nav=ped.navigation;
    return builder::active()&&nav.destinationSelected&&nav.surfaceMode&&nav.surfaceRevision==revision()&&
        len(nav.goal-ped.target)<.01f&&std::abs(jolt_world::pedHeight(ped)-nav.previousHeight)<20;
}
bool supported(Vec2 point,float referenceHeight,Vec3& feet,float tolerance){
    if(!builder::active()||len(point-player)>1200||frameStats.physicsQueries>=queriesPerFrame)return false;
    currentFloors();Context context(point,point,referenceHeight);
    return context.nearest(point,referenceHeight,tolerance,feet)&&context.unsupported(feet)==0&&
        queryAvailable()&&jolt_world::pedestrianClear(feet,true);
}
bool reachable(Vec3 from,Vec3 to){
    if(!builder::active()||len(horizontal(from)-player)>1200||len(horizontal(to)-player)>1200||
       frameStats.plans>=plansPerFrame||frameStats.physicsQueries>=queriesPerFrame)return false;
    currentFloors();Context context(horizontal(from),horizontal(to),from.y);Vec3 start{},end{};
    if(!context.nearest(horizontal(from),from.y,2.5f,start)||!context.nearest(horizontal(to),to.y,2.5f,end)||
       context.unsupported(start)!=0||context.unsupported(end)!=0)return false;
    ++frameStats.plans;
    auto path=context.clear(start,end)?std::vector<Vec3>{end}:search(start,end,context);
    return !path.empty()&&len(path.back()-end)<2.5f;
}
bool destination(Ped& ped,Vec2 intended,Vec3& feet,const Vec3* threatEye){
    if(!builder::active()||!ped.alive||ped.speed<=0||ped.drivingVehicle>=0||len(ped.p-player)>1200||
       frameStats.plans>=plansPerFrame||frameStats.physicsQueries>=queriesPerFrame)return false;
    currentFloors();float height=jolt_world::pedHeight(ped);Context context(ped.p,intended,height);Vec3 start{},goal{};
    if(!context.nearest(ped.p,height,2.5f,start))return false;
    bool goalFloor=context.nearest(intended,height,4000,goal);
    auto concealed=[&](Vec3 p){return !threatEye||
        (!clearLineAtHeight(*threatEye,p+Vec3{0,10,0})&&!clearLineAtHeight(*threatEye,p+Vec3{0,28,0}));};
    // Do cheap floor/cover rejection before spending a route-plan slot.
    if(threatEye&&(!goalFloor||context.unsupported(goal)!=0||!concealed(goal)))return false;
    if(!goalFloor)goal={intended.x,height,intended.z};
    ++frameStats.plans;
    auto path=goalFloor&&context.unsupported(goal)==0&&context.clear(start,goal)?std::vector<Vec3>{goal}:search(start,goal,context);
    if(path.empty())return false;auto end=path.back();
    if((!threatEye&&len(horizontal(end)-ped.p)<8)||context.unsupported(end)!=0||!concealed(end))return false;
    // search() may return only a safe approach to an unreachable intent. Make
    // that supported endpoint the actual AI target, never the inaccessible one.
    auto& nav=ped.navigation;nav={};nav.surfaceMode=true;nav.surfaceRevision=revision();nav.destinationSelected=true;
    nav.surfacePath=std::move(path);nav.goal=horizontal(end);nav.goalHeight=end.y;nav.previous=ped.p;nav.previousHeight=height;
    nav.planned=true;nav.repath=.55f;feet=end;return true;
}
Vec2 velocity(Ped& ped,Vec2 goal,float goalHeight,float speed,float dt){
    if(dt<=0||speed<=0||!ped.alive||ped.drivingVehicle>=0||len(ped.p-player)>1200)return {};
    auto& nav=ped.navigation;float height=jolt_world::pedHeight(ped);
    currentFloors();auto currentRevision=revision();
    if(!nav.surfaceMode||nav.surfaceRevision!=currentRevision||len(ped.p-nav.previous)>80||std::abs(height-nav.previousHeight)>20){nav={};nav.surfaceMode=true;nav.surfaceRevision=currentRevision;}
    nav.repath=std::max(0.0f,nav.repath-dt);
    if(nav.planned&&len(ped.p-nav.previous)<speed*dt*.08f&&len(goal-ped.p)>20)nav.stuck+=dt;else nav.stuck=0;
    nav.previous=ped.p;nav.previousHeight=height;
    Context context(ped.p,goal,height);Vec3 start{},destination{};
    if(!context.nearest(ped.p,height,2.5f,start))return {}; // Let the real capsule finish falling first.
    float desired=std::isfinite(goalHeight)?goalHeight:
        nav.destinationSelected&&len(goal-nav.goal)<.01f?nav.goalHeight:height;
    if(!context.nearest(goal,desired,4000,destination))return {};
    while(nav.next<nav.surfacePath.size()&&len(horizontal(nav.surfacePath[nav.next])-ped.p)<(nav.next+1==nav.surfacePath.size()?2.0f:7.0f)&&std::abs(nav.surfacePath[nav.next].y-height)<5)++nav.next;
    bool changed=!nav.planned||len(goal-nav.goal)>36||std::abs(destination.y-nav.goalHeight)>5;
    bool arrived=len(horizontal(destination)-ped.p)<2&&std::abs(destination.y-height)<2.5f;
    if(arrived)return {};
    if((changed||nav.next>=nav.surfacePath.size()||nav.stuck>.7f)&&nav.repath<=0&&frameStats.plans<plansPerFrame){
        ++frameStats.plans;
        nav.surfacePath=context.clear(start,destination)?std::vector<Vec3>{destination}:search(start,destination,context);
        nav.next=0;nav.goal=goal;nav.goalHeight=destination.y;nav.planned=true;nav.repath=.55f;nav.stuck=0;
    }
    if(nav.next>=nav.surfacePath.size())return {};
    // Static edits invalidate the route through its revision. Moving obstacles
    // are checked by the short real-capsule steering sweep each frame; a stuck
    // walker replans instead of rescanning its entire route on every tick.
    Vec3 waypoint=nav.surfacePath[nav.next];
    float distance=len(horizontal(waypoint)-ped.p);auto preferred=norm(horizontal(waypoint)-ped.p)*std::min(speed,distance/dt);
    return steer(ped,preferred,speed,dt,context);
}
}
