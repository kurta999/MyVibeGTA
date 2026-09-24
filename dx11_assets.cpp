#include "dx11_assets.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>

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
}
void loadMeshes(const std::wstring& folder){
    meshes.clear();
    skins.clear();
    const char* names[]={
        "buildings/building-a","buildings/building-d","buildings/building-g",
        "buildings/building-j","buildings/building-m","buildings/building-skyscraper-c",
        "buildings/building-skyscraper-d",
        "nature/tree_palmDetailedTall","nature/tree_palmDetailedShort","nature/tree_oak",
        "nature/tree_detailed","nature/plant_bushDetailed","nature/grass_large",
        "vehicles/sedan","vehicles/sports-car","vehicles/motorboat"
    };
    for(const char* name:names){
        std::string path(name);
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
        result.textured=path.rfind("buildings/",0)==0;
        meshes.emplace(path,std::move(result));
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
            if(std::string(magic,4)=="M3S2"&&count>0&&count<300000&&joints>0&&
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
    const char* nature[]={"tree_palmDetailedTall","tree_palmDetailedShort","tree_oak",
        "tree_detailed","plant_bushDetailed","grass_large"};
    for(const char* name:nature){
        std::string key="nature/"+std::string(name);
        auto source=meshes.find(key);
        if(source!=meshes.end())meshes.emplace(key+"-lod",distantNature(source->second,
            std::string(name).rfind("tree_",0)==0));
    }
}
const Mesh* mesh(const std::string& name){auto it=meshes.find(name);return it==meshes.end()?nullptr:&it->second;}
const SkinMesh* skinMesh(const std::string& name){auto it=skins.find(name);return it==skins.end()?nullptr:&it->second;}
}
