#include "dx11_assets.h"
#include "game.h"
#include "masonry.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>

namespace dx11 {
namespace {
std::unordered_map<std::string,Mesh> meshes;
std::unordered_map<std::string,SkinMesh> skins;
std::unordered_map<std::string,LodChain> lodChains;
std::unordered_map<const Mesh*,LodChain> shadowLods;
std::vector<std::string> issues;
Mesh distantNature(const Mesh& original,bool tree){
    Mesh lod;
    lod.minX=original.minX;lod.maxX=original.maxX;
    lod.minY=original.minY;lod.maxY=original.maxY;
    lod.minZ=original.minZ;lod.maxZ=original.maxZ;
    const float cx=(lod.minX+lod.maxX)*0.5f,cz=(lod.minZ+lod.maxZ)*0.5f;
    const float width=std::max(0.01f,lod.maxX-lod.minX);
    const float depth=std::max(0.01f,lod.maxZ-lod.minZ);
    const float height=std::max(0.01f,lod.maxY-lod.minY);
    auto face=[&](float ax,float ay,float az,float bx,float by,float bz,
                  float cx0,float cy,float cz0,float nx,float ny,float nz,
                  float r,float g,float b){
        lod.vertices.push_back({ax,ay,az,nx,ny,nz,0,0,r,g,b,1});
        lod.vertices.push_back({bx,by,bz,nx,ny,nz,0,0,r,g,b,1});
        lod.vertices.push_back({cx0,cy,cz0,nx,ny,nz,0,0,r,g,b,1});
    };
    float base=tree?lod.minY+height*0.52f:lod.minY+height*0.1f;
    if(tree){
        float radius=std::min(width,depth)*0.075f;
        for(int side=0;side<4;++side){
            float a=side*1.5707963f,b=(side+1)*1.5707963f;
            float x0=cx+std::cos(a)*radius,z0=cz+std::sin(a)*radius;
            float x1=cx+std::cos(b)*radius,z1=cz+std::sin(b)*radius;
            face(x0,lod.minY,z0,x1,lod.minY,z1,x1,base,z1,0,0,1,0.36f,0.24f,0.13f);
            face(x0,lod.minY,z0,x1,base,z1,x0,base,z0,0,0,1,0.36f,0.24f,0.13f);
        }
    }
    const float tip=lod.maxY,mid=base+(tip-base)*0.45f;
    const float rx=width*0.48f,rz=depth*0.48f;
    for(int side=0;side<4;++side){
        float a=side*1.5707963f,b=(side+1)*1.5707963f;
        float x0=cx+std::cos(a)*rx,z0=cz+std::sin(a)*rz;
        float x1=cx+std::cos(b)*rx,z1=cz+std::sin(b)*rz;
        face(cx,tip,cz,x0,mid,z0,x1,mid,z1,0,0.7f,0,0.24f,0.51f,0.21f);
        face(cx,base,cz,x1,mid,z1,x0,mid,z0,0,-0.6f,0,0.20f,0.42f,0.17f);
    }
    return lod;
}
Mesh distantBox(const Mesh& original,float r,float g,float b){
    Mesh result;result.minX=original.minX;result.minY=original.minY;
    result.minZ=original.minZ;result.maxX=original.maxX;
    result.maxY=original.maxY;result.maxZ=original.maxZ;
    result.textured=original.textured;
    result.alphaTest=original.alphaTest;
    result.textureFile=original.textureFile;
    auto quad=[&](float ax,float ay,float az,float bx,float by,float bz,
                  float cx,float cy,float cz,float dx,float dy,float dz,
                  float nx,float ny,float nz,float shade){
        auto v=[&](float x,float y,float z,float u,float vv){
            return Vertex{x,y,z,nx,ny,nz,u,vv,r*shade,g*shade,b*shade,1};};
        result.vertices.push_back(v(ax,ay,az,0,1));
        result.vertices.push_back(v(bx,by,bz,1,1));
        result.vertices.push_back(v(cx,cy,cz,1,0));
        result.vertices.push_back(v(ax,ay,az,0,1));
        result.vertices.push_back(v(cx,cy,cz,1,0));
        result.vertices.push_back(v(dx,dy,dz,0,0));
    };
    float x0=result.minX,x1=result.maxX,y0=result.minY,y1=result.maxY;
    float z0=result.minZ,z1=result.maxZ;
    quad(x0,y0,z0,x1,y0,z0,x1,y1,z0,x0,y1,z0,0,0,-1,0.86f);
    quad(x1,y0,z1,x0,y0,z1,x0,y1,z1,x1,y1,z1,0,0,1,0.95f);
    quad(x0,y0,z1,x0,y0,z0,x0,y1,z0,x0,y1,z1,-1,0,0,0.78f);
    quad(x1,y0,z0,x1,y0,z1,x1,y1,z1,x1,y1,z0,1,0,0,0.90f);
    quad(x0,y1,z0,x1,y1,z0,x1,y1,z1,x0,y1,z1,0,1,0,1.12f);
    return result;
}
Mesh distantPerson(const Mesh& original,int style){
    Mesh result;result.minX=original.minX;result.minY=original.minY;
    result.minZ=original.minZ;result.maxX=original.maxX;
    result.maxY=original.maxY;result.maxZ=original.maxZ;
    float x=(result.minX+result.maxX)*0.5f,z=(result.minZ+result.maxZ)*0.5f;
    float w=result.maxX-result.minX,h=result.maxY-result.minY,d=result.maxZ-result.minZ;
    auto add=[&](float centerX,float bottom,float centerZ,float width,float height,float depth,
                 float r,float g,float b){
        Mesh part=original;
        part.minX=centerX-width*0.5f;part.maxX=centerX+width*0.5f;
        part.minY=bottom;part.maxY=bottom+height;
        part.minZ=centerZ-depth*0.5f;part.maxZ=centerZ+depth*0.5f;
        part.vertices.clear();part.textured=false;
        Mesh box=distantBox(part,r,g,b);
        result.vertices.insert(result.vertices.end(),box.vertices.begin(),box.vertices.end());
    };
    float shirtR=style==1?0.15f:style==2?0.58f:0.32f;
    float shirtG=style==1?0.20f:style==2?0.38f:0.40f;
    float shirtB=style==1?0.26f:style==2?0.52f:0.36f;
    add(x,result.minY+h*0.40f,z,w*0.52f,h*0.38f,d*0.55f,shirtR,shirtG,shirtB);
    add(x,result.minY+h*0.78f,z,w*0.37f,h*0.21f,d*0.42f,0.72f,0.53f,0.40f);
    for(int sign:{-1,1}){
        add(x+sign*w*0.35f,result.minY+h*0.44f,z,w*0.17f,h*0.31f,d*0.35f,
            0.66f,0.49f,0.37f);
        add(x+sign*w*0.15f,result.minY,z,w*0.20f,h*0.43f,d*0.38f,
            0.18f,0.22f,0.29f);
    }
    return result;
}
}
bool chooseDetailedLod(float pixels,float distance,float threshold,float cap,
                       bool hasPrevious,bool previousDetailed){
    if(!hasPrevious)return pixels>=threshold&&distance<=cap;
    if(previousDetailed)return pixels>=threshold*0.88f&&distance<=cap*1.10f;
    return pixels>threshold*1.12f&&distance<cap*0.90f;
}
BoundingSphere instanceBounds(const ModelInstance& model){
    const Mesh& mesh=*model.source;
    const float localX=(mesh.minX+mesh.maxX)*0.5f-model.centerX;
    const float localY=(mesh.minY+mesh.maxY)*0.5f-model.minY;
    const float localZ=(mesh.minZ+mesh.maxZ)*0.5f-model.centerZ;
    float x=localX*model.scaleX,y=localY*model.scaleY,z=localZ*model.scaleZ;
    float cx=model.qy*z-model.qz*y,cy=model.qz*x-model.qx*z,cz=model.qx*y-model.qy*x;
    float dx=model.qy*cz-model.qz*cy,dy=model.qz*cx-model.qx*cz,dz=model.qx*cy-model.qy*cx;
    x+=2*(model.qw*cx+dx);y+=2*(model.qw*cy+dy);z+=2*(model.qw*cz+dz);
    const float pitchedX=x;
    const float pitchedY=model.cosPitch*y+model.sinPitch*z;
    const float pitchedZ=-model.sinPitch*y+model.cosPitch*z;
    const float halfX=(mesh.maxX-mesh.minX)*0.5f*model.scaleX;
    const float halfY=(mesh.maxY-mesh.minY)*0.5f*model.scaleY;
    const float halfZ=(mesh.maxZ-mesh.minZ)*0.5f*model.scaleZ;
    return {model.x+model.cosYaw*pitchedX+model.sinYaw*pitchedZ,
            model.y+pitchedY,
            model.z-model.sinYaw*pitchedX+model.cosYaw*pitchedZ,
            std::sqrt(halfX*halfX+halfY*halfY+halfZ*halfZ)+(mesh.grassFoliage?5.0f:2.0f)};
}
void sortInstancesForRendering(std::vector<ModelInstance>& instances,
                               float eyeX,float eyeY,float eyeZ){
    auto distanceSquared=[&](const ModelInstance& model){
        const auto sphere=instanceBounds(model);
        const float dx=sphere.x-eyeX,dy=sphere.y-eyeY,dz=sphere.z-eyeZ;
        return dx*dx+dy*dy+dz*dz;
    };
    std::stable_sort(instances.begin(),instances.end(),[&](const auto& a,const auto& b){
        if(a.source->transparent!=b.source->transparent)
            return !a.source->transparent;
        if(a.source->transparent)return distanceSquared(a)>distanceSquared(b);
        if(a.material!=b.material)return a.material<b.material;
        return std::less<const Mesh*>{}(a.source,b.source);
    });
}

void loadMeshes(const std::wstring& folder){
    meshes.clear();
    skins.clear();
    Mesh boxBase;
    boxBase.minX=boxBase.minZ=-0.5f;boxBase.maxX=boxBase.maxZ=0.5f;
    boxBase.minY=0;boxBase.maxY=1;
    meshes.emplace("primitive/box",distantBox(boxBase,1,1,1));
    for(int kind=0;kind<masonry::Count;++kind){
        Mesh rubble;rubble.minX=rubble.minY=rubble.minZ=-.5f;
        rubble.maxX=rubble.maxY=rubble.maxZ=.5f;rubble.roughness=.96f;rubble.allowTessellation=false;
        for(const auto& t:masonry::build(kind).triangles){
            auto a=t.b-t.a,b=t.c-t.a;auto n=game::norm(game::Vec3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x});
            for(auto p:{t.a,t.b,t.c})rubble.vertices.push_back({p.x,p.y,p.z,n.x,n.y,n.z,p.x+.5f,p.z+.5f,t.color.r,t.color.g,t.color.b,1});
        }
        meshes.emplace(masonry::name(kind),std::move(rubble));
    }
    auto effectSprite=[&](const char* name,const wchar_t* texture){
        Mesh sprite;
        sprite.minX=sprite.minZ=-0.5f;sprite.maxX=sprite.maxZ=0.5f;
        sprite.minY=0;sprite.maxY=1;
        sprite.textured=true;sprite.transparent=true;sprite.castsShadow=false;
        sprite.textureFile=(std::filesystem::path(folder).parent_path().parent_path()/
            "effects"/texture).wstring();
        for(int plane=0;plane<2;++plane){
            float angle=float(plane)*1.57079633f;
            float x=std::cos(angle)*0.5f,z=std::sin(angle)*0.5f;
            auto v=[&](float px,float py,float pz,float u,float vv){
                return Vertex{px,py,pz,0,0,1,u,vv,1,1,1,1};
            };
            auto a=v(-x,0,-z,0,1),b=v(x,0,z,1,1);
            auto c=v(x,1,z,1,0),d=v(-x,1,-z,0,0);
            sprite.vertices.insert(sprite.vertices.end(),{a,b,c,a,c,d});
        }
        meshes.emplace(name,std::move(sprite));
    };
    effectSprite("effect/flame",L"flame.png");
    effectSprite("effect/smoke",L"smoke.png");
    // Soft radial opacity makes small brush puffs readable over both grass
    // and stone. Keep this cosmetic mesh separate from ordinary smoke.
    Mesh dust;dust.minX=dust.minZ=-.5f;dust.maxX=dust.maxZ=.5f;dust.minY=0;dust.maxY=1;
    dust.transparent=true;dust.castsShadow=false;dust.unlit=true;dust.allowTessellation=false;
    for(int plane=0;plane<2;++plane){float angle=plane*1.57079633f;
        auto v=[&](float radius,float theta,float alpha){float x=std::cos(theta)*radius;
            return Vertex{x*std::cos(angle),.5f+std::sin(theta)*radius,x*std::sin(angle),0,0,1,0,0,1,1,1,alpha};};
        for(int n=0;n<12;++n){float a=n*6.28318531f/12,b=(n+1)*6.28318531f/12;
            auto center=v(0,0,.65f),innerA=v(.22f,a,.38f),innerB=v(.22f,b,.38f),outerA=v(.5f,a,0),outerB=v(.5f,b,0);
            dust.vertices.insert(dust.vertices.end(),{center,innerA,innerB,innerA,outerA,outerB,innerA,outerB,innerB});}}
    meshes.emplace("effect/builder-dust",std::move(dust));
    effectSprite("effect/flash",L"flash.png");
    effectSprite("effect/blood",L"blood_splat.png");
    // An individual alpha sprite per weapon keeps mipmaps and silhouettes from
    // bleeding into neighboring atlas cells. PNG IHDR supplies its aspect ratio.
    const auto icons=std::filesystem::path(folder).parent_path().parent_path()/"icons"/"weapons";
    if(std::filesystem::exists(icons))for(const auto& entry:std::filesystem::directory_iterator(icons)){
        if(entry.path().extension()!=L".png"||entry.path().stem()==L"source-atlas")continue;
        unsigned char header[24]{};std::ifstream png(entry.path(),std::ios::binary);
        if(!png.read(reinterpret_cast<char*>(header),sizeof(header)))continue;
        auto dimension=[&](int offset){return (unsigned(header[offset])<<24)|
            (unsigned(header[offset+1])<<16)|(unsigned(header[offset+2])<<8)|header[offset+3];};
        unsigned width=dimension(16),height=dimension(20);if(!width||!height)continue;
        Mesh sprite;sprite.minX=-0.5f;sprite.maxX=0.5f;
        sprite.maxY=float(height)/width;sprite.textured=true;sprite.transparent=true;
        sprite.unlit=true;sprite.castsShadow=false;sprite.allowTessellation=false;
        sprite.textureFile=entry.path().wstring();
        Vertex a{-0.5f,0,0,0,0,1,0,1,1,1,1,1},b{0.5f,0,0,0,0,1,1,1,1,1,1,1};
        Vertex c{0.5f,sprite.maxY,0,0,0,1,1,0,1,1,1,1},d{-0.5f,sprite.maxY,0,0,0,1,0,0,1,1,1,1};
        sprite.vertices={a,b,c,a,c,d};
        meshes.emplace("icons/"+entry.path().stem().string(),std::move(sprite));
    }
    Mesh shockwave;
    shockwave.minX=shockwave.minZ=-0.5f;
    shockwave.maxX=shockwave.maxZ=0.5f;
    shockwave.minY=0;shockwave.maxY=1;
    shockwave.textured=true;shockwave.transparent=true;shockwave.castsShadow=false;
    shockwave.textureFile=(std::filesystem::path(folder).parent_path().parent_path()/
        "effects"/"shockwave.png").wstring();
    auto ringVertex=[](float x,float z,float u,float v){
        return Vertex{x,0,z,0,1,0,u,v,1,1,1,1};
    };
    auto ra=ringVertex(-0.5f,-0.5f,0,0),rb=ringVertex(0.5f,-0.5f,1,0);
    auto rc=ringVertex(0.5f,0.5f,1,1),rd=ringVertex(-0.5f,0.5f,0,1);
    shockwave.vertices={ra,rb,rc,ra,rc,rd};
    meshes.emplace("effect/shockwave",std::move(shockwave));
    Mesh bloodDecal=meshes.at("effect/shockwave");
    bloodDecal.textureFile=(std::filesystem::path(folder).parent_path().parent_path()/
        "effects"/"blood_splat.png").wstring();
    meshes.emplace("effect/blood-decal",std::move(bloodDecal));
    Mesh markerRing;
    markerRing.minX=markerRing.minZ=-0.5f;
    markerRing.maxX=markerRing.maxZ=0.5f;
    markerRing.minY=0;markerRing.maxY=1;
    markerRing.transparent=true;markerRing.castsShadow=false;
    for(int segment=0;segment<32;++segment){
        float a=float(segment)*6.2831853f/32.0f;
        float b=float(segment+1)*6.2831853f/32.0f;
        auto v=[](float angle,float radius,float alpha){
            return Vertex{std::cos(angle)*radius,0,std::sin(angle)*radius,
                0,1,0,0,0,1,1,1,alpha};
        };
        auto a0=v(a,0.38f,0.72f),a1=v(a,0.5f,0.08f);
        auto b0=v(b,0.38f,0.72f),b1=v(b,0.5f,0.08f);
        markerRing.vertices.insert(markerRing.vertices.end(),{a0,a1,b1,a0,b1,b0});
    }
    meshes.emplace("marker/ring",std::move(markerRing));
    Mesh markerPillar;
    markerPillar.minX=markerPillar.minZ=-0.5f;
    markerPillar.maxX=markerPillar.maxZ=0.5f;
    markerPillar.minY=0;markerPillar.maxY=1;
    markerPillar.transparent=true;markerPillar.castsShadow=false;
    for(int segment=0;segment<24;++segment){
        float a=float(segment)*6.2831853f/24.0f;
        float b=float(segment+1)*6.2831853f/24.0f;
        auto v=[](float angle,float height,float alpha){
            return Vertex{std::cos(angle)*0.46f,height,std::sin(angle)*0.46f,
                std::cos(angle),0,std::sin(angle),0,0,1,1,1,alpha};
        };
        auto a0=v(a,0,0.26f),a1=v(a,1,0.0f);
        auto b0=v(b,0,0.26f),b1=v(b,1,0.0f);
        markerPillar.vertices.insert(markerPillar.vertices.end(),{a0,b0,b1,a0,b1,a1});
    }
    meshes.emplace("marker/pillar",std::move(markerPillar));
    Mesh bullet;
    bullet.minX=-0.5f;bullet.maxX=0.5f;
    bullet.minY=0;bullet.maxY=1;
    bullet.minZ=-0.5f;bullet.maxZ=0.5f;
    bullet.castsShadow=false;
    for(int segment=0;segment<8;++segment){
        float a=float(segment)*0.78539816f,b=float(segment+1)*0.78539816f;
        auto ring=[&](float angle,float depth,float radius,float shade){
            float x=std::cos(angle),y=std::sin(angle);
            return Vertex{x*radius,0.5f+y*radius,depth,x,y,0,0,0,
                0.85f*shade,0.63f*shade,0.31f*shade,1};
        };
        auto a0=ring(a,-0.5f,0.44f,0.72f),b0=ring(b,-0.5f,0.44f,0.72f);
        auto a1=ring(a,0.12f,0.44f,1),b1=ring(b,0.12f,0.44f,1);
        auto a2=ring(a,0.38f,0.37f,1.1f),b2=ring(b,0.38f,0.37f,1.1f);
        auto tip=ring(a,0.5f,0,1.18f);
        bullet.vertices.insert(bullet.vertices.end(),
            {a0,b0,b1,a0,b1,a1,a1,b1,b2,a1,b2,a2,a2,b2,tip});
    }
    meshes.emplace("primitive/bullet",std::move(bullet));
    Mesh ball;
    ball.minX=ball.minY=ball.minZ=-1;
    ball.maxX=ball.maxY=ball.maxZ=1;
    constexpr int rows=6,columns=10;
    for(int row=0;row<rows;++row){
        float lower=-1.5707963f+row*3.1415926f/rows;
        float upper=-1.5707963f+(row+1)*3.1415926f/rows;
        for(int column=0;column<columns;++column){
            float left=column*6.2831853f/columns;
            float right=(column+1)*6.2831853f/columns;
            auto point=[](float elevation,float angle){
                float x=std::cos(elevation)*std::cos(angle);
                float y=std::sin(elevation);
                float z=std::cos(elevation)*std::sin(angle);
                return Vertex{x,y,z,x,y,z,0,0,1,1,1,1};
            };
            auto a=point(lower,left),b=point(lower,right);
            auto c=point(upper,right),d=point(upper,left);
            ball.vertices.insert(ball.vertices.end(),{a,b,c,a,c,d});
        }
    }
    meshes.emplace("primitive/sphere",std::move(ball));
    Mesh cylinder;
    cylinder.minX=cylinder.minZ=-0.5f;
    cylinder.maxX=cylinder.maxZ=0.5f;
    cylinder.minY=0;cylinder.maxY=1;
    constexpr int sides=16;
    for(int side=0;side<sides;++side){
        float a=side*6.2831853f/sides,b=(side+1)*6.2831853f/sides;
        float ax=std::cos(a)*0.5f,az=std::sin(a)*0.5f;
        float bx=std::cos(b)*0.5f,bz=std::sin(b)*0.5f;
        auto vertex=[](float x,float y,float z,float nx,float ny,float nz){
            return Vertex{x,y,z,nx,ny,nz,0,0,1,1,1,1};
        };
        auto lowerA=vertex(ax,0,az,ax*2,0,az*2);
        auto lowerB=vertex(bx,0,bz,bx*2,0,bz*2);
        auto upperA=vertex(ax,1,az,ax*2,0,az*2);
        auto upperB=vertex(bx,1,bz,bx*2,0,bz*2);
        cylinder.vertices.insert(cylinder.vertices.end(),
            {lowerA,lowerB,upperB,lowerA,upperB,upperA,
             vertex(0,1,0,0,1,0),vertex(ax,1,az,0,1,0),vertex(bx,1,bz,0,1,0)});
    }
    meshes.emplace("primitive/cylinder",std::move(cylinder));
    const char* fixedNames[]={
        "buildings/building-a","buildings/building-d","buildings/building-g",
        "buildings/building-j","buildings/building-m","buildings/building-skyscraper-c",
        "buildings/building-skyscraper-d",
        "marina/marina-wave","marina/marina-wave-lod",
        "marina/marina-terrace","marina/marina-terrace-lod",
        "marina/marina-courtyard","marina/marina-courtyard-lod",
        "marina/marina-bayfront","marina/marina-bayfront-lod",
        "nature/tree_palmDetailedTall","nature/tree_palmDetailedShort","nature/tree_oak",
        "nature/tree_detailed","nature/plant_bushDetailed","nature/grass_large",
        "vehicles/sedan","vehicles/sports-car","vehicles/motorboat",
        "vehicles/helicopter","vehicles/helicopter-rotor",
        "vehicles/traffic-1","vehicles/traffic-2","vehicles/traffic-3",
        "vehicles/traffic-4","vehicles/traffic-5",
        "weapons/pistol","weapons/ak","weapons/lightning",
        "weapons/c4","weapons/remote-trigger","weapons/grenade","weapons/smoke-grenade",
        "weapons/molotov","weapons/flashbang","weapons/timed-bomb",
        "modern/coastal-office","modern/terrace-apartments",
        "modern/compact-pistol","modern/carbine","modern/street-lamp",
        "modern/twin-lamp","modern/bench","modern/bin","modern/bollard",
        "modern/bike-rack","modern/planter","modern/hydrant"
    };
    std::vector<std::string> names(std::begin(fixedNames),std::end(fixedNames));
    struct Catalog {std::string base;std::array<std::string,4> names;std::array<float,4> minPixels;};
    std::vector<Catalog> catalogs;
    lodChains.clear();
    shadowLods.clear();
    issues.clear();
    for(const char* name:fixedNames){
        std::ifstream file(std::filesystem::path(folder)/(std::string(name)+".lod"));
        if(!file)continue;
        std::string magic;file>>magic;Catalog catalog{};catalog.base=name;
        bool valid=magic=="MCLOD1";
        for(int level=0;level<4;++level){
            auto& key=catalog.names[level];auto& threshold=catalog.minPixels[level];
            if(!(file>>key>>threshold)||key.empty()||key.find("..")!=std::string::npos||
               key.front()=='/'||key.find(':')!=std::string::npos||
               key.find('\\')!=std::string::npos||!std::isfinite(threshold)||threshold<0)
                valid=false;
            if(level&&threshold>=catalog.minPixels[level-1])valid=false;
        }
        std::string trailing;
        if(file>>trailing)valid=false;
        if(catalog.names[0]!=name||catalog.minPixels[3]!=0)valid=false;
        if(valid){
            catalogs.push_back(catalog);
            names.insert(names.end(),catalog.names.begin()+1,catalog.names.end());
        }else issues.push_back("Rejected invalid LOD catalog: "+std::string(name));
    }
    const auto vehicleFolder=std::filesystem::path(folder)/L"vehicles";
    if(std::filesystem::exists(vehicleFolder))
        for(const auto& entry:std::filesystem::directory_iterator(vehicleFolder))
            if(entry.is_regular_file()&&entry.path().extension()==L".m3d"&&
               entry.path().stem().wstring().rfind(L"expansion-",0)==0)
                names.push_back("vehicles/"+entry.path().stem().string());
    const auto animalFolder=std::filesystem::path(folder)/L"animals";
    if(std::filesystem::exists(animalFolder))
        for(const auto& entry:std::filesystem::directory_iterator(animalFolder))
            if(entry.is_regular_file()&&entry.path().extension()==L".m3d")
                names.push_back("animals/"+entry.path().stem().string());
    const auto buildingFolder=std::filesystem::path(folder)/L"buildings";
    const auto birdFolder=std::filesystem::path(folder)/L"birds";
    if(std::filesystem::exists(birdFolder))
        for(const auto& entry:std::filesystem::directory_iterator(birdFolder))
            if(entry.is_regular_file()&&entry.path().extension()==L".m3d")
                names.push_back("birds/"+entry.path().stem().string());
    if(std::filesystem::exists(buildingFolder))
        for(const auto& entry:std::filesystem::directory_iterator(buildingFolder)){
            if(!entry.is_regular_file()||entry.path().extension()!=L".m3d")continue;
            std::string stem=entry.path().stem().string();
            if(stem.rfind("urban-",0)==0)names.push_back("buildings/"+stem);
        }
    const auto natureFolder=std::filesystem::path(folder)/L"nature";
    if(std::filesystem::exists(natureFolder))
        for(const auto& entry:std::filesystem::directory_iterator(natureFolder)){
            if(!entry.is_regular_file()||entry.path().extension()!=L".m3d")continue;
            std::string stem=entry.path().stem().string();
            if(stem.rfind("tree_",0)==0||stem.rfind("bush_",0)==0||
               stem.rfind("cactus_",0)==0||stem.rfind("grass_",0)==0||
               stem.rfind("rock_",0)==0)names.push_back("nature/"+stem);
        }
    const auto builderFolder=std::filesystem::path(folder)/L"builder";
    if(std::filesystem::exists(builderFolder))
        for(const auto& entry:std::filesystem::directory_iterator(builderFolder))
            if(entry.is_regular_file()&&entry.path().extension()==L".m3d")names.push_back("builder/"+entry.path().stem().string());
    for(const std::string& path:names){
        std::wstring wide(path.begin(),path.end());
        std::ifstream file(folder+L"\\"+wide+L".m3d",std::ios::binary);
        if(!file)continue;
        char magic[4]{};uint32_t count=0;
        file.read(magic,4);file.read(reinterpret_cast<char*>(&count),4);
        const bool indexed=std::string(magic,4)=="M3D2";
        if(!indexed&&std::string(magic,4)!="M3D1")continue;
        uint32_t indexCount=0;
        if(indexed)file.read(reinterpret_cast<char*>(&indexCount),4);
        if(count==0||count>3000000||
           (indexed&&(indexCount==0||indexCount>9000000||indexCount%3)))continue;
        Mesh result;result.vertices.resize(count);
        if(path.rfind("builder/",0)==0)result.allowTessellation=false;
        file.read(reinterpret_cast<char*>(result.vertices.data()),count*sizeof(Vertex));
        if(indexed){
            result.indices.resize(indexCount);
            file.read(reinterpret_cast<char*>(result.indices.data()),
                      indexCount*sizeof(uint32_t));
            if(std::any_of(result.indices.begin(),result.indices.end(),
                [count](uint32_t index){return index>=count;}))continue;
        }
        if(!file)continue;
        result.minX=result.minY=result.minZ=std::numeric_limits<float>::max();
        result.maxX=result.maxY=result.maxZ=std::numeric_limits<float>::lowest();
        for(const auto& vertex:result.vertices){
            result.minX=std::min(result.minX,vertex.x);result.maxX=std::max(result.maxX,vertex.x);
            result.minY=std::min(result.minY,vertex.y);result.maxY=std::max(result.maxY,vertex.y);
            result.minZ=std::min(result.minZ,vertex.z);result.maxZ=std::max(result.maxZ,vertex.z);
        }
        for(const wchar_t* extension:{L".png",L".jpg",L".jpeg"}){
            auto texturePath=std::filesystem::path(folder)/wide;
            texturePath.replace_extension(extension);
            if(std::filesystem::exists(texturePath)){
                result.textureFile=texturePath.wstring();
                result.textured=true;
                break;
            }
        }
        if(path.rfind("marina/",0)==0){
            result.textureFile=(std::filesystem::path(folder)/
                L"marina/MarinaFacade_Color.png").wstring();
            result.textured=true;
        }
        if(path.rfind("animals/",0)==0){
            std::string base=path;
            auto action=base.find("-walk-");if(action!=std::string::npos)base.resize(action);
            action=base.find("-dead");if(action!=std::string::npos)base.resize(action);
            result.textureFile=(std::filesystem::path(folder)/(base+".png")).wstring();
            result.textured=std::filesystem::exists(result.textureFile);
        }
        if(path.rfind("birds/",0)==0){
            std::string base=path.substr(0,path.find('-',6));
            result.textureFile=(std::filesystem::path(folder)/(base+".png")).wstring();
            result.textured=std::filesystem::exists(result.textureFile);
        }
        auto materialPath=std::filesystem::path(folder)/wide;
        materialPath.replace_extension(L".pbr");
        std::ifstream materialFile(materialPath);
        std::string materialLine;
        bool invalidMaterial=false;
        while(std::getline(materialFile,materialLine)){
            if(materialLine.empty()||materialLine[0]=='#')continue;
            std::istringstream fields(materialLine);
            MaterialRange range;
            std::string base,normal,orm,occlusion,emissive;
            if(!(fields>>range.start>>range.count>>range.roughness>>range.metallic>>
                 range.emissive>>base>>normal>>orm>>occlusion>>emissive)||
               range.count==0||range.start>uint32_t(indexed?indexCount:count)||
               range.count>uint32_t(indexed?indexCount:count)-range.start){
                invalidMaterial=true;break;
            }
            auto resolve=[&](const std::string& name){
                return name=="-"?std::wstring{}:
                    (materialPath.parent_path()/std::filesystem::path(name)).wstring();
            };
            range.baseFile=resolve(base);range.normalFile=resolve(normal);
            range.ormFile=resolve(orm);range.occlusionFile=resolve(occlusion);
            range.emissiveFile=resolve(emissive);
            std::string alphaMode;
            if(fields>>alphaMode){
                if(alphaMode!="OPAQUE"&&alphaMode!="MASK"){
                    invalidMaterial=true;break;
                }
                range.alphaTest=alphaMode=="MASK";
                if(!(fields>>range.alphaCutoff)||range.alphaCutoff<0||
                   range.alphaCutoff>1){
                    invalidMaterial=true;break;
                }
                std::string extension;
                if(fields>>extension){
                    if(extension!="SURFACE1"||
                       !(fields>>range.clearcoat>>range.clearcoatRoughness>>range.glassIor)||
                       !std::isfinite(range.clearcoat)||!std::isfinite(range.clearcoatRoughness)||
                       !std::isfinite(range.glassIor)||range.clearcoat<0||range.clearcoat>1||
                       range.clearcoatRoughness<0||range.clearcoatRoughness>1||
                       (range.glassIor!=0&&(range.glassIor<1||range.glassIor>2.5f))||
                       (fields>>extension)){
                        invalidMaterial=true;break;
                    }
                }
            }
            result.materialRanges.push_back(std::move(range));
        }
        if(invalidMaterial)continue;
        if(!result.materialRanges.empty()){
            result.roughness=result.materialRanges.front().roughness;
            result.metallic=result.materialRanges.front().metallic;
            if(!result.materialRanges.front().baseFile.empty()){
                result.textureFile=result.materialRanges.front().baseFile;
                result.textured=true;
            }
        }
        result.wrapTextures=path.rfind("modern/",0)==0;
        result.vehicleWear=path.rfind("vehicles/",0)==0;
        result.allowTessellation=!result.wrapTextures&&path.rfind("nature/rock_namaqualand_",0)!=0&&path.rfind("nature/rock_coastal_",0)!=0;
        result.grassFoliage=path.rfind("nature/grass_",0)==0&&
            !result.materialRanges.empty();
        if(result.grassFoliage){
            result.allowTessellation=false;
            result.castsShadow=path.find("-lod")==std::string::npos;
        }
        result.alphaTest=(path.rfind("nature/tree_",0)==0||
                          path.rfind("nature/bush_",0)==0||result.grassFoliage)&&result.textured;
        result.temporalStable=path.rfind("characters/",0)!=0&&
            path.rfind("vehicles/",0)!=0&&path.rfind("animals/",0)!=0&&
            path.rfind("birds/",0)!=0&&path.rfind("weapons/",0)!=0&&
            path.rfind("effect/",0)!=0&&!result.grassFoliage;
        if(path.rfind("vehicles/",0)==0){
            if(path.rfind("vehicles/expansion-",0)==0)result.allowTessellation=false;
            std::ifstream windows(folder+L"\\"+wide+L".glass");
            const std::size_t triangleCount=indexed?result.indices.size()/3:
                result.vertices.size()/3;
            std::vector<bool> isGlass(triangleCount,false);
            unsigned index=0;bool found=false;
            while(windows>>index)if(index<isGlass.size()){isGlass[index]=true;found=true;}
            if(found){
                Mesh glass=result;glass.vertices.clear();glass.indices.clear();
                glass.materialRanges.clear();
                glass.textured=false;glass.textureFile.clear();glass.transparent=true;
                glass.castsShadow=false;
                std::vector<Vertex> body;body.reserve(triangleCount*3);
                for(std::size_t i=0;i<triangleCount*3;++i){
                    auto vertex=result.vertices[indexed?result.indices[i]:i];
                    if(isGlass[i/3]){
                        vertex.r=0.35f;vertex.g=0.53f;vertex.b=0.61f;vertex.a=0.18f;
                        glass.vertices.push_back(vertex);
                    }else body.push_back(vertex);
                }
                result.vertices=std::move(body);
                result.indices.clear();
                meshes.emplace(path+"-glass",std::move(glass));
            }
        }
        if(path.rfind("builder/",0)==0){
            Mesh preview=result;preview.transparent=true;preview.castsShadow=false;preview.unlit=true;
            for(auto& vertex:preview.vertices)vertex.a=.24f;
            meshes.emplace(path+"-preview",std::move(preview));
        }
        meshes.emplace(path,std::move(result));
    }
    for(const char* gun:{"compact-pistol","carbine"}){
        auto found=meshes.find(std::string("modern/")+gun);
        if(found!=meshes.end())found->second.temporalStable=false;
    }
    // Baked LODs use the detailed mesh's own atlas.
    for(const std::string& key:names){
        if(key.size()<4||key.compare(key.size()-4,4,"-lod")!=0)continue;
        auto detail=meshes.find(key.substr(0,key.size()-4));
        auto lod=meshes.find(key);
        if(detail!=meshes.end()&&lod!=meshes.end()){
            lod->second.textureFile=detail->second.textureFile;
            lod->second.textured=detail->second.textured;
            lod->second.alphaTest=detail->second.alphaTest;
            if(key.rfind("nature/tree_",0)==0){
                // Leaf density changes must not resize or move the trunk when
                // model() normalizes the selected mesh to the catalog dimensions.
                lod->second.minX=detail->second.minX;lod->second.maxX=detail->second.maxX;
                lod->second.minY=detail->second.minY;lod->second.maxY=detail->second.maxY;
                lod->second.minZ=detail->second.minZ;lod->second.maxZ=detail->second.maxZ;
            }
        }
    }
    const char* people[]={"beach-man","casual-man","casual-woman","hoodie-man"};
    for(const char* person:people){
        std::string skinName="characters/"+std::string(person);
        std::wstring skinWide(skinName.begin(),skinName.end());
        std::ifstream skinFile(folder+L"\\"+skinWide+L".m3s",std::ios::binary);
        if(skinFile){
            char magic[4]{};uint32_t count=0,joints=0,clipCount=0;
            skinFile.read(magic,4);skinFile.read(reinterpret_cast<char*>(&count),4);
            skinFile.read(reinterpret_cast<char*>(&joints),4);
            skinFile.read(reinterpret_cast<char*>(&clipCount),4);
            if(std::string(magic,4)=="M3S3"&&count>0&&count<300000&&joints>0&&
               joints<=255&&clipCount>0&&clipCount<=12){
                SkinMesh skin;skin.jointCount=joints;skin.vertices.resize(count);
                skinFile.read(reinterpret_cast<char*>(skin.vertices.data()),count*sizeof(SkinVertex));
                skin.bodyPartForJoint.resize(joints);
                skinFile.read(reinterpret_cast<char*>(skin.bodyPartForJoint.data()),joints);
                for(uint32_t i=0;i<clipCount&&skinFile;++i){
                    char label[16]{};uint32_t frames=0;float duration=0;
                    skinFile.read(label,16);skinFile.read(reinterpret_cast<char*>(&frames),4);
                    skinFile.read(reinterpret_cast<char*>(&duration),4);
                    if(frames==0||frames>128||duration<=0)break;
                    SkinClip clip;clip.name=std::string(label,std::find(label,label+16,'\0'));
                    clip.frames=frames;clip.duration=duration;
                    clip.palettes.resize(size_t(frames)*joints);
                    skinFile.read(reinterpret_cast<char*>(clip.palettes.data()),
                        clip.palettes.size()*sizeof(std::array<float,16>));
                    clip.rightHands.resize(frames);
                    skinFile.read(reinterpret_cast<char*>(clip.rightHands.data()),
                        clip.rightHands.size()*sizeof(std::array<float,3>));
                    skin.clips.push_back(std::move(clip));
                }
                if(skinFile&&skin.clips.size()==clipCount)skins.emplace(skinName,std::move(skin));
            }
        }
        for(int variant=-2;variant<8;++variant){
            std::string name="characters/"+std::string(person);
            if(variant==-1)name+="-aim";
            else if(variant>=0)name+=(variant<4?"-walk":"-run")+std::to_string(variant%4);
            else if(variant!=-2)continue;
            std::wstring wide(name.begin(),name.end());
            std::ifstream file(folder+L"\\"+wide+L".m3d",std::ios::binary);
            if(!file)continue;
            char magic[4]{};uint32_t count=0;
            file.read(magic,4);file.read(reinterpret_cast<char*>(&count),4);
            if(magic[0]!='M'||magic[1]!='3'||magic[2]!='D'||magic[3]!='1'||count>3000000)continue;
            Mesh result;result.vertices.resize(count);
            file.read(reinterpret_cast<char*>(result.vertices.data()),count*sizeof(Vertex));
            if(!file)continue;
            result.minX=result.minY=result.minZ=std::numeric_limits<float>::max();
            result.maxX=result.maxY=result.maxZ=std::numeric_limits<float>::lowest();
            for(const auto& vertex:result.vertices){
                result.minX=std::min(result.minX,vertex.x);result.maxX=std::max(result.maxX,vertex.x);
                result.minY=std::min(result.minY,vertex.y);result.maxY=std::max(result.maxY,vertex.y);
                result.minZ=std::min(result.minZ,vertex.z);result.maxZ=std::max(result.maxZ,vertex.z);
            }
            meshes.emplace(name,std::move(result));
        }
    }
    for(const std::string& key:names){
        if(key.rfind("nature/",0)!=0||key.rfind("nature/grass_",0)==0)continue;
        if(key.size()>=4&&key.compare(key.size()-4,4,"-lod")==0)continue;
        auto source=meshes.find(key);
        if(source!=meshes.end()&&meshes.find(key+"-lod")==meshes.end()&&
           key.rfind("nature/rock_",0)!=0&&
           key.rfind("nature/cactus_",0)!=0)
            meshes.emplace(key+"-lod",distantNature(source->second,
                key.rfind("nature/tree_",0)==0));
    }
    const char* buildingNames[]={"building-a","building-d","building-g","building-j",
        "building-m","building-skyscraper-c","building-skyscraper-d"};
    for(const char* name:buildingNames){
        std::string key="buildings/"+std::string(name);
        auto source=meshes.find(key);
        if(source!=meshes.end())meshes.emplace(key+"-lod",distantBox(source->second,1,1,1));
    }
    for(int index=0;index<30;++index){
        std::string number=std::to_string(index);
        std::string key=std::string("buildings/urban-")+(index<10?"0":"")+number;
        auto source=meshes.find(key);
        if(source!=meshes.end()&&meshes.find(key+"-lod")==meshes.end())
            meshes.emplace(key+"-lod",distantBox(source->second,1,1,1));
    }
    // Window panes and thin trim should receive shadows but not cast their
    // own unstable sub-pixel shadows back onto the same facade.
    for(int index=0;index<30;++index){
        std::string number=std::to_string(index);
        std::string base=std::string("buildings/urban-")+(index<10?"0":"")+number;
        for(const char* suffix:{"","-lod"}){
            std::string key=base+suffix;
            auto source=meshes.find(key);
            if(source==meshes.end())continue;
            Mesh proxy=distantBox(source->second,1,1,1);
            proxy.textured=false;
            proxy.textureFile.clear();
            proxy.materialRanges.clear();
            auto inserted=meshes.emplace(key+"-shadow",std::move(proxy));
            source->second.shadowProxy=&inserted.first->second;
        }
    }
    for(int i=0;i<4;++i){
        std::string key="characters/"+std::string(people[i]);
        auto source=meshes.find(key);
        if(source!=meshes.end())meshes.emplace(key+"-lod",distantPerson(source->second,i));
    }
    for(const auto& catalog:catalogs){
        LodChain chain{};chain.minPixels=catalog.minPixels;
        bool valid=true;size_t previousCount=std::numeric_limits<size_t>::max();
        for(int level=0;level<4;++level){
            const Mesh* source=mesh(catalog.names[level]);
            if(!source){valid=false;break;}
            size_t count=source->indices.empty()?source->vertices.size():source->indices.size();
            if(count>=previousCount)valid=false;
            previousCount=count;chain.meshes[level]=source;
            if(level){
                const auto& base=*chain.meshes[0];
                for(float delta:{source->minX-base.minX,source->minY-base.minY,
                    source->minZ-base.minZ,source->maxX-base.maxX,
                    source->maxY-base.maxY,source->maxZ-base.maxZ})
                    if(std::abs(delta)>0.001f){
                        valid=false;issues.push_back("LOD bounds mismatch: "+catalog.base+
                            " level "+std::to_string(level)+" delta "+std::to_string(delta));
                    }
            }
        }
        if(valid)lodChains.emplace(catalog.base,chain);
        else issues.push_back("Rejected LOD chain with missing meshes, incompatible bounds or nondecreasing geometry: "+catalog.base);
    }
    auto elements=[](const Mesh* m){return m->indices.empty()?m->vertices.size():m->indices.size();};
    auto compatible=[](const Mesh* a,const Mesh* b){return
        std::abs(a->minX-b->minX)<.001f&&std::abs(a->maxX-b->maxX)<.001f&&
        std::abs(a->minY-b->minY)<.001f&&std::abs(a->maxY-b->maxY)<.001f&&
        std::abs(a->minZ-b->minZ)<.001f&&std::abs(a->maxZ-b->maxZ)<.001f;};
    for(const auto& entry:meshes){const Mesh* source=&entry.second;
        auto found=meshes.find(entry.first+"-lod");
        if(found==meshes.end()||!source->castsShadow||source->transparent)continue;
        const Mesh* coarse=&found->second;
        if(!coarse->castsShadow||coarse->transparent||!compatible(source,coarse)||elements(coarse)>=elements(source))continue;
        // Generated silhouette proxies are reserved for small shadow footprints.
        shadowLods[source]={{source,coarse,coarse,coarse},{32,0,0,0}};
    }
    for(const auto& entry:lodChains){const auto& chain=entry.second;
        for(unsigned first=0;first<4;++first){const Mesh* source=chain.meshes[first];
            if(!source->castsShadow||source->transparent)continue;
            LodChain shadow{};shadow.meshes.fill(source);
            for(unsigned level=0;level<4;++level){const Mesh* candidate=chain.meshes[std::max(first,level)];
                if(candidate->castsShadow&&!candidate->transparent&&elements(candidate)<=elements(source))shadow.meshes[level]=candidate;
                // Retain more detail than the camera LOD at the same footprint.
                shadow.minPixels[level]=chain.minPixels[level]*.5f;
            }
            shadowLods[source]=shadow;
        }
    }
}
const Mesh* shadowLodMesh(const Mesh* source,float texels){
    if(!source)return nullptr;
    if(source->shadowProxy)return source->shadowProxy;
    auto found=shadowLods.find(source);if(found==shadowLods.end())return source;
    const auto& chain=found->second;
    return chain.meshes[chooseLodLevel(texels,chain.minPixels,false,0)];
}
const Mesh* mesh(const std::string& name){auto it=meshes.find(name);return it==meshes.end()?nullptr:&it->second;}
std::vector<const Mesh*> regionalMeshes(){
    std::vector<const Mesh*> result;
    for(const auto& entry:meshes){
        const std::string& name=entry.first;
        if(name.rfind("nature/",0)==0||name.rfind("buildings/urban-",0)==0||
           name.rfind("marina/",0)==0||name.rfind("modern/",0)==0||
           name.rfind("animals/",0)==0||name.rfind("birds/",0)==0)
            result.push_back(&entry.second);
    }
    return result;
}
const SkinMesh* skinMesh(const std::string& name){auto it=skins.find(name);return it==skins.end()?nullptr:&it->second;}
const LodChain* lodChain(const std::string& name){auto it=lodChains.find(name);return it==lodChains.end()?nullptr:&it->second;}
const std::vector<std::string>& assetIssues(){return issues;}
unsigned chooseLodLevel(float pixels,const std::array<float,4>& minPixels,
                        bool hasPrevious,unsigned previousLevel){
    unsigned level=hasPrevious?std::min(3u,previousLevel):0;
    if(!hasPrevious){while(level<3&&pixels<minPixels[level])++level;return level;}
    while(level<3&&pixels<minPixels[level]*0.88f)++level;
    while(level>0&&pixels>minPixels[level-1]*1.12f)--level;
    return level;
}
void deformSkinCpu(const SkinInstance& instance,std::vector<Vertex>& output){
    using game::Vec3;
    const auto& skin=*instance.source;
    const auto& s=instance.scale;const auto& o=instance.origin;
    const auto& t=instance.transform;const auto& yaw=instance.yaw;
    const auto& deform=instance.deformation;
    int motion=int(deform.motion[0]);float waveSin=deform.motion[1],waveCos=deform.motion[2],rising=deform.motion[3];
    float bodyHeight=deform.parameters[0],centerZ=o[2],minY=o[1];
    float tilt=motion==1?1.05f:motion==2?0.10f:motion==3?0.12f:0.0f;
    float coTilt=std::cos(tilt),siTilt=std::sin(tilt);
    auto applyMotion=[&](Vec3& p,Vec3& n,const float parts[6]){
        if(motion==0)return;
        if(motion>=4&&motion<=7){
            // Articulate the idle skin around hips, knees and shoulders. Keep
            // the original character's clothes, face and skin weights.
            bool bike=motion>=5;
            auto bend=[&](Vec3 point,float pivotY,float pivotZ,float angle){
                float y=point.y-pivotY,z=point.z-pivotZ;
                return Vec3{point.x,pivotY+std::cos(angle)*y+std::sin(angle)*z,
                    pivotZ-std::sin(angle)*y+std::cos(angle)*z};
            };
            float hip=minY+bodyHeight*0.50f;
            float knee=minY+bodyHeight*0.27f;
            float thigh=bike?1.05f:1.45f;
            Vec3 leg=bend(p,hip,centerZ,thigh);
            Vec3 bentKnee=bend({p.x,knee,centerZ},hip,centerZ,thigh);
            float legAngle=thigh;
            if(p.y<knee){leg=bend(leg,bentKnee.y,bentKnee.z,-thigh);legAngle=0;}
            if(bike)leg.x+=(parts[5]-parts[4])*bodyHeight*
                (motion==7?0.30f:motion==6?0.16f:0.055f);
            float legWeight=std::clamp(parts[4]+parts[5],0.0f,1.0f);
            Vec3 original=p;p=p+(leg-p)*legWeight;
            Vec3 legNormal=bend(n,0,0,legAngle);n=n+(legNormal-n)*legWeight;
            float armWeight=std::clamp(parts[2]+parts[3],0.0f,1.0f);
            float shoulder=minY+bodyHeight*0.77f;
            float armAngle=bike?0.95f:1.25f;
            Vec3 arm=bend(original,shoulder,centerZ,armAngle);
            p=p+(arm-original)*armWeight;
            Vec3 armNormal=bend(n,0,0,armAngle);n=n+(armNormal-n)*armWeight;
            if(motion==5){
                p=bend(p,hip,centerZ,-0.22f);
                n=bend(n,0,0,-0.22f);
            }
            return;
        }
        float left=waveSin,right=-left;
        float armWave=parts[2]*left+parts[3]*right;
        float legWave=parts[4]*right+parts[5]*left;
        if(motion==8){
            float arms=parts[2]+parts[3];
            p.y+=bodyHeight*arms*(0.12f+0.06f*waveSin);
            p.z+=bodyHeight*arms*(0.12f+0.04f*waveCos);
            p.x+=bodyHeight*(parts[2]-parts[3])*0.035f*waveCos;
            return;
        }
        if(motion==1){
            p.x+=bodyHeight*0.17f*(parts[3]*(0.5f-0.5f*waveSin)-
                parts[2]*(0.5f+0.5f*waveSin));
            p.z+=bodyHeight*(0.18f*armWave+0.10f*legWave);
            p.y+=bodyHeight*(0.08f*(parts[2]+parts[3])*waveCos+
                0.07f*legWave);
        }else if(motion==2){
            float armWeight=parts[2]+parts[3];
            float shoulderY=minY+bodyHeight*0.77f;
            p.y+=armWeight*std::max(0.0f,shoulderY-p.y)*1.85f+
                bodyHeight*(0.07f*armWave+0.13f*legWave);
            p.z+=bodyHeight*(0.09f*armWeight-0.05f*legWave);
        }else if(motion==3){
            p.y+=bodyHeight*(0.14f*(parts[2]+parts[3])+
                0.10f*rising*(parts[4]+parts[5]));
            p.z+=bodyHeight*(0.09f*rising*(parts[4]+parts[5])-0.04f*armWave);
        }
        float pivot=minY+bodyHeight*0.45f;
        float vertical=p.y-pivot;
        float depth=p.z-centerZ;
        p.y=pivot+coTilt*vertical-siTilt*depth;
        p.z=centerZ+siTilt*vertical+coTilt*depth;
        float ny=n.y,nz=n.z;
        n.y=coTilt*ny-siTilt*nz;
        n.z=siTilt*ny+coTilt*nz;
    };

    auto rotate=[](const std::array<float,4>& q,Vec3 v){
        Vec3 cross{q[1]*v.z-q[2]*v.y,q[2]*v.x-q[0]*v.z,q[0]*v.y-q[1]*v.x};
        Vec3 nested{q[1]*cross.z-q[2]*cross.y,q[2]*cross.x-q[0]*cross.z,q[0]*cross.y-q[1]*cross.x};
        return v+(cross*q[3]+nested)*2.0f;
    };
    output.reserve(output.size()+skin.vertices.size());
    for(const auto& input:skin.vertices){
        Vec3 p{},n{};float parts[6]{};const auto& v=input.base;
        for(int influence=0;influence<4;++influence){
            float weight=input.weights[influence];unsigned joint=input.joints[influence];
            if(weight<=0||joint>=instance.palette.size())continue;
            unsigned part=joint<skin.bodyPartForJoint.size()?skin.bodyPartForJoint[joint]:0;
            if(part>=6)part=0;parts[part]+=weight;
            const auto& m=instance.palette[joint];
            Vec3 point{m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12],m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13],m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]};
            Vec3 normal{m[0]*v.nx+m[4]*v.ny+m[8]*v.nz,m[1]*v.nx+m[5]*v.ny+m[9]*v.nz,m[2]*v.nx+m[6]*v.ny+m[10]*v.nz};
            if(deform.parameters[3]!=0){
                float x=(point.x-o[0])*s[0],z=(point.z-o[2])*s[2];
                Vec3 rest{t[0]+yaw[0]*x+yaw[1]*z,t[1]+(point.y-o[1])*s[1],t[2]-yaw[1]*x+yaw[0]*z};
                const auto& r=deform.bodyRest[part];const auto& b=deform.bodyPosition[part];
                point=Vec3{b[0],b[1],b[2]}+rotate(deform.bodyRotation[part],rest-Vec3{r[0],r[1],r[2]});
                normal={normal.x/s[0],normal.y/s[1],normal.z/s[2]};
                normal=rotate(deform.bodyRotation[part],{yaw[0]*normal.x+yaw[1]*normal.z,normal.y,-yaw[1]*normal.x+yaw[0]*normal.z});
            }
            p=p+point*weight;n=n+normal*weight;
        }
        Vec3 world,normal;
        if(deform.parameters[3]!=0){world=p;normal=game::norm(n);}
        else{
            applyMotion(p,n,parts);
            float x=(p.x-o[0])*s[0],z=(p.z-o[2])*s[2];
            float nx=n.x/s[0],ny=n.y/s[1],nz=n.z/s[2];
            float length=std::max(0.0001f,std::sqrt(nx*nx+ny*ny+nz*nz));
            world={t[0]+yaw[0]*x+yaw[1]*z,t[1]+(p.y-o[1])*s[1],t[2]-yaw[1]*x+yaw[0]*z};
            normal={(yaw[0]*nx+yaw[1]*nz)/length,ny/length,(-yaw[1]*nx+yaw[0]*nz)/length};
            float aimPitch=deform.parameters[1],weight=parts[2]+parts[3];
            if(weight>0&&std::abs(aimPitch)>=0.001f){
                float pitch=aimPitch*std::clamp(weight,0.0f,1.0f);
                Vec3 forward{yaw[1],0,yaw[0]},pivot{t[0]+forward.x*3,t[1]+deform.parameters[2]*0.77f,t[2]+forward.z*3};
                float depth=(world.x-pivot.x)*forward.x+(world.z-pivot.z)*forward.z,vertical=world.y-pivot.y;
                float rotated=std::cos(pitch)*depth-std::sin(pitch)*vertical;
                world.x+=forward.x*(rotated-depth);world.z+=forward.z*(rotated-depth);
                world.y=pivot.y+std::sin(pitch)*depth+std::cos(pitch)*vertical;
            }
        }
        const auto& anchor=deform.attachmentOrigin;
        Vec3 pivot{anchor[0],anchor[1],anchor[2]};
        world=pivot+rotate(deform.attachmentRotation,world-pivot);
        normal=rotate(deform.attachmentRotation,normal);
        output.push_back({world.x,world.y,world.z,normal.x,normal.y,normal.z,v.u,v.v,v.r,v.g,v.b,v.a});
    }
}
}
