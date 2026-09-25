#include "dx11_assets.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace dx11 {
namespace {
std::unordered_map<std::string,Mesh> meshes;
std::unordered_map<std::string,SkinMesh> skins;
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
void loadMeshes(const std::wstring& folder){
    meshes.clear();
    skins.clear();
    Mesh boxBase;
    boxBase.minX=boxBase.minZ=-0.5f;boxBase.maxX=boxBase.maxZ=0.5f;
    boxBase.minY=0;boxBase.maxY=1;
    meshes.emplace("primitive/box",distantBox(boxBase,1,1,1));
    Mesh grassTuft;
    grassTuft.minX=grassTuft.minZ=-0.5f;
    grassTuft.maxX=grassTuft.maxZ=0.5f;
    grassTuft.minY=0;grassTuft.maxY=1;
    grassTuft.castsShadow=false;
    for(int blade=0;blade<3;++blade){
        float angle=float(blade)*2.0943951f;
        float forwardX=std::cos(angle),forwardZ=std::sin(angle);
        float sideX=-forwardZ,sideZ=forwardX;
        float rootX=forwardX*0.07f,rootZ=forwardZ*0.07f;
        float tipX=forwardX*(0.28f+0.06f*blade);
        float tipZ=forwardZ*(0.28f+0.06f*blade);
        float height=0.76f+0.12f*blade;
        grassTuft.vertices.push_back({rootX-sideX*0.16f,0,rootZ-sideZ*0.16f,
            forwardX,0.4f,forwardZ,0,1,0.72f,0.82f,0.66f,1});
        grassTuft.vertices.push_back({rootX+sideX*0.16f,0,rootZ+sideZ*0.16f,
            forwardX,0.4f,forwardZ,1,1,0.72f,0.82f,0.66f,1});
        grassTuft.vertices.push_back({tipX,height,tipZ,
            forwardX,0.4f,forwardZ,0.5f,0,0.96f,1.0f,0.86f,1});
    }
    meshes.emplace("primitive/grass-tuft",std::move(grassTuft));
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
    effectSprite("effect/flash",L"flash.png");
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
        "vehicles/sedan","vehicles/sports-car","vehicles/motorboat"
    };
    std::vector<std::string> names(std::begin(fixedNames),std::end(fixedNames));
    const auto buildingFolder=std::filesystem::path(folder)/L"buildings";
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
               stem.rfind("cactus_",0)==0||
               stem.rfind("rock_",0)==0)names.push_back("nature/"+stem);
        }
    for(const std::string& path:names){
        std::wstring wide(path.begin(),path.end());
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
        auto materialPath=std::filesystem::path(folder)/wide;
        materialPath.replace_extension(L".pbr");
        std::ifstream materialFile(materialPath);
        std::string materialLine;
        while(std::getline(materialFile,materialLine)){
            if(materialLine.empty()||materialLine[0]=='#')continue;
            std::istringstream fields(materialLine);
            MaterialRange range;
            std::string base,normal,orm,occlusion,emissive;
            if(!(fields>>range.start>>range.count>>range.roughness>>range.metallic>>
                 range.emissive>>base>>normal>>orm>>occlusion>>emissive)||
               range.count==0||range.start+range.count>count){
                result.materialRanges.clear();break;
            }
            auto resolve=[&](const std::string& name){
                return name=="-"?std::wstring{}:
                    (materialPath.parent_path()/std::filesystem::path(name)).wstring();
            };
            range.baseFile=resolve(base);range.normalFile=resolve(normal);
            range.ormFile=resolve(orm);range.occlusionFile=resolve(occlusion);
            range.emissiveFile=resolve(emissive);
            result.materialRanges.push_back(std::move(range));
        }
        if(!result.materialRanges.empty()){
            result.roughness=result.materialRanges.front().roughness;
            result.metallic=result.materialRanges.front().metallic;
            if(!result.materialRanges.front().baseFile.empty()){
                result.textureFile=result.materialRanges.front().baseFile;
                result.textured=true;
            }
        }
        result.alphaTest=(path.rfind("nature/tree_",0)==0||
                          path.rfind("nature/bush_",0)==0)&&result.textured;
        meshes.emplace(path,std::move(result));
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
        if(key.rfind("nature/",0)!=0)continue;
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
    for(int i=0;i<4;++i){
        std::string key="characters/"+std::string(people[i]);
        auto source=meshes.find(key);
        if(source!=meshes.end())meshes.emplace(key+"-lod",distantPerson(source->second,i));
    }
}
const Mesh* mesh(const std::string& name){auto it=meshes.find(name);return it==meshes.end()?nullptr:&it->second;}
const SkinMesh* skinMesh(const std::string& name){auto it=skins.find(name);return it==skins.end()?nullptr:&it->second;}
}
