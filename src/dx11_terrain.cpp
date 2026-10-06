#include "dx11_terrain.h"
#include "terrain.h"
#include "regions.h"
#include "excavation.h"
#include "scenery_edits.h"
#include "surface_work.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

namespace dx11 {
namespace {
constexpr unsigned columns=21;
constexpr float chunkWidth=800;
struct Chunk {std::array<Mesh,5> meshes;std::uint64_t revision=0,editRevision=0;};
// Stable addresses retain the renderer's immutable GPU buffers across travel.
std::array<Chunk,columns*columns> chunks;
void prepare(Chunk& chunk,unsigned cx,unsigned cz){
    if(chunk.revision==terrain::revision()&&chunk.editRevision==excavation::revision())return;
    for(auto& source:chunk.meshes){source.vertices.clear();++source.revision;
        source.minX=cx*chunkWidth;source.maxX=(cx+1)*chunkWidth;
        source.minZ=cz*chunkWidth;source.maxZ=(cz+1)*chunkWidth;
        source.minY=1000;source.maxY=-1000;source.allowTessellation=false;source.wrapTextures=true;}
    for(unsigned z=0;z<16;++z)for(unsigned x=0;x<16;++x){
        float left=cx*chunkWidth+x*terrain::spacing,top=cz*chunkWidth+z*terrain::spacing;
        if(left<game::WORLD_W&&top<game::WORLD_D)continue;
        game::Vec2 center{left+25,top+25};
        if(regions::waterAt(center))continue;
        auto biome=regions::biomeAt(center);auto n=terrain::normal(center);
        float elevation=terrain::baseHeight(center);
        int group=regions::roadAt(center)?4:biome==regions::Biome::Snow?3:
            (n.y<.93f||elevation>150)?2:biome==regions::Biome::Desert?1:0;
        auto& source=chunk.meshes[group];
        auto tint=group==2?game::rgb(203,194,173):regions::groundColor(center);
        if(group==4)tint=game::rgb(112,114,116);
        if(group==2&&biome==regions::Biome::Desert)tint=game::rgb(233,192,145);
        auto vertexAt=[&](float px,float pz){
            float y=terrain::baseHeight({px,pz})+(group==4?.11f:0);
            auto normal=terrain::normal({px,pz});
            source.minY=std::min(source.minY,y);source.maxY=std::max(source.maxY,y);
            return Vertex{px,y,pz,normal.x,normal.y,normal.z,px/70,pz/70,tint.r,tint.g,tint.b,1};
        };
        auto a=vertexAt(left,top),b=vertexAt(left+50,top),c=vertexAt(left+50,top+50),d=vertexAt(left,top+50);
        excavation::clipTriangle({a,d,c},source.vertices);excavation::clipTriangle({a,c,b},source.vertices);
    }
    const auto* rock=mesh("nature/rock_namaqualand_boulder_02");
    if(rock){
        auto folder=std::filesystem::path(rock->textureFile).parent_path()/L"../../../materials/terrain";
        auto& rocky=chunk.meshes[2];rocky.materialRanges.clear();
        if(!rocky.vertices.empty()){
            MaterialRange range{};range.count=unsigned(rocky.vertices.size());range.roughness=.93f;
            range.baseFile=(folder/L"rocky_terrain_diff_1k.jpg").wstring();
            range.normalFile=(folder/L"rocky_terrain_nor_dx_1k.jpg").wstring();
            rocky.materialRanges.push_back(range);rocky.textured=true;
        }
    }
    chunk.revision=terrain::revision();chunk.editRevision=excavation::revision();
}
struct EditedChunk {std::map<int,Mesh> materials;std::uint64_t revision=0,baseRevision=0;};
std::map<excavation::Patch,EditedChunk> edited;
void appendExcavations(std::vector<ModelInstance>& instances,float radius){
    if(!builder::active())return;
    for(const auto& patch:excavation::patches()){
        game::Vec2 center{(patch.first.x+.5f)*200,(patch.first.z+.5f)*200};
        if(game::len(center-game::player)>radius+150)continue;
        auto& chunk=edited[patch.first];
        if(chunk.revision!=patch.second||chunk.baseRevision!=terrain::revision()){
            for(auto& group:chunk.materials){group.second.vertices.clear();++group.second.revision;}
            for(const auto& face:excavation::faces(patch.first))if(face.item>=0){
                auto& source=chunk.materials[face.item];auto* material=mesh("builder/"+builder::items()[face.item].id);
                source.textured=true;source.allowTessellation=false;source.wrapTextures=true;source.roughness=.95f;
                if(material)source.textureFile=material->textureFile;
                source.minX=patch.first.x*200.0f;source.maxX=source.minX+200;source.minZ=patch.first.z*200.0f;source.maxZ=source.minZ+200;
                source.minY=-400;source.maxY=3000;
                source.vertices.insert(source.vertices.end(),face.vertices.begin(),face.vertices.end());
            }chunk.revision=patch.second;chunk.baseRevision=terrain::revision();
        }
        for(const auto& group:chunk.materials)if(!group.second.vertices.empty())instances.push_back({&group.second,2,1,1,1,1,0,0,0,0,0,0,0,1,1,1});
    }
}
}
void appendRegionalTerrain(std::vector<ModelInstance>& instances,float radius){
    surface_work::append(instances,radius);
    appendExcavations(instances,radius);
    // Mountain silhouettes remain visible with a short local draw radius.
    radius=std::max(radius,3800.0f);
    const int materials[]={9,8,0,0,7};
    for(unsigned z=0;z<columns;++z)for(unsigned x=0;x<columns;++x){
        game::Vec2 center{(x+.5f)*chunkWidth,(z+.5f)*chunkWidth};
        if(game::len(center-game::player)>radius+570)continue;
        auto& chunk=chunks[z*columns+x];prepare(chunk,x,z);
        for(unsigned group=0;group<5;++group){
            const auto& source=chunk.meshes[group];if(source.vertices.empty())continue;
            instances.push_back({&source,materials[group],1,1,1,1,0,0,0,0,0,0,0,1,1,1});
        }
    }
    // Sparse scanned outcrops decorate ridges; traversal uses the ground mesh.
    for(int z=0;z<42;++z)for(int x=0;x<42;++x){
        unsigned hash=unsigned(x)*73856093u^unsigned(z)*19349663u;
        if(hash%3)continue;
        game::Vec2 p{(x+.2f+float(hash%50)/100)*400,(z+.2f+float((hash>>8)%50)/100)*400};
        if(game::len(p-game::player)>std::min(radius,1900.0f)||regions::waterAt(p)||regions::roadAt(p))continue;
        float h=terrain::baseHeight(p);if(h<90||terrain::normal(p).y>.995f)continue;
        if(scenery_edits::appendIfEdited("outcrop:"+std::to_string(x)+":"+std::to_string(z),instances))continue;
        bool cliff=hash%4==0;
        std::string name=cliff?"nature/rock_coastal_cliff_02":"nature/rock_namaqualand_boulder_02";
        if(game::len(p-game::player)>800)name+="-lod";
        const auto* source=mesh(name);if(!source)continue;
        float width=cliff?100+float(hash%60):22+float(hash%30),height=cliff?35:width*.65f;
        float yaw=float(hash%628)/100;
        auto tint=regions::biomeAt(p)==regions::Biome::Desert?game::rgb(234,202,166):game::rgb(226,224,207);
        instances.push_back({source,0,width/(source->maxX-source->minX),height/(source->maxY-source->minY),
            width/(source->maxZ-source->minZ),std::cos(yaw),std::sin(yaw),p.x,h-height*.22f,p.z,
            (source->minX+source->maxX)*.5f,source->minY,(source->minZ+source->maxZ)*.5f,tint.r,tint.g,tint.b});
    }
}
}
