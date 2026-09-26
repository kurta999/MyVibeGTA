#include "traffic.h"
#include "game_internal.h"
#include "regions.h"
#include "jolt_world.h"
#include "content.h"
#include "ped_navigation.h"
#include <limits>
#include <queue>

namespace traffic {
using namespace game;
namespace {
struct Node {Vec2 p;std::vector<int> links;};
struct Segment {Vec2 a,b;};
std::vector<Node> nodes;
float dot(Vec2 a,Vec2 b){return a.x*b.x+a.z*b.z;}
float cross(Vec2 a,Vec2 b){return a.x*b.z-a.z*b.x;}
bool validCar(int i){return i>=0&&i<int(vehicles.size());}
bool validPed(int i){return i>=0&&i<int(peds.size());}
int pedIndex(const Ped& ped){
    for(int i=0;i<int(peds.size());++i)if(&peds[i]==&ped)return i;
    return -1;
}
int node(Vec2 p){
    for(int i=0;i<int(nodes.size());++i)if(len(nodes[i].p-p)<1)return i;
    nodes.push_back({p,{}});return int(nodes.size())-1;
}
void connect(Vec2 a,Vec2 b){
    if(len(a-b)<2)return;
    int x=node(a),y=node(b);
    if(std::find(nodes[x].links.begin(),nodes[x].links.end(),y)==nodes[x].links.end()){
        nodes[x].links.push_back(y);nodes[y].links.push_back(x);
    }
}
bool roadAssigned(const Vehicle& v){
    return v.roadFrom>=0&&v.roadTo>=0&&v.roadFrom<int(nodes.size())&&v.roadTo<int(nodes.size());
}
Vec2 lanePoint(int from,int to,float progress){
    Vec2 axis=norm(nodes[to].p-nodes[from].p);
    return nodes[from].p+axis*progress+Vec2{-axis.z,axis.x}*23.0f;
}
void assignRoad(Vehicle& v,bool snap){
    float best=std::numeric_limits<float>::max();int from=-1,to=-1;Vec2 point{};
    for(int a=0;a<int(nodes.size());++a)for(int b:nodes[a].links){
        Vec2 axis=norm(nodes[b].p-nodes[a].p);
        float length=len(nodes[b].p-nodes[a].p);
        float t=std::clamp(dot(v.p-nodes[a].p,axis),0.0f,length);
        Vec2 candidate=lanePoint(a,b,t);
        float score=len(candidate-v.p)+(1-dot(forward(v.angle),axis))*25;
        if(score<best){best=score;from=a;to=b;point=candidate;}
    }
    v.roadFrom=from;v.roadTo=to;
    if(snap&&from>=0&&!solid(point,22)){
        v.p=point;Vec2 axis=nodes[to].p-nodes[from].p;v.angle=std::atan2(axis.z,axis.x);
    }
}
// Dijkstra is run only when a pursuer reaches a junction. It routes to the
// last observed position, never to an unseen player's live position.
int nextRoad(Vehicle& v,const Ped& driver){
    int at=v.roadTo;
    if(driver.grievance>0){
        int goal=0;float best=1e30f;
        for(int i=0;i<int(nodes.size());++i){float d=len(nodes[i].p-driver.lastKnown);
            if(d<best){best=d;goal=i;}}
        std::vector<float> dist(nodes.size(),1e30f);
        using Item=std::pair<float,int>;
        std::priority_queue<Item,std::vector<Item>,std::greater<Item>> open;
        dist[goal]=0;open.push({0,goal});
        while(!open.empty()){
            auto [cost,i]=open.top();open.pop();if(cost>dist[i])continue;
            for(int j:nodes[i].links){float d=cost+len(nodes[i].p-nodes[j].p);
                if(d<dist[j]){dist[j]=d;open.push({d,j});}}
        }
        int result=-1;best=1e30f;
        for(int j:nodes[at].links){
            float d=dist[j]+len(nodes[at].p-nodes[j].p)+(j==v.roadFrom?120.0f:0.0f);
            if(d<best){best=d;result=j;}}
        if(result>=0)return result;
    }
    std::vector<int> choices;
    for(int j:nodes[at].links)if(j!=v.roadFrom&&nodes[j].links.size()>1)choices.push_back(j);
    if(choices.empty())for(int j:nodes[at].links)if(j!=v.roadFrom)choices.push_back(j);
    if(choices.empty())return v.roadFrom;
    return choices[(++v.routeChoice+std::max(0,v.driver))%choices.size()];
}
bool available(int car,int who){
    if(!validCar(car)||car==occupied||car==enteringVehicle)return false;
    const auto& v=vehicles[car];
    return !v.exploded&&!v.owned&&v.burnTime<=0&&v.damage<75&&
        v.kind!=Kind::Boat&&v.kind!=Kind::Bike&&v.driver<0&&
        (v.reservedBy<0||v.reservedBy==who)&&std::abs(v.speed)<8;
}
bool exitCar(Ped& ped){
    if(!validCar(ped.drivingVehicle)){ped.drivingVehicle=-1;return true;}
    Vehicle& v=vehicles[ped.drivingVehicle];
    Vec2 side{-std::sin(v.angle),std::cos(v.angle)};
    for(float sign:{1.0f,-1.0f}){
        Vec2 out=v.p+side*(sign*44);
        if(solid(out,12))continue;
        jolt_world::driveVehicle(ped.drivingVehicle,0,0,0,true);
        v.driver=-1;v.trafficState=TrafficState::Parked;v.desiredSpeed=0;
        ped.drivingVehicle=-1;ped.p=out;ped.target=ped.lastKnown;
        ped.state=ped.hostile?PedState::Attack:PedState::Wander;
        ped.carSearchCooldown=6;return true;
    }
    return false;
}
bool visible(Vec2 from,Vec2 to){
    if(!clearLine(from,to))return false;
    float distance=len(to-from);
    for(float t=20;t<distance;t+=20)
        if(solid(from+(to-from)*(t/distance),3))return false;
    return true;
}
}

std::size_t roadNodeCount(){return nodes.size();}
void reset(){
    nodes.clear();std::vector<Segment> roads;
    for(int x=0;x<5;++x)roads.push_back({{300+x*450.0f,60},{300+x*450.0f,1540}});
    for(int z=0;z<4;++z)roads.push_back({{60,250+z*390.0f},{2340,250+z*390.0f}});
    roads.push_back({{1200,1420},{1200,1600}});
    auto city=regions::secondCity();
    for(int x=0;x<=city.columns;++x)roads.push_back({{city.x+x*city.spacing,city.z},
        {city.x+x*city.spacing,city.z+city.rows*city.spacing}});
    for(int z=0;z<=city.rows;++z)roads.push_back({{city.x,city.z+z*city.spacing},
        {city.x+city.columns*city.spacing,city.z+z*city.spacing}});
    for(const auto& road:regions::roads())roads.push_back({road.start,road.end});
    // Split crossing roads and collinear overlaps into shared junctions.
    for(const auto& road:roads){
        Vec2 axis=road.b-road.a;float length=len(axis);if(length<2)continue;
        std::vector<float> cuts{0,1};
        for(const auto& other:roads){
            Vec2 direction=other.b-other.a;float denom=cross(axis,direction);
            if(std::abs(denom)>0.01f){
                float t=cross(other.a-road.a,direction)/denom;
                float u=cross(other.a-road.a,axis)/denom;
                if(t>0&&t<1&&u>=0&&u<=1)cuts.push_back(t);
            }else for(Vec2 endpoint:{other.a,other.b}){
                float t=dot(endpoint-road.a,axis)/(length*length);
                if(t>0&&t<1&&len(road.a+axis*t-endpoint)<1)cuts.push_back(t);
            }
        }
        std::sort(cuts.begin(),cuts.end());
        for(std::size_t i=1;i<cuts.size();++i)connect(road.a+axis*cuts[i-1],road.a+axis*cuts[i]);
    }
    for(int i=0;i<int(vehicles.size());++i){
        auto& v=vehicles[i];
        bool cityCommuter=v.id.rfind("east-vehicle-",0)==0&&i%2==0&&v.kind!=Kind::Bike;
        if((v.id.rfind("traffic-",0)!=0&&!cityCommuter)||v.kind==Kind::Boat)continue;
        assignRoad(v,true);if(!roadAssigned(v))continue;
        // Lane snapping must not spawn a chassis on the player or another car.
        auto freeSpawn=[&](Vec2 point){
            if(len(point-player)<110||solid(point,22))return false;
            for(int other=0;other<int(vehicles.size());++other)
                if(other!=i&&len(vehicles[other].p-point)<65)return false;
            return true;
        };
        for(int attempt=0;attempt<24&&!freeSpawn(v.p);++attempt){
            float length=len(nodes[v.roadTo].p-nodes[v.roadFrom].p);
            Vec2 axis=norm(nodes[v.roadTo].p-nodes[v.roadFrom].p);
            float progress=dot(v.p-nodes[v.roadFrom].p,axis)+90;
            if(progress>length-35){
                Ped commuter{};int next=nextRoad(v,commuter);
                v.roadFrom=v.roadTo;v.roadTo=next;progress=60;
            }
            v.p=lanePoint(v.roadFrom,v.roadTo,progress);
            axis=nodes[v.roadTo].p-nodes[v.roadFrom].p;
            v.angle=std::atan2(axis.z,axis.x);
        }
        if(!freeSpawn(v.p))continue;
        Ped driver{};driver.id="driver-"+v.id;driver.p=v.p;driver.target=v.p;
        driver.speed=52;driver.style=i%4;driver.shirt=v.c;driver.cash=content::rollPedCash(false);
        driver.drivingVehicle=i;driver.state=PedState::Drive;
        v.driver=int(peds.size());v.trafficState=TrafficState::Cruise;
        peds.push_back(driver);
    }
}
void provoke(Ped& ped,Vec2 origin){
    if(!ped.alive||ped.police)return;
    ped.hostile=true;ped.grievance=60;ped.pursuitMemory=12;
    ped.lastKnown=origin;ped.alertTime=12;ped.sightMemory=4;
}
void damaged(int index,float amount){
    if(!validCar(index)||amount<=0||index==occupied)return;
    auto& v=vehicles[index];
    if(!validPed(v.driver)||!peds[v.driver].alive)return;
    auto& driver=peds[v.driver];
    bool fresh=driver.grievance<=0;
    provoke(driver,player);
    if(fresh){v.reactionTime=0.8f;v.trafficState=TrafficState::React;
        announce("You hit a driver's car. They are coming after you!",3);}
}
void carjacked(int index){
    if(!validCar(index))return;
    auto& v=vehicles[index];
    if(validPed(v.driver)){
        auto& ped=peds[v.driver];
        if(!exitCar(ped))return;
        provoke(ped,player);ped.state=PedState::Attack;ped.carSearchCooldown=0;
        announce("The driver wants their car back!",3);
    }
    if(validPed(v.reservedBy)){
        peds[v.reservedBy].seekingVehicle=-1;
        peds[v.reservedBy].state=PedState::Attack;
    }
    v.reservedBy=-1;v.trafficState=TrafficState::Parked;v.trafficRoute=-1;
}
void release(Ped& ped){
    ped_navigation::clear(ped);
    int who=pedIndex(ped);
    if(validCar(ped.seekingVehicle)&&vehicles[ped.seekingVehicle].reservedBy==who)
        vehicles[ped.seekingVehicle].reservedBy=-1;
    ped.seekingVehicle=-1;ped.boardingTime=0;
    if(validCar(ped.drivingVehicle)&&vehicles[ped.drivingVehicle].driver==who){
        auto& v=vehicles[ped.drivingVehicle];v.driver=-1;v.trafficState=TrafficState::Parked;
        v.desiredSpeed=0;jolt_world::driveVehicle(ped.drivingVehicle,0,0,0,true);
    }
    ped.drivingVehicle=-1;ped.grievance=0;ped.pursuitMemory=0;
}
void afterLoad(){
    // Grievances and reservations are session-local. Keep saved deaths, damage,
    // ownership and cash, but never restore a stale pursuit or a driver in a wreck.
    for(auto& v:vehicles){v.reservedBy=-1;v.reactionTime=0;v.blockedTime=0;}
    for(auto& ped:peds){
        ped_navigation::clear(ped);
        ped.seekingVehicle=-1;ped.grievance=0;ped.pursuitMemory=0;
        ped.boardingTime=0;ped.carSearchCooldown=0;
        if(!ped.police){ped.hostile=false;ped.alertTime=0;ped.panic=0;
            ped.state=ped.drivingVehicle>=0?PedState::Drive:PedState::Wander;ped.target=ped.p;}
        if(ped.drivingVehicle<0)continue;
        if(!ped.alive){release(ped);continue;}
        if(!validCar(ped.drivingVehicle)){release(ped);continue;}
        auto& v=vehicles[ped.drivingVehicle];
        if(v.exploded||v.owned||ped.drivingVehicle==occupied){exitCar(ped);continue;}
        ped.p=v.p;ped.hostile=false;ped.state=PedState::Drive;
        v.trafficState=TrafficState::Cruise;assignRoad(v,false);
    }
}
bool updatePed(Ped& ped,float dt){
    if(!ped.alive){release(ped);return false;}
    ped.carSearchCooldown=std::max(0.0f,ped.carSearchCooldown-dt);
    if(ped.grievance>0){
        ped.grievance=std::max(0.0f,ped.grievance-dt);
        bool sees=health>0&&len(player-ped.p)<650&&visible(ped.p,player);
        ped.pursuitMemory=sees?12.0f:std::max(0.0f,ped.pursuitMemory-dt);
        if(sees){ped.lastKnown=player;ped.sightMemory=4;}
        if(ped.grievance<=0||ped.pursuitMemory<=0||health<=0){
            ped.grievance=0;ped.hostile=false;ped.alertTime=0;
            if(validCar(ped.seekingVehicle))vehicles[ped.seekingVehicle].reservedBy=-1;
            ped.seekingVehicle=-1;ped.state=ped.drivingVehicle>=0?PedState::Drive:PedState::Wander;
        }else{ped.hostile=true;ped.alertTime=ped.pursuitMemory;}
    }
    if(validCar(ped.drivingVehicle)){
        auto& v=vehicles[ped.drivingVehicle];ped.p=v.p;ped.angle=v.angle;
        if(v.exploded||v.burnTime>0||v.damage>=85||v.owned||ped.drivingVehicle==occupied){
            if(exitCar(ped)){ped.state=ped.burnTime>0?PedState::Flee:PedState::Attack;return false;}
        }
        if(ped.grievance>0&&occupied<0&&len(ped.lastKnown-v.p)<95&&std::abs(v.speed)<10){
            if(exitCar(ped))return false;
        }
        return true;
    }
    if(ped.burnTime>0&&validCar(ped.seekingVehicle)){
        vehicles[ped.seekingVehicle].reservedBy=-1;ped.seekingVehicle=-1;ped.boardingTime=0;
    }
    if(ped.police||ped.grievance<=0||ped.burnTime>0||ped.knockedDown>0)return false;
    int who=pedIndex(ped);if(who<0)return false;
    if(validCar(ped.seekingVehicle)&&!available(ped.seekingVehicle,who)){
        if(vehicles[ped.seekingVehicle].reservedBy==who)vehicles[ped.seekingVehicle].reservedBy=-1;
        ped.seekingVehicle=-1;ped.state=PedState::Attack;ped.carSearchCooldown=1;
    }
    if(ped.seekingVehicle<0&&ped.carSearchCooldown<=0&&
       (occupied>=0||len(ped.lastKnown-ped.p)>65)){
        float best=360;int car=-1;
        for(int i=0;i<int(vehicles.size());++i)if(available(i,who)){
            float d=len(vehicles[i].p-ped.p);
            if(d<best&&visible(ped.p,vehicles[i].p)){best=d;car=i;}}
        ped.carSearchCooldown=2;
        if(car>=0){ped.seekingVehicle=car;vehicles[car].reservedBy=who;
            ped.state=PedState::SeekVehicle;ped.boardingTime=0;ped.tacticTimer=10;}
    }
    if(validCar(ped.seekingVehicle)){
        auto& v=vehicles[ped.seekingVehicle];
        Vec2 side{-std::sin(v.angle),std::cos(v.angle)};
        Vec2 door=v.p+side*38;
        if(solid(door,10))door=v.p-side*38;
        ped.target=door;ped.angle=std::atan2(v.p.z-ped.p.z,v.p.x-ped.p.x);
        if(len(ped.p-door)<15){
            ped.state=PedState::EnterVehicle;ped.boardingTime+=dt;
            if(ped.boardingTime>=0.75f){
                ped.drivingVehicle=ped.seekingVehicle;ped.seekingVehicle=-1;
                v.reservedBy=-1;v.driver=who;v.trafficState=TrafficState::Pursue;
                ped.p=v.p;ped.state=PedState::Drive;assignRoad(v,false);
                announce("An attacker took a car and is pursuing you!",3);
            }
        }else{
            ped.state=PedState::SeekVehicle;ped.boardingTime=0;
            jolt_world::movePed(std::size_t(who),
                ped_navigation::velocity(ped,door,ped.speed*1.7f,dt),dt);
            ped.tacticTimer-=dt;
            if(ped.tacticTimer<=0){v.reservedBy=-1;ped.seekingVehicle=-1;
                ped.state=PedState::Attack;ped.carSearchCooldown=5;}
        }
        return true;
    }
    if(!ped.armed||ped.state==PedState::SeekVehicle||ped.state==PedState::EnterVehicle)
        ped.state=PedState::Attack;
    if(!ped.armed||ped.state==PedState::Attack)ped.target=ped.lastKnown;
    return false;
}

void update(float dt){
    if(dt<=0)return;
    for(int i=0;i<int(vehicles.size());++i){
        auto& v=vehicles[i];
        if(i==occupied||i==enteringVehicle)continue;
        if(!validPed(v.driver))continue;
        auto& driver=peds[v.driver];
        if(!driver.alive){release(driver);continue;}
        if(v.exploded||v.owned||v.burnTime>0){jolt_world::driveVehicle(i,0,0,dt,true);continue;}
        if(!roadAssigned(v))assignRoad(v,false);
        if(!roadAssigned(v))continue;
        bool chasing=driver.grievance>0&&driver.hostile;
        if(len(v.p-player)>1000&&!chasing){
            v.desiredSpeed=0;jolt_world::driveVehicle(i,0,0,dt,true);continue;
        }
        v.reactionTime=std::max(0.0f,v.reactionTime-dt);
        Vec2 axis=norm(nodes[v.roadTo].p-nodes[v.roadFrom].p);
        float length=len(nodes[v.roadTo].p-nodes[v.roadFrom].p);
        float progress=dot(v.p-nodes[v.roadFrom].p,axis);
        float toJunction=len(v.p-nodes[v.roadTo].p);
        if(toJunction<45||progress>length){
            int next=nextRoad(v,driver);v.roadFrom=v.roadTo;v.roadTo=next;
            axis=norm(nodes[v.roadTo].p-nodes[v.roadFrom].p);
            length=len(nodes[v.roadTo].p-nodes[v.roadFrom].p);
            progress=dot(v.p-nodes[v.roadFrom].p,axis);
            toJunction=len(v.p-nodes[v.roadTo].p);
        }
        Vec2 target=lanePoint(v.roadFrom,v.roadTo,std::clamp(progress+65.0f,0.0f,length));
        bool sees=chasing&&driver.pursuitMemory>=11.8f;
        float distance=len(driver.lastKnown-v.p);
        // Only leave the road locally with an unobstructed path. Distant or
        // occluded targets remain graph-routed to the remembered location.
        if(chasing&&distance<200&&visible(v.p,driver.lastKnown))target=driver.lastKnown;
        Vec2 delta=target-v.p;
        float error=std::atan2(std::sin(std::atan2(delta.z,delta.x)-v.angle),
            std::cos(std::atan2(delta.z,delta.x)-v.angle));
        float desired=chasing?145.0f:78.0f+float(v.driver%4)*7;
        desired*=std::clamp(1-std::abs(error)*0.6f,0.22f,1.0f);
        if(toJunction<110&&nodes[v.roadTo].links.size()>2)desired=std::min(desired,55.0f);
        v.trafficState=chasing?(sees?TrafficState::Pursue:TrafficState::Search):TrafficState::Cruise;
        Vec2 facing=forward(v.angle);
        auto avoid=[&](Vec2 point,Vec2 velocity,float radius){
            Vec2 offset=point-v.p;float ahead=dot(offset,facing);
            if(ahead<=0||ahead>220)return;
            float arrival=std::clamp(ahead/std::max(30.0f,std::abs(v.speed)),0.0f,1.0f);
            Vec2 predicted=offset+velocity*arrival;
            if(std::min(std::abs(cross(offset,facing)),std::abs(cross(predicted,facing)))>radius)return;
            float gap=std::max(0.0f,ahead-radius-32);
            // Comfortable braking envelope plus a standstill gap.
            desired=std::min(desired,std::sqrt(2.0f*95.0f*gap));
        };
        if(!chasing||!sees||occupied<0)avoid(player,playerVelocity,occupied>=0?32.0f:20.0f);
        for(int j=0;j<int(vehicles.size());++j)if(j!=i){
            const auto& other=vehicles[j];
            if(chasing&&sees&&j==occupied)continue;
            float theirs=len(other.p-nodes[v.roadTo].p);
            bool ownPriority=toJunction<170&&nodes[v.roadTo].links.size()>2&&
                other.driver>=0&&other.roadTo==v.roadTo&&i<j&&theirs>75&&
                dot(facing,forward(other.angle))<0.8f;
            if(!ownPriority)avoid(other.p,other.velocity,42);
            // One stable winner at each intersection, with occupied junctions
            // taking priority. Queued cars never stop the vehicle clearing it.
            if(toJunction<170&&nodes[v.roadTo].links.size()>2&&other.driver>=0){
                if(theirs<65||(j<i&&other.roadTo==v.roadTo&&theirs<170))
                    if(toJunction>75)desired=0;
            }
        }
        for(const auto& ped:peds)if(ped.alive&&ped.drivingVehicle<0)
            avoid(ped.p,{},18);
        // Check the route corridor through turns. Extending the current heading
        // past a corner sees the opposite sidewalk and can stop a car forever.
        Vec2 routeDirection=norm(delta);
        for(float ahead=25;ahead<std::min(115.0f,len(delta));ahead+=15)
            if(solid(v.p+routeDirection*ahead,22)){desired=0;break;}
        if(chasing&&occupied<0&&distance<95)desired=0;
        if(v.reactionTime>0){desired=0;v.trafficState=TrafficState::React;}
        else if(desired<5)v.trafficState=TrafficState::Yield;
        v.desiredSpeed=desired;
        bool brake=desired<3||v.speed>desired+7;
        float throttle=brake?0:std::clamp((desired-v.speed)*0.035f,0.0f,0.85f);
        jolt_world::driveVehicle(i,throttle,std::clamp(error*1.7f,-1.0f,1.0f),dt,brake);
        v.blockedTime=chasing&&distance>95&&v.reactionTime<=0&&std::abs(v.speed)<3?
            v.blockedTime+dt:0;
        // No visible teleport recovery: a stuck pursuer gets out and continues
        // on foot; a commuter waits and can be carjacked.
        if(v.blockedTime>6&&chasing){exitCar(driver);v.blockedTime=0;}
        if(driver.drivingVehicle==i)driver.p=v.p;
    }
}
}
