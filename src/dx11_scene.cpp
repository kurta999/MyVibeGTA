#include "dx11_assets.h"
#include "game.h"
#include "game_internal.h"
#include "ui.h"
#include "camera.h"
#include "physics.h"
#include "fire.h"
#include "commerce.h"
#include "weapons.h"
#include "traversal.h"
#include "weather.h"
#include "regions.h"
#include "debug_menu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>

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
bool primitive(const char* name,Vec3 bottom,Vec3 size,Color tint){
    if(!modelInstances)return false;
    const Mesh* source=mesh(name);
    if(!source)return false;
    modelInstances->push_back({source,0,
        size.x/std::max(0.01f,source->maxX-source->minX),
        size.y/std::max(0.01f,source->maxY-source->minY),
        size.z/std::max(0.01f,source->maxZ-source->minZ),
        1,0,bottom.x,bottom.y,bottom.z,
        (source->minX+source->maxX)*0.5f,source->minY,
        (source->minZ+source->maxZ)*0.5f,tint.r,tint.g,tint.b});
    return true;
}
void box(float x,float y,float z,float w,float h,float d,Color tint,int tile=-1){
    if(tile<0&&primitive("primitive/box",{x,y,z},{w,h,d},tint))return;
    float a=x-w/2,b=x+w/2,c=z-d/2,e=z+d/2,t=y+h;
    quad(0,{a,y,c},{b,y,c},{b,t,c},{a,t,c},{0,0,-1},tint,tile);
    quad(0,{b,y,e},{a,y,e},{a,t,e},{b,t,e},{0,0,1},tint,tile);
    quad(0,{a,y,e},{a,y,c},{a,t,c},{a,t,e},{-1,0,0},tint,tile);
    quad(0,{b,y,c},{b,y,e},{b,t,e},{b,t,c},{1,0,0},tint,tile);
    quad(0,{a,t,c},{b,t,c},{b,t,e},{a,t,e},{0,1,0},tint,tile);
}
void beam(Vec3 start,Vec3 end,float width,float height,Color tint){
    const Mesh* source=mesh("primitive/box");
    Vec3 delta=end-start;
    float length=game::len(delta);
    if(!modelInstances||!source||length<0.01f)return;
    float horizontal=std::sqrt(delta.x*delta.x+delta.z*delta.z);
    float cosYaw=horizontal>0.001f?delta.z/horizontal:1.0f;
    float sinYaw=horizontal>0.001f?delta.x/horizontal:0.0f;
    Vec3 center=(start+end)*0.5f;
    modelInstances->push_back({source,0,width,height,length,cosYaw,sinYaw,
        center.x,center.y,center.z,0,0.5f,0,tint.r,tint.g,tint.b,
        delta.y/length,horizontal/length});
}
void sphere(Vec3 center,float radius,Color tint){
    if(primitive("primitive/sphere",{center.x,center.y-radius,center.z},
                 {radius*2,radius*2,radius*2},tint))return;
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
    float lodScale=ui::lodDistanceScale();
    if(name.rfind("nature/",0)==0&&
       game::len(Vec2{position.x,position.z}-game::player)>
           (name.rfind("nature/bush_",0)==0?90:160)*lodScale){
        if(const Mesh* lod=mesh(name+"-lod"))source=lod;
    }
    if((name.rfind("buildings/",0)==0||name.rfind("marina/",0)==0)&&
       game::len(Vec2{position.x,position.z}-game::player)>550*lodScale){
        if(const Mesh* lod=mesh(name+"-lod"))source=lod;
    }
    float sx=size.x/std::max(0.01f,source->maxX-source->minX);
    float sy=size.y/std::max(0.01f,source->maxY-source->minY);
    float sz=size.z/std::max(0.01f,source->maxZ-source->minZ);
    float centerX=(source->minX+source->maxX)*0.5f;
    float centerZ=(source->minZ+source->maxZ)*0.5f;
    float co=std::cos(yaw),si=std::sin(yaw);
    int group=source->textured?2:0;
    if(name.rfind("buildings/",0)==0)group=(name.find("skyscraper")!=std::string::npos||
        name.find("urban-")!=std::string::npos)?12:2;
    else if(name.rfind("marina/",0)==0)group=10;
    else if(name.rfind("nature/",0)==0)group=4;
    else if(name.rfind("characters/",0)==0)group=5;
    else if(name.rfind("vehicles/",0)==0)group=6;
    if(source->transparent)group=13;
    modelInstances->push_back({source,group,sx,sy,sz,co,si,position.x,position.y,position.z,
        centerX,source->minY,centerZ,tint.r,tint.g,tint.b});
}
void glowBox(Vec3 bottom,Vec3 size,float yaw,Color tint){
    if(!modelInstances)return;
    const Mesh* source=mesh("primitive/box");
    if(!source)return;
    modelInstances->push_back({source,14,size.x,size.y,size.z,
        std::cos(yaw),std::sin(yaw),bottom.x,bottom.y,bottom.z,
        0,0,0,tint.r,tint.g,tint.b});
}
bool skinnedCharacter(const std::string& name,Vec3 position,Vec3 size,float yaw,
                      int action,float actionWeight,Vec3* rightHand,float actionPhase,
                      int motion,float aimPitch){
    const SkinMesh* skin=skinMesh(name);
    const Mesh* bounds=mesh(name);
    if(!skin||!bounds||skin->clips.empty()||
       skin->bodyPartForJoint.size()!=skin->jointCount)return false;
    action=std::clamp(action,0,int(skin->clips.size())-1);
    actionWeight=std::clamp(actionWeight,0.0f,1.0f);
    auto frameAt=[&](const SkinClip& clip,float phaseOverride){
        float offset=(position.x*0.013f+position.z*0.017f)*0.1f;
        float phase=phaseOverride>=0?
            std::clamp(phaseOverride,0.0f,1.0f)*float(clip.frames-1):
            std::fmod(game::worldTime/std::max(0.01f,clip.duration)+offset,1.0f)*clip.frames;
        unsigned first=unsigned(phase)%clip.frames;
        unsigned next=phaseOverride>=0?std::min(first+1,clip.frames-1):(first+1)%clip.frames;
        return std::array<float,3>{float(first),float(next),phase-std::floor(phase)};
    };
    auto sample=[&](const SkinClip& clip,unsigned joint,std::array<float,16>& result,
                    float phaseOverride){
        auto frame=frameAt(clip,phaseOverride);
        unsigned first=unsigned(frame[0]),next=unsigned(frame[1]);
        float mix=frame[2];
        const auto& a=clip.palettes[size_t(first)*skin->jointCount+joint];
        const auto& b=clip.palettes[size_t(next)*skin->jointCount+joint];
        for(int i=0;i<16;++i)result[i]=a[i]+(b[i]-a[i])*mix;
    };
    std::vector<std::array<float,16>> palette(skin->jointCount);
    for(unsigned joint=0;joint<skin->jointCount;++joint){
        sample(skin->clips[0],joint,palette[joint],-1);
        if(action>0&&actionWeight>0){
            std::array<float,16> other{};
            sample(skin->clips[action],joint,other,actionPhase);
            for(int i=0;i<16;++i)palette[joint][i]+=(other[i]-palette[joint][i])*actionWeight;
        }
    }
    float sx=size.x/std::max(0.01f,bounds->maxX-bounds->minX);
    float sy=size.y/std::max(0.01f,bounds->maxY-bounds->minY);
    float sz=size.z/std::max(0.01f,bounds->maxZ-bounds->minZ);
    float centerX=(bounds->minX+bounds->maxX)*0.5f;
    float centerZ=(bounds->minZ+bounds->maxZ)*0.5f;
    float co=std::cos(yaw),si=std::sin(yaw);
    Vec2 aimForward=game::forward(game::PI/2-yaw);
    auto pitchArm=[&](Vec3& point,float weight){
        if(weight<=0||std::abs(aimPitch)<0.001f)return;
        float pitch=aimPitch*std::clamp(weight,0.0f,1.0f);
        Vec3 pivot{position.x+aimForward.x*3,position.y+size.y*0.77f,
            position.z+aimForward.z*3};
        float depth=(point.x-pivot.x)*aimForward.x+
            (point.z-pivot.z)*aimForward.z;
        float vertical=point.y-pivot.y;
        float rotated=std::cos(pitch)*depth-std::sin(pitch)*vertical;
        point.x+=aimForward.x*(rotated-depth);
        point.z+=aimForward.z*(rotated-depth);
        point.y=pivot.y+std::sin(pitch)*depth+std::cos(pitch)*vertical;
    };
    float bodyHeight=std::max(0.01f,bounds->maxY-bounds->minY);
    float motionPhase=motion==2?game::playerY*6.2831853f/42.0f:
        game::worldTime*(motion==1?8.0f:6.0f);
    float waveSin=std::sin(motionPhase),waveCos=std::cos(motionPhase);
    float tilt=motion==1?1.05f:motion==2?0.10f:motion==3?0.12f:0.0f;
    float coTilt=std::cos(tilt),siTilt=std::sin(tilt);
    float rising=std::clamp(game::playerVerticalSpeed/230.0f,0.0f,1.0f);
    auto applyMotion=[&](Vec3& p,Vec3& n,const float parts[6]){
        if(motion==0)return;
        float left=waveSin,right=-left;
        float armWave=parts[2]*left+parts[3]*right;
        float legWave=parts[4]*right+parts[5]*left;
        if(motion==1){
            p.x+=bodyHeight*0.17f*(parts[3]*(0.5f-0.5f*waveSin)-
                parts[2]*(0.5f+0.5f*waveSin));
            p.z+=bodyHeight*(0.18f*armWave+0.10f*legWave);
            p.y+=bodyHeight*(0.08f*(parts[2]+parts[3])*waveCos+
                0.07f*legWave);
        }else if(motion==2){
            float armWeight=parts[2]+parts[3];
            float shoulderY=bounds->minY+bodyHeight*0.77f;
            p.y+=armWeight*std::max(0.0f,shoulderY-p.y)*1.85f+
                bodyHeight*(0.07f*armWave+0.13f*legWave);
            p.z+=bodyHeight*(0.09f*armWeight-0.05f*legWave);
        }else if(motion==3){
            p.y+=bodyHeight*(0.14f*(parts[2]+parts[3])+
                0.10f*rising*(parts[4]+parts[5]));
            p.z+=bodyHeight*(0.09f*rising*(parts[4]+parts[5])-0.04f*armWave);
        }
        float pivot=bounds->minY+bodyHeight*0.45f;
        float vertical=p.y-pivot;
        float depth=p.z-centerZ;
        p.y=pivot+coTilt*vertical-siTilt*depth;
        p.z=centerZ+siTilt*vertical+coTilt*depth;
        float ny=n.y,nz=n.z;
        n.y=coTilt*ny-siTilt*nz;
        n.z=siTilt*ny+coTilt*nz;
    };
    if(rightHand){
        auto handAt=[&](const SkinClip& clip){
            auto frame=frameAt(clip,&clip==&skin->clips[action]?actionPhase:-1);
            unsigned first=unsigned(frame[0]),next=unsigned(frame[1]);
            float mix=frame[2];
            const auto& a=clip.rightHands[first];const auto& b=clip.rightHands[next];
            return Vec3{a[0]+(b[0]-a[0])*mix,a[1]+(b[1]-a[1])*mix,
                a[2]+(b[2]-a[2])*mix};
        };
        Vec3 hand=handAt(skin->clips[0]);
        if(action>0&&actionWeight>0)
            hand=hand+(handAt(skin->clips[action])-hand)*actionWeight;
        float handPart[6]{};handPart[3]=1;
        Vec3 handNormal{};
        applyMotion(hand,handNormal,handPart);
        float x=(hand.x-centerX)*sx,z=(hand.z-centerZ)*sz;
        *rightHand={position.x+co*x+si*z,position.y+(hand.y-bounds->minY)*sy,
            position.z-si*x+co*z};
        pitchArm(*rightHand,1.0f);
    }
    auto& output=buckets[5];output.reserve(output.size()+skin->vertices.size());
    for(const SkinVertex& input:skin->vertices){
        Vec3 p{},n{};
        float parts[6]{};
        for(int influence=0;influence<4;++influence){
            float weight=input.weights[influence];
            if(weight<=0||input.joints[influence]>=skin->jointCount)continue;
            int part=skin->bodyPartForJoint[input.joints[influence]];
            if(part>=0&&part<6)parts[part]+=weight;
            const auto& m=palette[input.joints[influence]];
            const auto& v=input.base;
            p.x+=weight*(m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12]);
            p.y+=weight*(m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13]);
            p.z+=weight*(m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]);
            n.x+=weight*(m[0]*v.nx+m[4]*v.ny+m[8]*v.nz);
            n.y+=weight*(m[1]*v.nx+m[5]*v.ny+m[9]*v.nz);
            n.z+=weight*(m[2]*v.nx+m[6]*v.ny+m[10]*v.nz);
        }
        applyMotion(p,n,parts);
        float x=(p.x-centerX)*sx,z=(p.z-centerZ)*sz;
        float nx=n.x/sx,ny=n.y/sy,nz=n.z/sz;
        float length=std::max(0.0001f,std::sqrt(nx*nx+ny*ny+nz*nz));
        const auto& v=input.base;
        Vec3 world{position.x+co*x+si*z,position.y+(p.y-bounds->minY)*sy,
            position.z-si*x+co*z};
        pitchArm(world,parts[2]+parts[3]);
        output.push_back({world.x,world.y,world.z,(co*nx+si*nz)/length,ny/length,
            (-si*nx+co*nz)/length,v.u,v.v,v.r,v.g,v.b,v.a});
    }
    return true;
}
float drawScale(){
    return ui::drawDistanceScale();
}
bool heldWeapon(const char* name,Vec3 hand,Vec3 muzzle,float width,float height){
    const Mesh* source=mesh(name);
    Vec3 delta=muzzle-hand;
    float length=game::len(delta);
    if(!modelInstances||!source||length<0.01f)return false;
    float horizontal=std::sqrt(delta.x*delta.x+delta.z*delta.z);
    Vec3 center=hand+delta*0.5f;
    modelInstances->push_back({source,2,
        width/std::max(0.01f,source->maxX-source->minX),
        height/std::max(0.01f,source->maxY-source->minY),
        length/std::max(0.01f,source->maxZ-source->minZ),
        horizontal>0.001f?delta.z/horizontal:1.0f,
        horizontal>0.001f?delta.x/horizontal:0.0f,
        center.x,center.y,center.z,
        (source->minX+source->maxX)*0.5f,
        (source->minY+source->maxY)*0.5f,
        (source->minZ+source->maxZ)*0.5f,
        1,1,1,delta.y/length,horizontal/length});
    return true;
}
bool close(Vec2 p,float distance){
    return game::len(p-game::player)<distance*drawScale();
}
void streetlights(){
    float daylight=std::sin((game::gameHour-6)*game::PI/12.0f);
    for(int column=0;column<5;++column)for(int row=0;row<7;++row){
        float x=300+column*450+68.0f,z=115+row*215.0f;
        if(!close({x,z},600))continue;
        box(x,0,z,2.5f,47,2.5f,game::rgb(75,77,78));
        box(x-7,46,z,14,2,2,game::rgb(72,74,78));
        box(x-14,43,z,7,4,7,daylight<0.1f?game::rgb(255,222,125):game::rgb(178,175,154));
        if(daylight<0.1f)
            glowBox({x-14,43,z},{7,4,7},0,game::rgb(255,220,135));
    }
    if(daylight<0.1f)for(const auto& shop:commerce::shops){
        if(!close(shop.p,350))continue;
        box(shop.p.x+12,0,shop.p.z,1.6f,21,1.6f,game::rgb(62,69,72));
        glowBox({shop.p.x+12,17,shop.p.z},{20,4,1.5f},0,
            game::rgb(72,225,243));
    }
}
void nightSky(){
    float solar=std::sin((game::gameHour-6)*game::PI/12.0f);
    if(solar>-0.18f||weather::current().clouds>0.72f)return;
    float phase=(game::gameHour-18.0f)*game::PI/12.0f;
    Vec3 moon{game::player.x+690*std::cos(phase*0.08f),
        175+20*std::sin(phase),
        game::player.z-250+45*std::sin(phase*0.08f)};
    sphere(moon,18,game::rgb(229,231,209));
    for(int index=0;index<24;++index){
        float angle=index*2.399963f;
        float distance=42.0f+float((index*17)%83);
        Vec3 star{moon.x+float(index%5)*3,
            moon.y+std::sin(angle)*distance*0.7f,
            moon.z+std::cos(angle)*distance};
        sphere(star,index%7==0?3.0f:1.8f,game::rgb(239,244,244));
    }
    for(int index=0;index<72;++index){
        float bearing=index*2.399963f;
        float elevation=0.16f+0.30f*float((index*37)%41)/40.0f;
        float distance=750.0f;
        Vec3 star{game::player.x+std::cos(bearing)*distance,
            100+elevation*distance,
            game::player.z+std::sin(bearing)*distance};
        float radius=index%9==0?3.2f:1.8f;
        sphere(star,radius,game::rgb(236,241,243));
    }
}
void regionalTerrain(){
    float radius=1100*drawScale();
    auto emitTiles=[&](float tile,bool distant){
        int minX=std::max(0,int(std::floor((game::player.x-radius)/tile)));
        int maxX=std::min(int(regions::WIDTH/tile)-1,
            int(std::floor((game::player.x+radius)/tile)));
        int minZ=std::max(0,int(std::floor((game::player.z-radius)/tile)));
        int maxZ=std::min(int(regions::DEPTH/tile)-1,
            int(std::floor((game::player.z+radius)/tile)));
        for(int z=minZ;z<=maxZ;++z)for(int x=minX;x<=maxX;++x){
            float left=x*tile,top=z*tile;
            if(left<game::WORLD_W&&top<game::WORLD_D)continue;
            Vec2 sample{left+tile*0.5f,top+tile*0.5f};
            float distance=game::len(sample-game::player);
            if(distance>radius+tile||(!distant&&distance>1750)||
               (distant&&distance<1550))continue;
            float level=distant?-0.12f:0.0f;
            if(regions::waterAt(sample))
                ground(left,top,left+tile,top+tile,level-0.4f,
                    game::rgb(74,145,183),-1,3);
            else if(regions::roadAt(sample))
                ground(left,top,left+tile,top+tile,level+0.11f,
                    game::rgb(112,114,116),-1,7);
            else{
                auto biome=regions::biomeAt(sample);
                int material=biome==regions::Biome::City||
                    biome==regions::Biome::Countryside||biome==regions::Biome::Savanna?9:0;
                ground(left,top,left+tile,top+tile,level,
                    regions::groundColor(sample),-1,material);
            }
        }
    };
    if(radius>1600)emitTiles(400,true);
    emitTiles(100,false);
    // Visual-only terrain beyond the playable bounds keeps the city edge from
    // cutting a hard line against the sky. The player and Jolt bounds stay put.
    if(game::player.x<radius+400||game::player.z<radius+400||
       game::player.x>regions::WIDTH-radius-400||
       game::player.z>regions::DEPTH-radius-400){
        constexpr float skirt=400.0f;
        int x0=int(std::floor((game::player.x-radius)/skirt));
        int x1=int(std::floor((game::player.x+radius)/skirt));
        int z0=int(std::floor((game::player.z-radius)/skirt));
        int z1=int(std::floor((game::player.z+radius)/skirt));
        for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x){
            if(x>=0&&z>=0&&x*skirt<regions::WIDTH&&
               z*skirt<regions::DEPTH)continue;
            Vec2 center{(x+0.5f)*skirt,(z+0.5f)*skirt};
            if(game::len(center-game::player)>radius+skirt)continue;
            Vec2 edge{std::clamp(center.x,1.0f,regions::WIDTH-1.0f),
                std::clamp(center.z,1.0f,regions::DEPTH-1.0f)};
            bool citySide=x<0&&edge.z<game::BEACH_START;
            bool water=!citySide&&regions::waterAt(edge);
            Color tint=water?game::rgb(74,145,183):
                citySide?game::rgb(91,139,79):regions::groundColor(edge);
            ground(x*skirt,z*skirt,(x+1)*skirt,(z+1)*skirt,
                water?-0.4f:-0.25f,tint,-1,
                water?3:regions::biomeAt(edge)==regions::Biome::Desert?8:9);
        }
    }
    if(close({7800,8500},1400)){
        for(float side:{-88.0f,88.0f}){
            beam({7600,7,8500+side},{8000,7,8500+side},3,3,game::rgb(178,183,179));
            for(int column=0;column<=8;++column){
                float x=7600+column*50.0f;
                beam({x,0,8500+side},{x,9,8500+side},2,2,game::rgb(148,154,152));
            }
        }
    }
    if(close({1200,2025},900))for(float side:{-58.0f,58.0f}){
        beam({1200+side,8,game::SHORE},{1200+side,8,game::WORLD_D},2,2,
            game::rgb(174,179,176));
        for(int post=0;post<=7;++post){
            float z=game::SHORE+post*50.0f;
            beam({1200+side,0,z},{1200+side,9,z},2,2,game::rgb(150,157,153));
        }
    }
}
void weatherGroundDetails(){
    const auto& conditions=weather::current();
    if(conditions.snow||conditions.precipitation<0.10f)return;
    int centerX=int(std::floor(game::player.x/48.0f));
    int centerZ=int(std::floor(game::player.z/48.0f));
    for(int z=centerZ-5;z<=centerZ+5;++z)for(int x=centerX-5;x<=centerX+5;++x){
        unsigned hash=unsigned(x)*73856093u^unsigned(z)*19349663u;
        if(hash%3!=0)continue;
        float px=(x+0.25f+float((hash>>4)%50)/100.0f)*48.0f;
        float pz=(z+0.25f+float((hash>>10)%50)/100.0f)*48.0f;
        bool road=regions::roadAt({px,pz});
        if(px>=0&&px<game::WORLD_W&&pz>=0&&pz<game::BEACH_START){
            for(int column=0;column<5;++column)
                road|=std::abs(px-(300+column*450))<game::ROAD_W*0.36f;
            for(int row=0;row<4;++row)
                road|=std::abs(pz-(250+row*390))<game::ROAD_W*0.36f;
        }
        if(px>=1140&&px<=1260&&pz>=game::BEACH_START&&
           pz<game::WORLD_D)road=true;
        if(!road)continue;
        float width=8+float((hash>>16)%13);
        float depth=5+float((hash>>22)%9);
        const Color tint=game::rgb(59,75,83);
        const Vec3 center{px,0.23f,pz};
        auto rim=[&](int corner){
            corner%=9;
            float angle=corner*2*game::PI/9;
            float irregular=0.72f+float((hash>>(corner%12))&7u)*0.055f;
            return Vec3{px+std::cos(angle)*width*0.5f*irregular,
                0.23f,pz+std::sin(angle)*depth*0.5f*irregular};
        };
        for(int corner=0;corner<9;++corner)
            triangle(15,vertex(center,{0,1,0},0.5f,0.5f,tint),
                vertex(rim(corner),{0,1,0},0,0,tint),
                vertex(rim(corner+1),{0,1,0},1,1,tint));
    }
}
void marinaScenery(){
    if(!close({8730,9720},1850))return;
    const Color pavement=game::rgb(194,190,177);
    const Color curb=game::rgb(221,216,202);
    for(const auto& road:regions::roads()){
        if(road.id.rfind("marina-",0)!=0)continue;
        float half=road.width*0.5f;
        float x0=std::min(road.start.x,road.end.x)-half;
        float x1=std::max(road.start.x,road.end.x)+half;
        float z0=std::min(road.start.z,road.end.z)-half;
        float z1=std::max(road.start.z,road.end.z)+half;
        ground(x0-11,z0-11,x1+11,z1+11,0.14f,curb,-1,11);
        ground(x0,z0,x1,z1,0.19f,game::rgb(91,96,99),-1,7);
    }
    // A paved Danube promenade with a planted strip in front of the west row.
    for(int z=8810;z<10015;z+=40)
        ground(8040,float(z),8137,float(std::min(z+40,10015)),0.20f,pavement,-1,11);
    ground(8015,8820,8040,10005,0.23f,game::rgb(101,145,93),-1,9);
    ground(8137,8820,8160,10005,0.23f,game::rgb(104,148,97),-1,9);
    for(int z=8850;z<10000;z+=40){
        ground(8067.0f,float(z),8068.4f,float(z+20),0.24f,
            game::rgb(176,168,151));
    }
    // Smaller tiles follow Foka Bay's curved shoreline more closely than the
    // normal regional terrain grid. The walkable decks sit above this layer.
    for(int z=10000;z<10720;z+=25)for(int x=8000;x<8800;x+=25){
        Vec2 p{float(x+12),float(z+12)};
        if(!regions::marinaBayAt(p))continue;
        int shade=(x/25+z/25)%5;
        ground(float(x),float(z),float(x+25),float(z+25),0.13f,
            game::rgb(62+shade*2,126+shade*2,157+shade*2));
    }
    for(int z=10035;z<=10685;z+=25){
        float q=(float(z)-10360.0f)/360.0f;
        float edge=8010.0f+770.0f*std::sqrt(std::max(0.0f,1.0f-q*q));
        ground(edge+7,float(z),edge+54,float(z+24),0.25f,pavement,-1,11);
        ground(edge+54,float(z),edge+73,float(z+24),0.24f,
            game::rgb(112,151,99),-1,9);
    }
    struct Pier {float x0,x1,z;};
    constexpr Pier piers[]={{8300,8730,10220},{8350,8780,10390},
        {8260,8670,10560}};
    for(const auto& pier:piers){
        float length=pier.x1-pier.x0;
        model("primitive/box",{(pier.x0+pier.x1)*0.5f,0.15f,pier.z},
            {length,1.4f,22},0,game::rgb(154,123,87));
        for(float x=pier.x0+7;x<pier.x1;x+=20){
            model("primitive/box",{x,1.56f,pier.z},
                {1.1f,0.13f,22},0,game::rgb(100,77,57));
        }
        for(float x=pier.x0+35;x<pier.x1;x+=90){
            model("primitive/cylinder",{x,0,pier.z-12},{2.8f,8,2.8f},0,
                game::rgb(73,80,80));
        }
    }
    float night=std::sin((game::gameHour-6)*game::PI/12.0f)<0.1f?1.0f:0.0f;
    for(int z=8860;z<10010;z+=116){
        model("primitive/cylinder",{8060.0f,0,float(z)},
            {3,38,3},0,game::rgb(82,88,90));
        model("primitive/box",{8060.0f,37,float(z)},
            {12,2,3},0,game::rgb(83,89,91));
        model("primitive/box",{8066.0f,34,float(z)},
            {7,3,6},0,night?game::rgb(255,221,147):game::rgb(191,198,185));
    }
    for(int z=8920;z<9980;z+=150){
        model("primitive/box",{8110.0f,2,float(z)},
            {18,2,5},0,game::rgb(119,88,61));
        for(float offset:{-7.0f,7.0f})
            model("primitive/box",{8110.0f+offset,0,float(z)},
                {2,4,5},0,game::rgb(77,82,80));
    }
}
void clouds(){
    float cover=weather::current().clouds;
    if(cover<0.15f)return;
    int count=int(10+cover*30);
    game::Vec2 wind=weather::current().wind;
    for(int index=0;index<count;++index){
        float drift=game::worldTime*wind.x*5.0f;
        float driftZ=game::worldTime*wind.z*5.0f;
        float x=game::player.x-440+std::fmod(index*137.0f+drift+4400.0f,880.0f);
        float z=game::player.z-440+std::fmod(index*233.0f+driftZ+4400.0f,880.0f);
        float y=390+float(index%4)*20;
        float solar=std::sin((game::gameHour-6)*game::PI/12.0f);
        float daylight=std::clamp(solar*2.0f+0.35f,0.0f,1.0f);
        float twilight=std::max(0.0f,1.0f-std::abs(solar)*4.0f);
        float shade=cover>0.7f?0.68f:0.90f;
        Color tint{(0.28f+0.65f*daylight+0.22f*twilight)*shade,
            (0.34f+0.62f*daylight+0.06f*twilight)*shade,
            (0.47f+0.50f*daylight-0.08f*twilight)*shade};
        model("primitive/sphere",{x,y,z},
            {130.0f+float(index%5)*18,22.0f+float(index%3)*6,90.0f},0,tint);
    }
}
void buildings(){
    float range=ui::graphicsQuality==0?500:ui::graphicsQuality==1?680:850;
    for(size_t index=0;index<game::buildings.size();++index){
        const auto& b=game::buildings[index];
        if(!close({b.x+b.w/2,b.z+b.d/2},range))continue;
        if(b.id.rfind("outpost-",0)==0){
            model("primitive/box",{b.x+b.w*0.5f,0,b.z+b.d*0.5f},
                {b.w,b.h,b.d},0,b.c);
            model("primitive/box",{b.x+b.w*0.5f,b.h,b.z+b.d*0.5f},
                {b.w+6,4,b.d+6},0,game::rgb(91,89,84));
            model("primitive/box",{b.x+b.w*0.5f,0,b.z-0.8f},
                {9,17,1.5f},0,game::rgb(93,66,45));
            continue;
        }
        if(b.id.rfind("marina-",0)==0){
            const char* style=b.id.find("wave")!=std::string::npos?"wave":
                b.id.find("terrace")!=std::string::npos?"terrace":
                b.id.find("bayfront")!=std::string::npos?"bayfront":"courtyard";
            model(std::string("marina/marina-")+style,
                {b.x+b.w*0.5f,0,b.z+b.d*0.5f},{b.w,b.h,b.d},0);
            if(std::sin((game::gameHour-6)*game::PI/12.0f)<0.12f&&
               close({b.x+b.w*0.5f,b.z+b.d*0.5f},360)){
                for(int floor=0;floor<3;++floor)for(int column=0;column<4;++column){
                    if((int(index)+floor*7+column*3)%4==0)continue;
                    glowBox({b.x+b.w*(column+1.0f)/5.0f,
                        16.0f+floor*b.h*0.20f,b.z-0.6f},
                        {b.w*0.065f,5.0f,0.7f},0,
                        game::rgb(255,198+((column+floor)%3)*12,126));
                }
            }
            continue;
        }
        model("primitive/box",{b.x+b.w*0.5f,0,b.z+b.d*0.5f},
            {b.w+24,0.12f,b.d+24},0,game::rgb(216,211,196));
        for(int row=0;row<2;++row)for(int col=0;col<2;++col){
            float width=b.w/2-7,depth=b.d/2-7;
            float x=b.x+b.w*(col+0.5f)/2,z=b.z+b.d*(row+0.5f)/2;
            int selection=int((index*11+row*7+col*13)%30);
            float height=b.h*(0.88f+0.13f*float((row+col+int(index))%3));
            std::string variant="buildings/urban-"+
                std::string(selection<10?"0":"")+std::to_string(selection);
            float yaw=(row+col)%2?game::PI/2:0;
            model(variant,{x,0.3f,z},{width,height,depth},yaw);
            if(std::sin((game::gameHour-6)*game::PI/12.0f)<0.12f&&
               close({x,z},350)){
                float co=std::cos(yaw),si=std::sin(yaw);
                for(int floor=0;floor<3;++floor)for(int column=0;column<3;++column){
                    if((int(index)+floor*5+column*7+row*3+col)%4==0)continue;
                    float sideways=(column-1)*width*0.23f;
                    glowBox({x+co*sideways-si*(depth*0.50f+0.5f),
                        12.0f+floor*height*0.24f,
                        z-si*sideways-co*(depth*0.50f+0.5f)},
                        {std::max(3.0f,width*0.08f),5.0f,0.7f},yaw,
                        game::rgb(255,197+((column+floor)%3)*11,132));
                }
            }
        }
        // Give each block a recognizable street-level frontage and roofline.
        if(game::len(Vec2{b.x+b.w*0.5f,b.z}-game::player)<470){
            const Color paint[]={game::rgb(56,111,116),game::rgb(151,89,61),
                game::rgb(105,92,139),game::rgb(126,111,68)};
            Color accent=paint[index%4];
            float front=b.z+2.6f;
            for(int col=0;col<2;++col){
                float center=b.x+b.w*(col+0.5f)*0.5f;
                model("primitive/box",{center,0,front},
                    {std::min(36.0f,b.w*0.30f),20,2.1f},0,
                    game::rgb(49,63,70));
                model("primitive/box",{center,20,front-4.0f},
                    {std::min(43.0f,b.w*0.36f),2.8f,9},0,accent);
                model("primitive/box",{center,23,front-1.8f},
                    {std::min(37.0f,b.w*0.31f),5.5f,1.5f},0,
                    game::rgb(212,196,154));
                model("primitive/box",{center,0,front-2.0f},
                    {8.5f,18,1.2f},0,game::rgb(117,82,57));
            }
            float roof=b.h*(0.88f+0.13f*float(int(index)%3))+0.3f;
            model("primitive/box",{b.x+b.w*0.25f,roof,b.z+b.d*0.25f},
                {std::min(18.0f,b.w*0.12f),7.0f,std::min(20.0f,b.d*0.13f)},0,
                game::rgb(88,94,92));
        }
    }
}
bool grassGround(Vec2 point,regions::Biome& biome){
    if(point.x<2||point.z<2||point.x>regions::WIDTH-2||
       point.z>regions::DEPTH-2)return false;
    if(point.x<game::WORLD_W&&point.z<game::WORLD_D){
        if(point.z>=game::BEACH_START)return false;
        for(int column=0;column<5;++column)
            if(std::abs(point.x-(300+column*450.0f))<game::ROAD_W*0.5f+3)
                return false;
        for(int row=0;row<4;++row)
            if(std::abs(point.z-(250+row*390.0f))<game::ROAD_W*0.5f+3)
                return false;
        biome=regions::Biome::City;
    }else{
        Vec2 tile{std::floor(point.x/100.0f)*100.0f+50.0f,
                  std::floor(point.z/100.0f)*100.0f+50.0f};
        if(regions::waterAt(tile)||regions::roadAt(tile)||
           regions::roadAt(point))return false;
        biome=regions::biomeAt(tile);
        if(biome!=regions::Biome::City&&
           biome!=regions::Biome::Countryside&&
           biome!=regions::Biome::Savanna)return false;
        const auto* region=regions::at(point);
        if(region&&region->id=="marina-part"){
            if(point.z>=8810&&point.z<=10015&&
               point.x>=8040&&point.x<=8137)return false;
            if(point.z>=10035&&point.z<=10710){
                float q=(point.z-10360.0f)/360.0f;
                float edge=8010.0f+770.0f*
                    std::sqrt(std::max(0.0f,1.0f-q*q));
                if(point.x>=edge+7&&point.x<=edge+54)return false;
            }
        }
    }
    return !game::solid(point,2.5f);
}
struct GrassTuft {Vec2 p;float width,height,yaw;Color tint;};
std::uint32_t grassHash(int x,int z,int variant){
    std::uint32_t value=std::uint32_t(x)*0x9e3779b9u ^
        std::uint32_t(z)*0x85ebca6bu ^std::uint32_t(variant)*0xc2b2ae35u;
    value^=value>>16;value*=0x7feb352du;
    value^=value>>15;value*=0x846ca68bu;
    return value^(value>>16);
}
float grassUnit(std::uint32_t value){return float(value&0xffffu)/65535.0f;}
void proceduralGrass(){
    if(ui::vegetationDensity==0)return;
    const float radius=40.0f+2.0f*ui::grassDistance;
    constexpr float cell=10.0f,recenter=20.0f;
    static int cacheX=std::numeric_limits<int>::min();
    static int cacheZ=std::numeric_limits<int>::min();
    static int cacheDistance=-1;
    static std::vector<GrassTuft> cached;
    int centerX=int(std::floor(game::player.x/recenter));
    int centerZ=int(std::floor(game::player.z/recenter));
    if(centerX!=cacheX||centerZ!=cacheZ||cacheDistance!=ui::grassDistance){
        cacheX=centerX;cacheZ=centerZ;cached.clear();
        cacheDistance=ui::grassDistance;
        Vec2 center{(centerX+0.5f)*recenter,(centerZ+0.5f)*recenter};
        float covered=radius+recenter;
        int x0=int(std::floor((center.x-covered)/cell));
        int x1=int(std::floor((center.x+covered)/cell));
        int z0=int(std::floor((center.z-covered)/cell));
        int z1=int(std::floor((center.z+covered)/cell));
        for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)
            for(int variant=0;variant<2;++variant){
                std::uint32_t seed=grassHash(x,z,variant+1);
                if(grassUnit(grassHash(x/4,z/4,variant+73))<0.26f)continue;
                Vec2 point{(x+grassUnit(seed))*cell,
                           (z+grassUnit(seed>>16))*cell};
                if(game::len(point-center)>covered)continue;
                regions::Biome biome{};
                if(!grassGround(point,biome))continue;
                std::uint32_t shape=grassHash(z,x,variant+17);
                float width=2.1f+grassUnit(shape)*2.3f;
                float height=1.9f+grassUnit(shape>>16)*3.2f;
                float yaw=grassUnit(grassHash(x,z,variant+39))*game::PI*2;
                int tone=int(shape%29);
                Color tint=biome==regions::Biome::Savanna?
                    game::rgb(119+tone,126+tone/2,65+tone/3):
                    game::rgb(68+tone/2,116+tone,57+tone/3);
                cached.push_back({point,width,height,yaw,tint});
            }
    }
    for(std::size_t index=0;index<cached.size();++index){
        if(ui::vegetationDensity==1&&index%2)continue;
        const auto& tuft=cached[index];
        float distance=game::len(tuft.p-game::player);
        if(distance>=radius)continue;
        float fade=std::clamp((radius-distance)/20.0f,0.0f,1.0f);
        model("primitive/grass-tuft",{tuft.p.x,0.02f,tuft.p.z},
            {tuft.width*fade,tuft.height*fade,tuft.width*fade},
            tuft.yaw,tuft.tint);
    }
}
void vegetation(){
    if(ui::vegetationDensity==0)return;
    proceduralGrass();
    auto bushName=[](int variant){
        variant%=36;
        return "nature/bush_"+std::string(variant<10?"0":"")+
            std::to_string(variant);
    };
    for(int index:regions::nearbyDecorationIndices(game::player,
        550*drawScale()+10)){
        if(index<0||std::size_t(index)>=regions::decorations().size())continue;
        const auto& prop=regions::decorations()[index];
        if(ui::vegetationDensity==1&&index%2)continue;
        if(!close(prop.p,550))continue;
        float distance=game::len(prop.p-game::player);
        if(distance>1400&&index%4!=0)continue;
        if(distance>2900&&index%12!=0)continue;
        model("nature/"+prop.modelId,{prop.p.x,0,prop.p.z},
            {prop.width,prop.height,prop.depth},float(index)*0.73f);
    }
    for(int index:regions::nearbyTreeIndices(game::player,850*drawScale()+10)){
        if(index<0||index>=int(game::trees.size()))continue;
        const auto& tree=game::trees[index];
        if(ui::vegetationDensity==1&&index%2)continue;
        if(!close(tree.p,tree.scale>=5?850:tree.palm?650:600))continue;
        float distance=game::len(tree.p-game::player);
        if(tree.scale<5&&distance>1800&&index%3!=0)continue;
        if(tree.scale<5&&distance>3300&&index%8!=0)continue;
        if(tree.destroyed){
            model("primitive/cylinder",{tree.p.x,0,tree.p.z},{8,13,8},0,
                game::rgb(44,40,37));continue;
        }
        float wear=std::clamp(tree.health/100.0f,0.25f,1.0f);
        Color shade{wear,wear,wear};
        float wind=game::len(weather::current().wind);
        float sway=std::sin(game::worldTime*(0.7f+wind)+tree.p.x*0.02f)*
            (0.025f+wind*0.035f);
        if(!tree.modelId.empty()){
            model("nature/"+tree.modelId,{tree.p.x,0,tree.p.z},
                {tree.crownWidth*tree.scale,tree.height*tree.scale,
                 tree.crownWidth*tree.scale},float(index)*0.43f+sway,shade);
        }
        else if(tree.palm)
            model(tree.variant==0?"nature/tree_palmDetailedShort":"nature/tree_palmDetailedTall",
                {tree.p.x,0,tree.p.z},
                {72*tree.scale,(tree.variant==0?70.0f:86.0f)*tree.scale,72*tree.scale},
                float(index)*0.43f+sway,shade);
        else{
            model(tree.variant==0?"nature/tree_detailed":"nature/tree_oak",
                {tree.p.x,0,tree.p.z},{54*tree.scale,68*tree.scale,54*tree.scale},
                float(index)*0.9f+sway,shade);
            if(ui::vegetationDensity==2&&close(tree.p,420)&&
               regions::biomeAt(tree.p)==regions::Biome::Countryside)
                model(bushName(index),{tree.p.x+18,0,tree.p.z-15},
                    {11,10,11},float(index)*0.9f+0.5f,game::rgb(185,217,176));
        }
    }
    if(ui::vegetationDensity==2){
        for(int i=0;i<70;++i){float x=35+float((i*137)%2280),z=game::BEACH_START+35+float((i*67)%210);
            if(close({x,z},400))model(i%4==0?bushName(i*13):"nature/grass_large",
                {x,0,z},{i%4==0?12.0f:8.0f,i%4==0?12.0f:7.0f,i%4==0?12.0f:8.0f},i*0.76f);}
    }
}
void character(Vec2 p,float angle,int style,bool armed,bool moving,bool running,float height=0,
               int actionOverride=-1,float actionWeight=1,bool playerControlled=false,
               float actionPhase=-1,int motion=0){
    const char* choices[]={"casual-man","hoodie-man","casual-woman","beach-man"};
    std::string name="characters/"+std::string(choices[style%4]);
    const Vec3 proportions[]={
        {14,34,14},{15,35,14},{13,32,13},{15,36,14}};
    Vec3 bodySize=proportions[style%4];
    if(playerControlled&&game::crouched)bodySize.y*=0.72f;
    Vec2 f=game::forward(angle),r{-f.z,f.x};
    Vec3 hand{p.x+f.x*6+r.x*6,height+(playerControlled&&game::crouched?15.0f:20.0f),p.z+f.z*6+r.z*6};
    Vec3 target{};
    float aimPitch=0;
    if(playerControlled&&armed){
        auto pose=camera::compute(p,height,true,-1);
        target=camera::traceReticle(pose,weapons::stats(game::weapon).range);
        Vec3 fromHand=target-hand;
        aimPitch=std::clamp(std::atan2(fromHand.y,
            std::sqrt(fromHand.x*fromHand.x+fromHand.z*fromHand.z)),-0.85f,0.8f);
    }
    int action=actionOverride>=0?actionOverride:armed?3:running?2:moving?1:0;
    float blend=actionOverride>=0?actionWeight:armed?1.0f:moving?0.9f:0.0f;
    bool detailed=game::len(p-game::player)<220*ui::lodDistanceScale();
    if(!(detailed&&skinnedCharacter(name,{p.x,height,p.z},bodySize,game::PI/2-angle,
            action,blend,&hand,actionPhase,motion,aimPitch))){
        if(!detailed)name+="-lod";
        else if(armed)name+="-aim";
        else if(moving)name+=(running?"-run":"-walk")+std::to_string((int(game::worldTime*(running?10:6)+p.x))%4);
        model(name,{p.x,height,p.z},bodySize,game::PI/2-angle);
    }
    bool meleeHeld=playerControlled&&weapons::stats(game::weapon).melee;
    if(armed||meleeHeld){
        if(playerControlled&&weapons::stats(game::weapon).arrow){
            Vec3 across{-f.z*12,0,f.x*12};
            Vec3 middle=hand+Vec3{f.x*8,0,f.z*8};
            beam(middle,middle+across+Vec3{0,15,0},2.0f,2.0f,game::rgb(132,86,48));
            beam(middle,middle-across+Vec3{0,-15,0},2.0f,2.0f,game::rgb(132,86,48));
            beam(middle+across+Vec3{0,15,0},
                middle-across+Vec3{0,-15,0},0.6f,0.6f,game::rgb(225,218,186));
            return;
        }
        if(meleeHeld){
            const std::string& id=weapons::stats(game::weapon).id;
            float reach=id=="katana"?32:id=="knife"?14:id=="machete"?25:
                id=="bat"?30:id=="rolling-pin"?20:18;
            float thickness=id=="katana"||id=="knife"?1.5f:id=="machete"?2.8f:4.0f;
            Color tint=id=="novelty-toy"?game::rgb(220,102,163):
                id=="bat"||id=="rolling-pin"?game::rgb(179,128,79):game::rgb(185,196,202);
            beam(hand,hand+Vec3{f.x*reach,reach*0.15f,f.z*reach},
                thickness,thickness,tint);
            return;
        }
        Vec3 muzzle=playerControlled?
            camera::weaponMuzzle(p,height,angle,target):
            Vec3{p.x+f.x*12,height+18,p.z+f.z*12};
        const std::string& weaponId=weapons::stats(playerControlled?game::weapon:0).id;
        const char* asset=weaponId=="pistol"||weaponId=="silenced-pistol"||weaponId=="smg"?
            "weapons/pistol":weaponId=="shotgun"||weaponId=="sniper"?
            "weapons/lightning":weaponId=="rifle"?"weapons/ak":nullptr;
        float width=weaponId=="pistol"||weaponId=="silenced-pistol"||weaponId=="smg"?2.5f:
            weaponId=="rifle"?6.0f:2.4f;
        float gunHeight=weaponId=="pistol"||weaponId=="silenced-pistol"||weaponId=="smg"?5.5f:
            weaponId=="rifle"?6.5f:4.5f;
        if(!asset||!heldWeapon(asset,hand,muzzle,width,gunHeight))
            beam(hand,muzzle,3.6f,3.0f,game::rgb(46,48,52));
        if(playerControlled&&game::dualWieldActive(game::weapon)){
            Vec3 leftHand=hand+Vec3{-r.x*13,0,-r.z*13};
            Vec3 leftMuzzle=muzzle+Vec3{-r.x*14,0,-r.z*14};
            if(!asset||!heldWeapon(asset,leftHand,leftMuzzle,width,gunHeight))
                beam(leftHand,leftMuzzle,3.6f,3.0f,game::rgb(46,48,52));
        }
    }
}
void people(){
    for(const auto& ped:game::peds){
        if(!close(ped.p,420))continue;
        if(!ped.alive){
            if(ped.pinned){
                if(ped.corpseVisualDelay<=0)
                    character(ped.p,ped.angle,ped.style,false,false,false,0,5,1);
                beam({ped.p.x,21,ped.p.z},
                    {ped.pinAnchor.x,21,ped.pinAnchor.z},1.3f,1.3f,
                    game::rgb(129,85,48));
            }else if(ped.corpseVisualDelay<=0||ped.carried)
                character(ped.p,ped.angle,ped.style,false,false,false,
                    ped.carried?10.0f:0.0f,5,1.0f);
            continue;
        }
        if(ped.knockedDown>0){
            float phase=ped.impactAnimationTotal>0?
                std::clamp(1.0f-ped.knockedDown/ped.impactAnimationTotal,0.0f,1.0f):1.0f;
            character(ped.p,ped.angle,ped.style,false,false,false,0,8,1.0f,false,phase);
            continue;
        }
        character(ped.p,ped.angle,ped.style,ped.armed,ped.panic>0||game::len(ped.target-ped.p)>10,
            ped.panic>0,0,ped.hitFlash>0?4:-1,std::min(1.0f,ped.hitFlash*8));
    }
    if(game::occupied<0&&game::health>0&&!camera::firstPersonActive()){
        bool entering=game::enteringVehicle>=0&&
            std::size_t(game::enteringVehicle)<game::vehicles.size();
        bool climbing=traversal::active();
        bool falling=!game::grounded&&!game::swimming&&!climbing&&
            !debug_menu::flyMode&&game::playerY>0.5f;
        Vec2 shown=game::player;
        float shownY=game::playerY-(game::swimming?13.0f:0.0f);
        float shownAngle=game::cameraYaw;
        float actionPhase=-1;
        if(entering){
            actionPhase=std::clamp(1.0f-game::vehicleEntryTime/0.65f,0.0f,1.0f);
            float slide=actionPhase*actionPhase*(3-2*actionPhase);
            const auto& vehicle=game::vehicles[game::enteringVehicle];
            Vec2 facing=game::forward(vehicle.angle);
            Vec2 side{-facing.z,facing.x};
            float sideSign=(game::player-vehicle.p).x*side.x+
                (game::player-vehicle.p).z*side.z>=0?1.0f:-1.0f;
            Vec2 doorway=vehicle.p+side*(sideSign*(vehicle.kind==game::Kind::Bike?14.0f:21.0f))-
                facing*(vehicle.kind==game::Kind::Boat?5.0f:3.0f);
            shown=shown+(doorway-shown)*slide;
            shownY-=slide*3.0f;
            shownAngle=std::atan2(vehicle.p.z-shown.z,vehicle.p.x-shown.x);
        }else if(falling)actionPhase=std::clamp(game::airTime/0.65f,0.0f,1.0f);
        bool moving=game::len(game::playerVelocity)>15;
        int motion=game::swimming?1:climbing?2:falling?3:0;
        int action=entering?9:game::swimming?2:climbing?1:falling?2:
            game::muzzleFlash>0||game::meleeVisualTime>0?6:game::reloadRemaining>0?7:-1;
        float weight=entering?1.0f:game::swimming||climbing?0.6f:
            falling?std::min(1.0f,game::airTime*6):
            game::muzzleFlash>0||game::meleeVisualTime>0?1.0f:
            game::reloadRemaining>0?0.75f:1.0f;
        character(shown,shownAngle,1,game::rightMouse&&!entering&&
            game::meleeVisualTime<=0,moving,
            !game::crouched&&game::len(game::playerVelocity)>205,
            shownY,action,weight,true,actionPhase,motion);
    }
}
void carLamp(Vec2 center,float y,Vec2 facing,Vec2 side,float width,float height,
             bool front,Color tint){
    Vec2 across=side*(front?width*0.5f:-width*0.5f);
    Vec3 a{center.x-across.x,y-height*0.5f,center.z-across.z};
    Vec3 b{center.x+across.x,y-height*0.5f,center.z+across.z};
    Vec3 c{b.x,y+height*0.5f,b.z};
    Vec3 d{a.x,y+height*0.5f,a.z};
    Vec3 normal{facing.x*(front?1.0f:-1.0f),0,
                facing.z*(front?1.0f:-1.0f)};
    quad(14,a,b,c,d,normal,tint);
}
float carLampDepth(const Mesh& body,Vec3 size,float x,float y,bool front){
    float sx=size.x/std::max(0.01f,body.maxX-body.minX);
    float sy=size.y/std::max(0.01f,body.maxY-body.minY);
    float sz=size.z/std::max(0.01f,body.maxZ-body.minZ);
    float cx=(body.minX+body.maxX)*0.5f;
    float cz=(body.minZ+body.maxZ)*0.5f;
    float depth=front?-std::numeric_limits<float>::max():
        std::numeric_limits<float>::max();
    for(std::size_t i=0;i+2<body.vertices.size();i+=3){
        const auto& a=body.vertices[i];
        const auto& b=body.vertices[i+1];
        const auto& c=body.vertices[i+2];
        float ax=(a.x-cx)*sx,ay=(a.y-body.minY)*sy;
        float bx=(b.x-cx)*sx,by=(b.y-body.minY)*sy;
        float px=(c.x-cx)*sx,py=(c.y-body.minY)*sy;
        float determinant=(by-py)*(ax-px)+(px-bx)*(ay-py);
        if(std::abs(determinant)<0.0001f)continue;
        float wa=((by-py)*(x-px)+(px-bx)*(y-py))/determinant;
        float wb=((py-ay)*(x-px)+(ax-px)*(y-py))/determinant;
        float wc=1-wa-wb;
        if(wa<-0.01f||wb<-0.01f||wc<-0.01f)continue;
        float z=(wa*a.z+wb*b.z+wc*c.z-cz)*sz;
        if(front)depth=std::max(depth,z);
        else depth=std::min(depth,z);
    }
    if(std::abs(depth)>size.z)return front?size.z*0.48f:-size.z*0.48f;
    return depth+(front?0.12f:-0.12f);
}
void vehicles(){
    static std::unordered_map<const Mesh*,std::array<float,4>> lampDepths;
    for(const auto& v:game::vehicles){
        if(!close(v.p,650))continue;
        Color paint=v.exploded?game::rgb(38,39,41):v.c;
        Vec2 facing=game::forward(v.angle);
        float yaw=game::PI/2-v.angle;
        if(v.kind==game::Kind::Car||v.kind==game::Kind::SportCar){
            std::uint32_t key=2166136261u;
            for(unsigned char ch:v.id)key=(key^ch)*16777619u;
            int variant=int(key%5)+1;
            std::string carName=v.kind==game::Kind::SportCar?
                "vehicles/sports-car":"vehicles/traffic-"+std::to_string(variant);
            if(!mesh(carName))carName="vehicles/sedan";
            const float carHeights[]={20,23,23,21,26,22};
            Color tint=v.exploded?game::rgb(65,65,65):Color{1,1,1};
            model(carName,{v.p.x,v.rideHeight,v.p.z},
                {26,carHeights[v.kind==game::Kind::SportCar?0:variant],48},yaw,tint);
            if(std::sin((game::gameHour-6)*game::PI/12.0f)<0.12f&&!v.exploded){
                Vec2 side{-facing.z,facing.x};
                const float sideOffsets[]={8.0f,9.2f,7.6f,8.9f,8.4f,8.0f};
                const float frontHeights[]={10.0f,7.8f,10.1f,8.2f,10.2f,9.6f};
                int geometry=v.kind==game::Kind::SportCar?0:variant;
                const Mesh* body=mesh(carName);
                auto found=lampDepths.find(body);
                if(found==lampDepths.end()){
                    std::array<float,4> depths{};
                    Vec3 dimensions{26,carHeights[geometry],48};
                    for(int sideIndex=0;sideIndex<2;++sideIndex){
                        float x=(sideIndex==0?-1.0f:1.0f)*sideOffsets[geometry];
                        depths[sideIndex]=carLampDepth(*body,dimensions,x,
                            frontHeights[geometry],true);
                        depths[sideIndex+2]=carLampDepth(*body,dimensions,x,9.7f,false);
                    }
                    found=lampDepths.emplace(body,depths).first;
                }
                float blink=std::fmod(game::worldTime,0.9f)<0.18f?1.3f:0.18f;
                for(int sideIndex=0;sideIndex<2;++sideIndex){
                    float sign=sideIndex==0?-1.0f:1.0f;
                    Vec2 front=v.p+facing*found->second[sideIndex]+
                        side*(sign*sideOffsets[geometry]);
                    Vec2 rear=v.p+facing*found->second[sideIndex+2]+
                        side*(sign*sideOffsets[geometry]);
                    carLamp(front,v.rideHeight+frontHeights[geometry],facing,side,
                        3.0f,1.7f,true,{blink,blink*0.94f,blink*0.76f});
                    carLamp(rear,v.rideHeight+9.7f,facing,side,
                        3.1f,1.7f,false,{blink,blink*0.12f,blink*0.08f});
                }
            }
        }else if(v.kind==game::Kind::Bike){
            for(float offset:{-12.0f,12.0f}){
                Vec3 wheel{v.p.x+facing.x*offset,v.rideHeight+1,
                    v.p.z+facing.z*offset};
                model("primitive/sphere",wheel,{3.4f,15,15},yaw,game::rgb(25,29,32));
                model("primitive/sphere",{wheel.x,wheel.y+3,wheel.z},
                    {4,6,6},yaw,game::rgb(150,158,163));
            }
            model("primitive/box",{v.p.x,v.rideHeight+11,v.p.z},
                {5,4,22},yaw,game::rgb(65,69,72));
            model("primitive/box",{v.p.x-facing.x*5,v.rideHeight+19,
                v.p.z-facing.z*5},{7,2.5f,10},yaw,game::rgb(37,39,42));
            model("primitive/box",{v.p.x+facing.x*5,v.rideHeight+16,
                v.p.z+facing.z*5},{8,5,10},yaw,paint);
            model("primitive/box",{v.p.x,v.rideHeight+13,v.p.z},
                {6,6,7},yaw,game::rgb(105,110,111));
            Vec3 front{v.p.x+facing.x*12,v.rideHeight+9,v.p.z+facing.z*12};
            Vec3 steering{v.p.x+facing.x*10,v.rideHeight+26,v.p.z+facing.z*10};
            beam(front,steering,2.2f,2.2f,game::rgb(157,163,166));
            Vec2 side{-facing.z,facing.x};
            beam({steering.x-side.x*7,steering.y,steering.z-side.z*7},
                 {steering.x+side.x*7,steering.y,steering.z+side.z*7},
                 2.0f,2.0f,game::rgb(45,47,49));
            sphere({steering.x+facing.x*2,steering.y-3,steering.z+facing.z*2},
                3.0f,game::rgb(246,224,170));
            if(std::sin((game::gameHour-6)*game::PI/12.0f)<0.12f&&!v.exploded)
                glowBox({front.x,front.y+8,front.z},{4,4,2},yaw,
                    game::rgb(255,239,189));
        }else{
            model("vehicles/motorboat",{v.p.x,v.rideHeight-2,v.p.z},{24,22,48},yaw);
        }
    }
}
void marker(Vec2 p,float radius,float height,Color tint){
    float pulse=1.0f+0.045f*std::sin(game::worldTime*3.0f);
    model("marker/ring",{p.x,0.45f,p.z},{radius*2*pulse,0.01f,radius*2*pulse},0,tint);
    model("marker/pillar",{p.x,0.5f,p.z},{radius*2,height,radius*2},0,tint);
}
void markerArrow(Vec2 p,float height,Color tint,Vec2 direction){
    Vec2 f=game::norm(direction);
    if(game::len(f)<0.01f)f={0,1};
    Vec2 side{-f.z,f.x};
    Vec3 tip{p.x+f.x*10,height+2.0f+std::sin(game::worldTime*2.5f)*2.0f,
        p.z+f.z*10};
    Vec3 left{p.x-f.x*6+side.x*8,tip.y,p.z-f.z*6+side.z*8};
    Vec3 right{p.x-f.x*6-side.x*8,tip.y,p.z-f.z*6-side.z*8};
    beam(left,tip,1.8f,1.8f,tint);
    beam(right,tip,1.8f,1.8f,tint);
}
void markers(){
    for(const auto& ladder:traversal::ladders)if(close(ladder.bottom,650)){
        Vec3 left{ladder.bottom.x-6,0,ladder.bottom.z};
        Vec3 right{ladder.bottom.x+6,0,ladder.bottom.z};
        beam(left,left+Vec3{0,ladder.height,0},1.5f,1.5f,game::rgb(166,177,175));
        beam(right,right+Vec3{0,ladder.height,0},1.5f,1.5f,game::rgb(166,177,175));
        for(float height=8;height<ladder.height;height+=12)
            beam(left+Vec3{0,height,0},right+Vec3{0,height,0},1.3f,1.3f,
                game::rgb(195,204,198));
        sphere({ladder.bottom.x,7,ladder.bottom.z},4,game::rgb(253,211,108));
    }
    for(const auto& tree:traversal::trees)if(close(tree.bottom,tree.generated?80:650)){
        if(tree.treeIndex>=0&&tree.treeIndex<int(game::trees.size())&&
           !game::trees[tree.treeIndex].destroyed)
            sphere({tree.bottom.x,7,tree.bottom.z},4,game::rgb(253,211,108));
    }
    for(const auto& pickup:game::pickups)if(pickup.available&&close(pickup.p,650))
        sphere({pickup.p.x,13+std::sin(game::worldTime*3)*3,pickup.p.z},5,game::rgb(84,238,235));
    for(const auto& mission:game::missions)if(close(mission.start,650)){
        Color tint=game::rgb(244,174,67);
        marker(mission.start,17,38,tint);
        markerArrow(mission.start,42,tint,{0,1});
    }
    for(const auto& shop:commerce::shops)if(close(shop.p,650)){
        marker(shop.p,14,26,game::rgb(75,228,145));
    }
    for(const auto& house:commerce::houses)if(close(house.p,650)){
        Color tint=house.owned?game::rgb(91,170,245):game::rgb(185,134,230);
        marker(house.p,14,26,tint);
    }
    if(game::activeMission>=0&&game::missionStep<int(game::missions[game::activeMission].goals.size())){
        Vec2 goal=game::missions[game::activeMission].goals[game::missionStep];
        bool target=game::missions[game::activeMission].kind==game::MissionKind::Targets||
            (game::missions[game::activeMission].kind==game::MissionKind::Finale&&game::missionStep==1);
        if(close(goal,650)){
            Color tint=target?game::rgb(255,104,70):game::rgb(255,211,78);
            marker(goal,target?20.0f:42.0f,target?42.0f:78.0f,tint);
            const auto& goals=game::missions[game::activeMission].goals;
            Vec2 next=game::missionStep+1<int(goals.size())?
                goals[game::missionStep+1]-goal:Vec2{0,1};
            markerArrow(goal,target?47.0f:82.0f,tint,next);
        }
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
struct EffectHandler {
    void sprite(const char* type,Vec3 bottom,float width,float height,float yaw,
                Color tint={1,1,1}){
        model(type,bottom,{width,height,width},yaw,tint);
    }
    void fireColumn(Vec3 base,float intensity,std::uint32_t seed){
        float phase=game::worldTime*11.0f+float(seed%101)*0.23f;
        int layers=ui::effectsQuality==2?2:1;
        for(int layer=0;layer<layers;++layer){
            float variation=float((seed>>(layer*7))&15u)/15.0f;
            float wave=std::sin(phase*(0.84f+variation*0.31f)+layer*2.3f);
            float width=(9.0f+intensity*8.0f)*(0.78f+variation*0.46f)*
                (layer==0?1.0f:0.64f);
            float height=(15.0f+intensity*14.0f)*(0.77f+variation*0.28f+wave*0.12f);
            Vec3 origin=base+Vec3{
                std::sin(phase*0.55f+layer*3.0f+variation)*3.3f,
                float(layer)*1.9f,
                std::cos(phase*0.47f+layer+variation)*3.3f};
            sprite("effect/flame",origin,width,height,
                variation*game::PI+phase*0.13f+layer,
                layer==0?Color{1,0.73f+variation*0.17f,0.43f+variation*0.24f}:
                    Color{1,0.94f,0.77f});
        }
        if(ui::effectsQuality>0&&seed%2==0){
            float rise=std::fmod(game::worldTime*15.0f+float(seed%29),31.0f);
            sprite("effect/smoke",base+Vec3{rise*0.2f,17+rise,rise*0.14f},
                15+rise*0.45f,19+rise*0.43f,phase*0.06f,
                game::rgb(165,167,170));
        }
    }
    void explosion(const game::Blast& blast){
        float progress=std::clamp(1.0f-blast.life/0.75f,0.0f,1.0f);
        float radius=std::clamp(blast.radius,25.0f,130.0f);
        if(progress<0.56f){
            float burst=std::sin(progress/0.56f*game::PI);
            sprite("effect/flash",blast.p-Vec3{0,radius*0.19f,0},
                radius*(0.32f+burst*0.85f),radius*(0.32f+burst*0.72f),0,
                game::rgb(255,226,159));
        }
        if(progress<0.72f)
            model("effect/shockwave",
                {blast.p.x,blast.p.y-radius*0.12f,blast.p.z},
                {radius*(0.48f+progress*1.6f),1,
                 radius*(0.48f+progress*1.6f)},0);
        int plumes=ui::effectsQuality>0?8:4;
        for(int index=0;index<plumes;++index){
            float angle=(float(index)/plumes)*game::PI*2+0.37f;
            float reach=radius*(0.08f+progress*0.40f);
            Vec3 center=blast.p+Vec3{std::cos(angle)*reach,-blast.p.y*0.32f+
                progress*radius*(0.03f+float(index%3)*0.03f),
                std::sin(angle)*reach};
            if(progress<0.62f)
                sprite("effect/flame",center,
                    radius*(0.32f+progress*0.22f),
                    radius*(0.24f+progress*0.16f),angle,
                    game::rgb(255,188+index%3*10,125));
            sprite("effect/smoke",center+Vec3{0,radius*0.13f,0},
                radius*(0.29f+progress*0.30f),
                radius*(0.25f+progress*0.30f),angle+0.6f,
                game::rgb(169,164,158));
        }
    }
    void impact(const game::HitFlash& flash){
        float life=std::clamp(flash.life/0.22f,0.0f,1.0f);
        if(flash.person){
            sprite("effect/blood",flash.p-Vec3{0,5.0f,0},
                8.0f+life*10.0f,9.0f+life*9.0f,
                flash.p.x*0.17f,Color{1,1,1});
        }else sprite("effect/flash",flash.p-Vec3{0,2.0f,0},
            3.0f+life*7.0f,3.0f+life*7.0f,
            flash.p.x*0.17f,flash.c);
    }
    void muzzle(Vec3 position){
        sprite("effect/flash",position-Vec3{0,5,0},
            11,11,game::worldTime*9,game::rgb(255,236,185));
    }
    void projectile(const game::Bullet& bullet){
        Vec3 direction=game::norm(bullet.v);
        if(bullet.streamType>0){
            const char* type=bullet.streamType==1?"effect/flame":"effect/smoke";
            Color tint=bullet.streamType==3?game::rgb(125,207,255):Color{1,1,1};
            sprite(type,bullet.p-Vec3{0,3,0},
                bullet.streamType==1?7.0f:5.0f,9.0f,
                game::worldTime*7,tint);
            return;
        }
        const Mesh* source=mesh("primitive/bullet");
        if(!source||!modelInstances)return;
        float horizontal=std::sqrt(direction.x*direction.x+direction.z*direction.z);
        float cosYaw=horizontal>0.001f?direction.z/horizontal:1.0f;
        float sinYaw=horizontal>0.001f?direction.x/horizontal:0.0f;
        float diameter=bullet.rocket?5.0f:bullet.arrow?1.9f:2.2f;
        float length=bullet.rocket?16.0f:bullet.arrow?18.0f:10.0f;
        Color tint=bullet.rocket?game::rgb(143,157,158):
            bullet.arrow?game::rgb(151,110,68):Color{1,1,1};
        modelInstances->push_back({source,0,diameter,diameter,length,
            cosYaw,sinYaw,bullet.p.x,bullet.p.y,bullet.p.z,
            0,0.5f,0,tint.r,tint.g,tint.b,
            direction.y,horizontal});
        if(bullet.rocket){
            Vec3 trail=bullet.p-direction*14;
            sprite("effect/flame",trail-Vec3{0,5,0},11,16,
                game::worldTime*8,game::rgb(255,201,119));
            sprite("effect/smoke",trail-direction*15-Vec3{0,7,0},
                13,16,game::worldTime*3);
        }
    }
};
void effects(){
    EffectHandler handler;
    const auto& conditions=weather::current();
    if(conditions.precipitation>0){
        int count=int(conditions.precipitation*120);
        for(int index=0;index<count;++index){
            float x=game::player.x-130+float((index*79)%260);
            float z=game::player.z-130+float((index*137)%260);
            if(game::len(Vec2{x-game::player.x,z-game::player.z})<55)continue;
            float speed=conditions.snow?24.0f:130.0f;
            float y=std::fmod(float((index*59)%125)+
                game::worldTime*speed,125.0f);
            if(conditions.snow)
                sphere({x,125-y,z},1.7f,game::rgb(240,244,245));
            else beam({x,125-y,z},
                {x+conditions.wind.x,120-y,z+conditions.wind.z},
                0.2f,0.2f,game::rgb(137,170,192));
        }
    }
    for(const auto& flame:fire::active())if(close(flame.p,500))
        handler.fireColumn({flame.p.x,0,flame.p.z},flame.intensity,
            grassHash(int(flame.p.x),int(flame.p.z),41));
    for(const auto& vehicle:game::vehicles)
        if(close(vehicle.p,500)&&
           (vehicle.exploded||vehicle.damage>=physics::tuning(vehicle.kind).smokeThreshold)){
            float rise=std::fmod(game::worldTime*12.0f+vehicle.p.x*0.17f,35.0f);
            handler.sprite("effect/smoke",
                {vehicle.p.x+rise*0.18f,vehicle.rideHeight+19+rise,vehicle.p.z},
                18+rise*0.4f,24+rise*0.35f,
                game::worldTime*0.3f,game::rgb(153,155,159));
        }
    if(game::muzzleFlash>0){
        handler.muzzle(game::lastMuzzle);
        if(game::occupied<0&&game::dualWieldActive(game::weapon))
            handler.muzzle(game::lastMuzzleLeft);
    }
    for(const auto& bullet:game::bullets)if(close({bullet.p.x,bullet.p.z},550)){
        handler.projectile(bullet);
    }
    for(const auto& blast:game::blasts)if(close({blast.p.x,blast.p.z},550))
        handler.explosion(blast);
    for(std::size_t index=0;index+5<game::ragdollParts.size();index+=6)
        if(close({game::ragdollParts[index].p.x,game::ragdollParts[index].p.z},500))
            ragdollMesh(game::ragdollParts.data()+index);
    if(ui::effectsQuality>0){
        for(const auto& flash:game::hitFlashes)
            if(close({flash.p.x,flash.p.z},500))
                handler.impact(flash);
        for(const auto& part:game::debris)if(close({part.p.x,part.p.z},500))
            box(part.p.x,part.p.y-part.h/2,part.p.z,part.w,part.h,part.d,
                part.tile>=24?game::rgb(55,66,82):game::rgb(161,91,86));
        for(const auto& impact:game::impacts)if(impact.person&&close(impact.p,500)){
            float size=15.0f+3.0f*(1.0f-std::clamp(impact.life/1.8f,0.0f,1.0f));
            model("effect/blood-decal",{impact.p.x,0.55f,impact.p.z},
                {size,0.01f,size},impact.p.x*0.13f,Color{0.55f,0.34f,0.34f});
        }
    }
    for(const auto& prop:game::props)if(prop.alive&&close(prop.p,500)){
        if(prop.barrel){
            model("primitive/cylinder",{prop.p.x,prop.y,prop.p.z},
                {19,23,19},0,game::rgb(88,111,116));
            model("primitive/cylinder",{prop.p.x,prop.y+17,prop.p.z},
                {19.5f,2.2f,19.5f},0,game::rgb(48,68,73));
        }else model("primitive/box",{prop.p.x,prop.y,prop.p.z},
            {23,22,23},0,game::rgb(155,124,87));
    }
}
}
void buildStaticScene(std::vector<Vertex> groups[MATERIAL_GROUPS]){
    buckets=groups;
    for(int i=0;i<MATERIAL_GROUPS;++i)groups[i].clear();
    ground(0,0,game::WORLD_W,game::BEACH_START,0,game::rgb(150,195,145),-1,9);
    ground(0,game::BEACH_START,game::WORLD_W,game::SHORE,0.05f,game::rgb(244,218,166),-1,8);
    ground(0,game::SHORE,game::WORLD_W,game::WORLD_D,-0.4f,
        game::rgb(86,160,197),-1,3);
    ground(1140,game::BEACH_START,1260,game::WORLD_D,0.15f,
        game::rgb(112,114,116),-1,7);
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
}
void buildScene(std::vector<Vertex> groups[MATERIAL_GROUPS],std::vector<ModelInstance>& instances){
    buckets=groups;modelInstances=&instances;instances.clear();
    for(int i=0;i<MATERIAL_GROUPS;++i)groups[i].clear();
    regionalTerrain();marinaScenery();weatherGroundDetails();nightSky();clouds();buildings();vegetation();streetlights();
    vehicles();people();markers();effects();
    float solar=std::sin((game::gameHour-6)*game::PI/12),angle=(game::gameHour-6)*game::PI/12;
    if(solar>=0)
        sphere({game::player.x+std::cos(angle)*820,solar*600+130,
            game::player.z-160},47.0f,game::rgb(255,230,151));
}
}
