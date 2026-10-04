#include "../src/dx11_assets.h"
#include "../src/dx11_grass.h"
#include "../src/game_internal.h"
#include "../src/regions.h"
#include "../src/ui.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <cstdio>
namespace {
using Root=std::pair<float,float>;
std::set<Root> capture(game::Vec2 point,int setting,int density,unsigned* detailed=nullptr){
    if(detailed)*detailed=0;
    ui::grassDistance=setting;ui::vegetationDensity=density;
    game::player=game::previousPlayer=point;
    std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];
    std::vector<dx11::ModelInstance> instances;
    dx11::buildScene(groups,instances,point.x,40,point.z-30);
    std::set<Root> roots;
    for(const auto& instance:instances){
        if(!instance.source->grassFoliage)continue;
        assert(dx11::grass::surfaceAt({instance.x,instance.z})!=dx11::grass::Surface::None);
        assert(game::len(game::Vec2{instance.x,instance.z}-point)<ui::grassDistanceUnits());
        assert(instance.scaleX>0&&instance.scaleY>0&&instance.scaleZ>0);
        bool isDetailed=instance.source->castsShadow;
        assert(isDetailed==(game::len(game::Vec2{instance.x,instance.z}-point)<=ui::grassLodDistanceUnits()));
        if(detailed&&isDetailed)++*detailed;
        roots.insert({instance.x,instance.z});
    }
    return roots;
}
}
void grassScenarios(){
    using namespace dx11::grass;
    dx11::loadMeshes(L"assets/models/baked");
    ui::grassLodDistance=50;
    for(const char* type:{"lawn","meadow","savanna","desert","snow","coastal"}){
        std::string name="nature/grass_"+std::string(type);
        const auto* detail=dx11::mesh(name);
        assert(!dx11::mesh(name+"-lod")); // Cutouts must keep their authored LODs.
        assert(detail&&detail->grassFoliage&&detail->textured&&detail->alphaTest&&detail->castsShadow);
        assert(!detail->allowTessellation&&!detail->temporalStable);
        assert(!detail->indices.empty()&&detail->materialRanges.size()==1);
        auto& material=detail->materialRanges.front();
        assert(material.alphaTest&&material.alphaCutoff>.2f&&material.alphaCutoff<.8f);
        assert(std::filesystem::exists(material.baseFile)&&std::filesystem::exists(material.normalFile)&&
            std::filesystem::exists(material.ormFile));
        std::ifstream png(material.baseFile,std::ios::binary);unsigned char header[24]{};png.read(reinterpret_cast<char*>(header),24);
        auto dimension=[&](int start){return unsigned(header[start])*16777216u+
            unsigned(header[start+1])*65536u+unsigned(header[start+2])*256u+header[start+3];};
        assert(png&&dimension(16)==2048&&dimension(20)==2048);
        bool rooted=false,flexible=false;
        for(const auto& v:detail->vertices){rooted|=v.a<.001f;flexible|=v.a>.9f;}
        assert(rooted&&flexible);
        for(int level=1;level<=2;++level){
            const auto* lod=dx11::mesh(name+"-lod"+std::to_string(level));
            assert(lod&&lod->grassFoliage&&!lod->castsShadow&&!lod->allowTessellation);
            assert(lod->indices.size()<detail->indices.size()/4);
            assert(lod->materialRanges[0].baseFile==material.baseFile);
        }
    }
    auto buildings=game::buildings;game::buildings.clear();
    assert(surfaceAt({150,1320})==Surface::Lawn);
    assert(surfaceAt({4550,4500})==Surface::Meadow);
    assert(surfaceAt({12550,13350})==Surface::Savanna);
    assert(surfaceAt({12250,3450})==Surface::Desert);
    assert(surfaceAt({4550,14450})==Surface::Snow);
    assert(surfaceAt({1750,1740})==Surface::Coastal);
    for(game::Vec2 point:{game::Vec2{300,100},{160,250},{7700,4000},{160,1950},
                         {1200,1700},{8500,10220},{8090,9200},{1,400}})
        assert(surfaceAt(point)==Surface::None);
    game::buildings.push_back({4510,4460,100,100,90,game::rgb(255,255,255),"grass-obstacle"});
    assert(surfaceAt({4550,4500})==Surface::None);
    game::buildings=std::move(buildings);
    ui::grassDistance=0;assert(ui::grassDistanceUnits()==0);
    ui::grassDistance=50;assert(ui::grassDistanceUnits()==230);
    ui::grassDistance=100;assert(ui::grassDistanceUnits()==800);
    assert(ringFade(0,0,230)==1&&ringFade(231,1,230)==0);
    assert(ringFade(145,0,800)>0&&ringFade(145,1,800)>0);
    assert(ringFade(380,1,800)>0&&ringFade(380,2,800)>0);
    auto nearbyRoots=capture({4550,4500},50,2),repeat=capture({4550,4500},50,2);
    assert(nearbyRoots.size()>500&&nearbyRoots==repeat);
    auto medium=capture({4550,4500},50,1);
    assert(medium.size()>100&&medium.size()<nearbyRoots.size());
    for(const auto& root:medium)assert(nearbyRoots.count(root));
    auto distantRoots=capture({4550,4500},100,2);
    assert(distantRoots.size()>nearbyRoots.size()&&distantRoots.size()<20000);
    for(const auto& root:nearbyRoots)assert(distantRoots.count(root));
    unsigned lowDetails=0,highDetails=0;
    ui::grassLodDistance=0;assert(ui::grassLodDistanceUnits()==40);
    auto lowLodRoots=capture({4550,4500},100,2,&lowDetails);
    ui::grassLodDistance=100;assert(ui::grassLodDistanceUnits()==800);
    auto highLodRoots=capture({4550,4500},100,2,&highDetails);
    assert(lowLodRoots==highLodRoots&&highLodRoots==distantRoots);
    assert(highDetails==highLodRoots.size()&&lowDetails>0&&highDetails>lowDetails*4);
    ui::grassLodDistance=50;assert(ui::grassLodDistanceUnits()==95);
    assert(capture({4550,4500},0,2).empty());
    assert(capture({4550,4500},100,0).empty());
    for(game::Vec2 point:{game::Vec2{150,1320},{12550,13350},{12250,3450},
                         {4550,14450},{1750,1740}})
        assert(!capture(point,50,2).empty());
    std::printf("Grass checks passed: 2K masked PBR, rooted wind, surface exclusions, deterministic density/LODs; default %zu, max %zu roots\n",nearbyRoots.size(),distantRoots.size());
}
