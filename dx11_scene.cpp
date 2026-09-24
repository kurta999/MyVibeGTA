#include "dx11_assets.h"
#include "game.h"
#include "ui.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace dx11 {
namespace {
using game::Color;using game::Vec2;using game::Vec3;using game::RagdollPart;
std::vector<Vertex>* buckets=nullptr;
std::vector<ModelInstance>* modelInstances=nullptr;
Vertex vertex(Vec3 p,Vec3 n,float u,float v,Color c){return {p.x,p.y,p.z,n.x,n.y,n.z,u,v,c.r,c.g,c.b,1};}
void triangle(int group,Vertex a,Vertex b,Vertex c){
    buckets[group].push_back(a);buckets[group].push_back(b);buckets[group].push_back(c);
}
void quad(int group,Vec3 a,Vec3 b,Vec3 c,Vec3 d,Vec3 normal,Color tint,int tile=-1){
    float u0=0,v0=0,u1=1,v1=1;
    if(tile>=0){group=1;u0=(tile%4)/4.0f;v0=(tile/4)/4.0f;
        u1=u0+0.25f;v1=v0+0.25f;}
    triangle(group,vertex(a,normal,u0,v1,tint),vertex(b,normal,u1,v1,tint),vertex(c,normal,u1,v0,tint));
    triangle(group,vertex(a,normal,u0,v1,tint),vertex(c,normal,u1,v0,tint),vertex(d,normal,u0,v0,tint));
}
void ground(float x0,float z0,float x1,float z1,float y,Color tint,int tile=-1,int material=0){
    quad(tile<0?material:1,{x0,y,z0},{x1,y,z0},{x1,y,z1},{x0,y,z1},{0,1,0},tint,tile);
}
void box(float x,float y,float z,float w,float h,float d,Color tint,int tile=-1){
    float a=x-w/2,b=x+w/2,c=z-d/2,e=z+d/2,t=y+h;
    quad(0,{a,y,c},{b,y,c},{b,t,c},{a,t,c},{0,0,-1},tint,tile);
    quad(0,{b,y,e},{a,y,e},{a,t,e},{b,t,e},{0,0,1},tint,tile);
    quad(0,{a,y,e},{a,y,c},{a,t,c},{a,t,e},{-1,0,0},tint,tile);
    quad(0,{b,y,c},{b,y,e},{b,t,e},{b,t,c},{1,0,0},tint,tile);
    quad(0,{a,t,c},{b,t,c},{b,t,e},{a,t,e},{0,1,0},tint,tile);
}
void sphere(Vec3 center,float radius,Color tint){
    constexpr int rows=6,columns=10;
    for(int row=0;row<rows;++row){
        float v0=-game::PI/2+row*game::PI/rows,v1=-game::PI/2+(row+1)*game::PI/rows;
        for(int col=0;col<columns;++col){
            float a0=col*2*game::PI/columns,a1=(col+1)*2*game::PI/columns;
            auto point=[&](float elevation,float angle){
                Vec3 n{std::cos(elevation)*std::cos(angle),std::sin(elevation),std::cos(elevation)*std::sin(angle)};
                return vertex(center+n*radius,n,0,0,tint);
            };
            auto p0=point(v0,a0),p1=point(v0,a1),p2=point(v1,a1),p3=point(v1,a0);
            triangle(0,p0,p1,p2);triangle(0,p0,p2,p3);
        }
    }
}
void model(const std::string& name,Vec3 position,Vec3 size,float yaw,Color tint={1,1,1}){
    const Mesh* source=mesh(name);if(!source)return;
    if(name.rfind("nature/",0)==0&&game::len(Vec2{position.x,position.z}-game::player)>360){
        if(const Mesh* lod=mesh(name+"-lod"))source=lod;
    }
    float sx=size.x/std::max(0.01f,source->maxX-source->minX);
    float sy=size.y/std::max(0.01f,source->maxY-source->minY);
    float sz=size.z/std::max(0.01f,source->maxZ-source->minZ);
    float centerX=(source->minX+source->maxX)*0.5f;
    float centerZ=(source->minZ+source->maxZ)*0.5f;
    float co=std::cos(yaw),si=std::sin(yaw);
    int group=source->textured?2:0;
    if(name.rfind("buildings/",0)==0)group=name.find("skyscraper")!=std::string::npos?3:2;
    else if(name.rfind("nature/",0)==0)group=4;
    else if(name.rfind("characters/",0)==0)group=5;
    else if(name.rfind("vehicles/",0)==0)group=6;
    modelInstances->push_back({source,group,sx,sy,sz,co,si,position.x,position.y,position.z,
        centerX,source->minY,centerZ,tint.r,tint.g,tint.b});
}
bool skinnedCharacter(const std::string& name,Vec3 position,Vec3 size,float yaw,
                      int action,float actionWeight){
    const SkinMesh* skin=skinMesh(name);
    const Mesh* bounds=mesh(name);
    if(!skin||!bounds||skin->clips.empty())return false;
    action=std::clamp(action,0,int(skin->clips.size())-1);
    actionWeight=std::clamp(actionWeight,0.0f,1.0f);
    auto sample=[&](const SkinClip& clip,unsigned joint,std::array<float,16>& result){
        float offset=(position.x*0.013f+position.z*0.017f)*0.1f;
        float phase=std::fmod(game::worldTime/std::max(0.01f,clip.duration)+offset,1.0f)*clip.frames;
        unsigned first=unsigned(phase)%clip.frames,next=(first+1)%clip.frames;
        float mix=phase-std::floor(phase);
        const auto& a=clip.palettes[size_t(first)*skin->jointCount+joint];
        const auto& b=clip.palettes[size_t(next)*skin->jointCount+joint];
        for(int i=0;i<16;++i)result[i]=a[i]+(b[i]-a[i])*mix;
    };
    std::vector<std::array<float,16>> palette(skin->jointCount);
    for(unsigned joint=0;joint<skin->jointCount;++joint){
        sample(skin->clips[0],joint,palette[joint]);
        if(action>0&&actionWeight>0){
            std::array<float,16> other{};sample(skin->clips[action],joint,other);
            for(int i=0;i<16;++i)palette[joint][i]+=(other[i]-palette[joint][i])*actionWeight;
        }
    }
    float sx=size.x/std::max(0.01f,bounds->maxX-bounds->minX);
    float sy=size.y/std::max(0.01f,bounds->maxY-bounds->minY);
    float sz=size.z/std::max(0.01f,bounds->maxZ-bounds->minZ);
    float centerX=(bounds->minX+bounds->maxX)*0.5f;
    float centerZ=(bounds->minZ+bounds->maxZ)*0.5f;
    float co=std::cos(yaw),si=std::sin(yaw);
    auto& output=buckets[5];output.reserve(output.size()+skin->vertices.size());
    for(const SkinVertex& input:skin->vertices){
        Vec3 p{},n{};
        for(int influence=0;influence<4;++influence){
            float weight=input.weights[influence];
            if(weight<=0||input.joints[influence]>=skin->jointCount)continue;
            const auto& m=palette[input.joints[influence]];
            const auto& v=input.base;
            p.x+=weight*(m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12]);
            p.y+=weight*(m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13]);
            p.z+=weight*(m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]);
            n.x+=weight*(m[0]*v.nx+m[4]*v.ny+m[8]*v.nz);
            n.y+=weight*(m[1]*v.nx+m[5]*v.ny+m[9]*v.nz);
            n.z+=weight*(m[2]*v.nx+m[6]*v.ny+m[10]*v.nz);
        }
        float x=(p.x-centerX)*sx,z=(p.z-centerZ)*sz;
        float nx=n.x/sx,ny=n.y/sy,nz=n.z/sz;
        float length=std::max(0.0001f,std::sqrt(nx*nx+ny*ny+nz*nz));
        const auto& v=input.base;
        output.push_back({position.x+co*x+si*z,position.y+(p.y-bounds->minY)*sy,
            position.z-si*x+co*z,(co*nx+si*nz)/length,ny/length,
            (-si*nx+co*nz)/length,v.u,v.v,v.r,v.g,v.b,v.a});
    }
    return true;
}
bool close(Vec2 p,float distance){return game::len(p-game::player)<distance;}
void streetlights(){
    float daylight=std::sin((game::gameHour-6)*game::PI/12.0f);
    for(int column=0;column<5;++column)for(int row=0;row<7;++row){
        float x=300+column*450+68.0f,z=115+row*215.0f;
        if(!close({x,z},600))continue;
        box(x,0,z,2.5f,47,2.5f,game::rgb(75,77,78));
        box(x-7,46,z,14,2,2,game::rgb(72,74,78));
        box(x-14,43,z,7,4,7,daylight<0.1f?game::rgb(255,222,125):game::rgb(178,175,154));
    }
}
void buildings(){
    const char* variants[]={"building-a","building-d","building-g","building-j",
        "building-m","building-skyscraper-c","building-skyscraper-d"};
    float range=ui::graphicsQuality==0?500:ui::graphicsQuality==1?680:850;
    for(size_t index=0;index<game::buildings.size();++index){
        const auto& b=game::buildings[index];
        if(!close({b.x+b.w/2,b.z+b.d/2},range))continue;
        ground(b.x-12,b.z-12,b.x+b.w+12,b.z+b.d+12,0.1f,game::rgb(216,211,196),4);
        for(int row=0;row<2;++row)for(int col=0;col<2;++col){
            float width=b.w/2-7,depth=b.d/2-7;
            float x=b.x+b.w*(col+0.5f)/2,z=b.z+b.d*(row+0.5f)/2;
            int selection=int((index*5+row*3+col*7)%7);
            float height=b.h*(0.88f+0.13f*float((row+col+int(index))%3));
            model(std::string("buildings/")+variants[selection],{x,0.3f,z},
                {width,height,depth},(row+col)%2?game::PI/2:0);
            if(selection<3){
                box(x,1,z-depth*0.48f,width*0.5f,10,4,game::rgb(140,65+selection*20,71));
            }
        }
    }
}
void vegetation(){
    if(ui::vegetationDensity==0)return;
    for(int i=0;i<22;i+=ui::vegetationDensity==1?2:1){
        float x=70+i*108.0f,z=game::BEACH_START+49;
        if(close({x,z},650))model(i%3==0?"nature/tree_palmDetailedShort":"nature/tree_palmDetailedTall",
            {x,0,z},{72,i%3==0?70.0f:86.0f,72},i*0.43f);
    }
    for(int col=0;col<5;++col)for(int row=0;row<4;++row){
        float x=300+col*450+83.0f,z=250+row*390+81.0f;
        if(close({x,z},600))model((row+col)%3==0?"nature/tree_detailed":"nature/tree_oak",
            {x,0,z},{54,68,54},row*0.9f);
    }
    if(ui::vegetationDensity==2){
        for(int i=0;i<70;++i){float x=35+float((i*137)%2280),z=game::BEACH_START+35+float((i*67)%210);
            if(close({x,z},400))model(i%4==0?"nature/plant_bushDetailed":"nature/grass_large",
                {x,0,z},{i%4==0?12.0f:8.0f,i%4==0?12.0f:7.0f,i%4==0?12.0f:8.0f},i*0.76f);}
    }
}
void character(Vec2 p,float angle,int style,bool armed,bool moving,bool running,float height=0){
    const char* choices[]={"casual-man","hoodie-man","casual-woman","beach-man"};
    std::string name="characters/"+std::string(choices[style%4]);
    if(close(p,220)&&skinnedCharacter(name,{p.x,height,p.z},{14,34,14},game::PI/2-angle,
            armed?3:running?2:moving?1:0,armed?1.0f:moving?0.9f:0.0f)){
        if(armed)name+="-aim";
        else if(moving)name+=(running?"-run":"-walk")+std::to_string((int(game::worldTime*(running?10:6)+p.x))%4);
        model(name,{p.x,height,p.z},{14,34,14},game::PI/2-angle);
    }
    if(armed){Vec2 f=game::forward(angle),r{-f.z,f.x};
        box(p.x+f.x*11+r.x*5,height+19,p.z+f.z*11+r.z*5,17,3,4,game::rgb(46,48,52));}
}
void people(){
    for(const auto& ped:game::peds){
        if(!ped.alive||!close(ped.p,420))continue;
        character(ped.p,ped.angle,ped.style,ped.armed,ped.panic>0||game::len(ped.target-ped.p)>10,
            ped.panic>0);
    }
    if(game::occupied<0&&game::health>0){
        bool moving=game::len(game::playerVelocity)>15;
        character(game::player,game::cameraYaw,1,game::rightMouse,moving,
            game::len(game::playerVelocity)>205,game::playerY);
    }
}
void vehicles(){
    for(const auto& v:game::vehicles){
        if(!close(v.p,650))continue;
        Color paint=v.c;
        if(v.kind==game::Kind::Car||v.kind==game::Kind::SportCar){
            Color tint{0.68f+paint.r*0.32f,0.68f+paint.g*0.32f,0.68f+paint.b*0.32f};
            model(v.kind==game::Kind::SportCar?"vehicles/sports-car":"vehicles/sedan",
                {v.p.x,0,v.p.z},{26,v.kind==game::Kind::SportCar?20.0f:23.0f,48},
                game::PI/2-v.angle,tint);
            if(v.damage>40)box(v.p.x+19,14,v.p.z,3,2,13,game::rgb(48,47,47));
        }else if(v.kind==game::Kind::Bike){
            for(float offset:{-12.0f,12.0f})box(v.p.x+offset,1,v.p.z,8,8,8,game::rgb(39,42,45));
            box(v.p.x,8,v.p.z,25,5,7,paint);
            box(v.p.x+10,14,v.p.z,3,10,5,game::rgb(70,73,74));
        }else{
            model("vehicles/motorboat",{v.p.x,-2,v.p.z},{24,22,48},game::PI/2-v.angle);
        }
    }
}
void markers(){
    for(const auto& pickup:game::pickups)if(pickup.available&&close(pickup.p,650))
        sphere({pickup.p.x,13+std::sin(game::worldTime*3)*3,pickup.p.z},5,game::rgb(84,238,235));
    for(const auto& mission:game::missions)if(close(mission.start,650)){
        ground(mission.start.x-14,mission.start.z-14,mission.start.x+14,mission.start.z+14,
            0.3f,game::rgb(201,149,77));
        sphere({mission.start.x,24+std::sin(game::worldTime*2)*3,mission.start.z},7,
            game::rgb(243,197,96));
    }
    if(game::activeMission>=0&&game::missionStep<int(game::missions[game::activeMission].goals.size())){
        Vec2 goal=game::missions[game::activeMission].goals[game::missionStep];
        bool target=game::missions[game::activeMission].kind==game::MissionKind::Targets||
            (game::missions[game::activeMission].kind==game::MissionKind::Finale&&game::missionStep==1);
        if(close(goal,650))sphere({goal.x,24+std::sin(game::worldTime*2)*3,goal.z},9,
            target?game::rgb(255,139,75):game::rgb(135,255,123));
    }
}
Vec3 rotateBy(const RagdollPart& body,Vec3 v){
    Vec3 q{body.qx,body.qy,body.qz};
    Vec3 cross{q.y*v.z-q.z*v.y,q.z*v.x-q.x*v.z,q.x*v.y-q.y*v.x};
    Vec3 nested{q.y*cross.z-q.z*cross.y,q.z*cross.x-q.x*cross.z,
        q.x*cross.y-q.y*cross.x};
    return v+(cross*body.qw+nested)*2.0f;
}
void ragdollMesh(const RagdollPart* bodies){
    const char* choices[]={"casual-man","hoodie-man","casual-woman","beach-man"};
    std::string name="characters/"+std::string(choices[bodies[0].style%4]);
    const SkinMesh* skin=skinMesh(name);const Mesh* bounds=mesh(name);
    if(!skin||!bounds||skin->clips.empty()||skin->bodyPartForJoint.size()!=skin->jointCount)return;
    const auto& idle=skin->clips[0];
    float sx=14/std::max(0.01f,bounds->maxX-bounds->minX);
    float sy=34/std::max(0.01f,bounds->maxY-bounds->minY);
    float sz=14/std::max(0.01f,bounds->maxZ-bounds->minZ);
    float cx=(bounds->minX+bounds->maxX)*0.5f;
    float cz=(bounds->minZ+bounds->maxZ)*0.5f;
    float co=std::cos(bodies[0].yaw),si=std::sin(bodies[0].yaw);
    auto& output=buckets[5];output.reserve(output.size()+skin->vertices.size());
    for(const SkinVertex& input:skin->vertices){
        Vec3 world{},normal{};
        for(int influence=0;influence<4;++influence){
            float weight=input.weights[influence];unsigned joint=input.joints[influence];
            if(weight<=0||joint>=skin->jointCount)continue;
            const auto& m=idle.palettes[joint];
            const auto& v=input.base;
            Vec3 p{m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12],
                m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13],
                m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]};
            Vec3 n{m[0]*v.nx+m[4]*v.ny+m[8]*v.nz,
                m[1]*v.nx+m[5]*v.ny+m[9]*v.nz,
                m[2]*v.nx+m[6]*v.ny+m[10]*v.nz};
            float x=(p.x-cx)*sx,z=(p.z-cz)*sz;
            Vec3 restWorld{bodies[0].origin.x+co*x+si*z,
                bodies[0].origin.y+(p.y-bounds->minY)*sy,
                bodies[0].origin.z-si*x+co*z};
            int part=skin->bodyPartForJoint[joint];
            if(part<0||part>=6)part=0;
            const auto& body=bodies[part];
            Vec3 deformed=body.p+rotateBy(body,restWorld-body.rest);
            world=world+deformed*weight;
            Vec3 scaled{n.x/sx,n.y/sy,n.z/sz};
            Vec3 oriented{co*scaled.x+si*scaled.z,scaled.y,-si*scaled.x+co*scaled.z};
            normal=normal+rotateBy(body,oriented)*weight;
        }
        normal=game::norm(normal);
        const auto& v=input.base;
        output.push_back({world.x,world.y,world.z,normal.x,normal.y,normal.z,
            v.u,v.v,v.r,v.g,v.b,v.a});
    }
}
void effects(){
    for(const auto& bullet:game::bullets)if(close({bullet.p.x,bullet.p.z},550))
        sphere(bullet.p,2.6f,game::rgb(255,235,112));
    for(std::size_t index=0;index+5<game::ragdollParts.size();index+=6)
        if(close({game::ragdollParts[index].p.x,game::ragdollParts[index].p.z},500))
            ragdollMesh(game::ragdollParts.data()+index);
    if(ui::effectsQuality>0){
        for(const auto& part:game::debris)if(close({part.p.x,part.p.z},500))
            box(part.p.x,part.p.y-part.h/2,part.p.z,part.w,part.h,part.d,
                part.tile>=24?game::rgb(55,66,82):game::rgb(161,91,86));
        for(const auto& impact:game::impacts)if(close(impact.p,500))
            ground(impact.p.x-8,impact.p.z-8,impact.p.x+8,impact.p.z+8,0.4f,game::rgb(130,31,37));
    }
    for(const auto& prop:game::props)if(prop.alive&&close(prop.p,500))
        box(prop.p.x,prop.y,prop.p.z,prop.barrel?19.0f:23.0f,prop.barrel?23.0f:22.0f,
            prop.barrel?19.0f:23.0f,prop.barrel?game::rgb(88,111,116):game::rgb(155,124,87));
}
}
void buildScene(std::vector<Vertex> groups[MATERIAL_GROUPS],std::vector<ModelInstance>& instances){
    buckets=groups;modelInstances=&instances;instances.clear();
    for(int i=0;i<MATERIAL_GROUPS;++i)groups[i].clear();
    ground(0,0,game::WORLD_W,game::BEACH_START,0,game::rgb(150,195,145),-1,9);
    ground(0,game::BEACH_START,game::WORLD_W,game::SHORE,0.05f,game::rgb(244,218,166),-1,8);
    ground(0,game::SHORE,game::WORLD_W,game::WORLD_D,-0.4f,game::rgb(86,160,197),7);
    for(int col=0;col<5;++col){float x=300+col*450.0f;
        ground(x-game::ROAD_W/2,0,x+game::ROAD_W/2,game::BEACH_START,0.1f,game::rgb(126,129,133),-1,7);
        for(int segment=0;segment<38;++segment){float z=20+segment*41.0f;
            ground(x-1.5f,z,x+1.5f,z+19,0.15f,game::rgb(219,197,126));}}
    for(int row=0;row<4;++row){float z=250+row*390.0f;
        ground(0,z-game::ROAD_W/2,game::WORLD_W,z+game::ROAD_W/2,0.12f,game::rgb(126,129,133),-1,7);
        for(int segment=0;segment<58;++segment){float x=20+segment*41.0f;
            ground(x,z-1.5f,x+19,z+1.5f,0.16f,game::rgb(219,197,126));}}
    for(float x:{300.0f,800.0f,1490.0f})
        ground(x-22,game::SHORE-25,x+22,game::SHORE+90,0.2f,game::rgb(160,139,111),4);
    buildings();vegetation();streetlights();vehicles();people();markers();effects();
    float solar=std::sin((game::gameHour-6)*game::PI/12),angle=(game::gameHour-6)*game::PI/12;
    bool day=solar>=0;float sign=day?1.0f:-1.0f;
    sphere({game::player.x+std::cos(angle)*sign*820,std::abs(solar)*600+130,game::player.z-160},
        day?47.0f:31.0f,day?game::rgb(255,230,151):game::rgb(222,234,255));
}
}
