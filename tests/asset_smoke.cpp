#include "../src/dx11_assets.h"
#include "../src/data_file.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <set>
#include <sstream>

int main(){
    static_assert(sizeof(dx11::SkinVertex)==68);
    dx11::loadMeshes(L"assets/models/baked");
    for(const char* name:{"traffic-1","traffic-2","traffic-3","traffic-4","traffic-5","sports-car","sedan"}){
        std::string key=std::string("vehicles/")+name;
        const auto* body=dx11::mesh(key);const auto* glass=dx11::mesh(key+"-glass");
        assert(body&&glass&&glass->transparent&&!glass->castsShadow&&!glass->textured);
        assert(glass->vertices.size()>=30&&body->vertices.size()>glass->vertices.size());
        assert(body->minY==glass->minY&&body->maxY==glass->maxY&&body->minZ==glass->minZ);
        for(const auto& v:glass->vertices)assert(v.a>0&&v.a<0.3f);
    }
    for(const char* name:{"seagull","crow","sparrow","parrot","dove"}){
        std::wstring texture;
        for(int pose=0;pose<9;++pose){
            const auto* bird=dx11::mesh(std::string("birds/")+name+
                (pose==8?"-dead":"-flap-"+std::to_string(pose)));
            assert(bird&&bird->vertices.size()>300&&bird->textured);
            assert(std::filesystem::exists(bird->textureFile));
            if(pose<8)for(const auto& v:bird->vertices){
                assert(std::abs(v.x)<=0.52f&&std::abs(v.y)*0.7f<=0.40f&&std::abs(v.z)<=0.60f);
            }
            if(pose==0)texture=bird->textureFile;else assert(texture==bird->textureFile);
        }
    }
    for(const char* species:{"tiger","elephant","cat","dog","pig","cow","capybara",
        "bear","goat","donkey","roe-deer","deer","weasel","beaver","mouse"}){
        const std::string key=std::string("animals/")+species;
        const auto* base=dx11::mesh(key);
        assert(base&&base->vertices.size()>300&&base->textured);
        assert(std::filesystem::exists(base->textureFile));
        for(const std::string& suffix:{"-dead","-walk-0","-walk-1","-walk-2",
            "-walk-3","-walk-4","-walk-5","-walk-6","-walk-7"}){
            const auto* pose=dx11::mesh(key+suffix);
            assert(pose&&pose->vertices.size()==base->vertices.size()&&pose->textured);
            assert(pose->textureFile==base->textureFile);
            bool changed=false;
            for(std::size_t i=0;i<pose->vertices.size();++i){
                const auto& a=pose->vertices[i];const auto& b=base->vertices[i];
                if(a.x!=b.x||a.y!=b.y||a.z!=b.z)changed=true;
            }
            assert(changed);
        }
    }
    const auto* grass=dx11::mesh("primitive/grass-tuft");
    const auto* bullet=dx11::mesh("primitive/bullet");
    assert(grass&&grass->vertices.size()==9&&!grass->castsShadow);
    assert(bullet&&bullet->vertices.size()>=100&&!bullet->castsShadow);
    for(const char* name:{"effect/flame","effect/smoke","effect/flash",
                          "effect/shockwave","effect/blood","effect/blood-decal"}){
        const auto* effect=dx11::mesh(name);
        assert(effect&&effect->transparent&&!effect->castsShadow&&
            effect->textured&&std::filesystem::exists(effect->textureFile));
    }
    for(const char* name:{"characters/beach-man","characters/casual-man",
                          "characters/casual-woman","characters/hoodie-man"}){
        const auto* skin=dx11::skinMesh(name);
        assert(skin&&skin->jointCount>0&&skin->vertices.size()>1000);
        assert(skin->bodyPartForJoint.size()==skin->jointCount);
        assert(skin->clips.size()==12);
        assert(skin->clips[8].name=="Fall");
        assert(skin->clips[9].name=="Enter");
        assert(skin->clips[10].name=="Punch");
        assert(skin->clips[11].name=="Slash");
        assert(skin->clips[4].palettes!=skin->clips[0].palettes);
        assert(skin->clips[10].palettes!=skin->clips[0].palettes);
        for(const auto& clip:skin->clips)
            assert(clip.palettes.size()==size_t(clip.frames)*skin->jointCount);
        for(const auto& clip:skin->clips)
            assert(clip.rightHands.size()==clip.frames);
        assert(dx11::mesh((std::string(name)+"-lod").c_str()));
    }
    assert(dx11::mesh("buildings/building-a-lod"));
    for(const char* name:{"building-a","building-d","building-g","building-j",
                          "building-m","building-skyscraper-c","building-skyscraper-d"}){
        const std::string key=std::string("buildings/")+name;
        const auto* building=dx11::mesh(key);
        assert(building&&building->textured&&std::filesystem::exists(building->textureFile));
        assert(std::filesystem::path(building->textureFile).stem().string()==name);
        assert(dx11::mesh(key+"-lod")->textureFile==building->textureFile);
    }
    std::set<std::wstring> buildingTextures;
    for(int index=0;index<30;++index){
        std::string number=(index<10?"0":"")+std::to_string(index);
        std::string key="buildings/urban-"+number;
        const auto* building=dx11::mesh(key);
        const auto* lod=dx11::mesh(key+"-lod");
        assert(building&&lod&&building->vertices.size()>1000&&lod->vertices.size()>100);
        assert(building->shadowProxy&&lod->shadowProxy&&
            building->shadowProxy->vertices.size()==30&&
            lod->shadowProxy->vertices.size()==30&&
            !building->shadowProxy->textured);
        assert(building->textured&&std::filesystem::exists(building->textureFile));
        assert(lod->textured&&lod->textureFile==building->textureFile);
        assert(buildingTextures.insert(building->textureFile).second);
    }
    std::ifstream cityManifest("assets/models/CITY_MANIFEST.csv");
    assert(cityManifest);
    std::string cityRow;std::getline(cityManifest,cityRow);
    int cityCount=0;
    while(std::getline(cityManifest,cityRow))if(!cityRow.empty())++cityCount;
    assert(cityCount==30);
    assert(!dx11::mesh("vehicles/sedan")->textured&&
        dx11::mesh("vehicles/sedan")->textureFile.empty());
    for(int variant=1;variant<=5;++variant){
        const auto* car=dx11::mesh("vehicles/traffic-"+std::to_string(variant));
        const auto* glass=dx11::mesh("vehicles/traffic-"+std::to_string(variant)+"-glass");
        assert(car&&car->textured&&glass&&car->vertices.size()+glass->vertices.size()>900);
        std::ifstream original("assets/models/baked/vehicles/traffic-"+std::to_string(variant)+".m3d",std::ios::binary);
        original.seekg(4);std::uint32_t vertexCount=0;
        original.read(reinterpret_cast<char*>(&vertexCount),sizeof(vertexCount));
        assert(car->vertices.size()+glass->vertices.size()==vertexCount);
        assert(std::filesystem::exists(car->textureFile));
    }
    for(const char* name:{"pistol","ak","lightning"}){
        const auto* gun=dx11::mesh(std::string("weapons/")+name);
        assert(gun&&gun->textured&&gun->vertices.size()>2000);
        assert(!gun->materialRanges.empty());
        for(const auto& range:gun->materialRanges)
            assert(range.count>0&&std::filesystem::exists(range.baseFile));
    }
    assert(dx11::mesh("nature/tree_oak-lod"));
    std::ifstream manifest("assets/models/NATURE_MANIFEST.csv");
    assert(manifest);
    std::set<std::string> manifestModels;
    std::set<std::string> distinctTreeSources;
    std::string row;
    std::getline(manifest,row);
    while(std::getline(manifest,row)){
        std::istringstream fields(row);std::string id,source,baked,url,creator,license,date,credit;
        std::getline(fields,id,',');std::getline(fields,source,',');
        std::getline(fields,baked,',');std::getline(fields,url,',');
        std::getline(fields,creator,',');std::getline(fields,license,',');
        std::getline(fields,date,',');std::getline(fields,credit,',');
        assert(!id.empty()&&url.rfind("https://",0)==0&&
            !creator.empty()&&license=="CC0-1.0"&&credit=="no");
        assert(std::ifstream("assets/models/"+source).good());
        assert(std::ifstream("assets/models/"+baked).good());
        assert(manifestModels.insert(id).second);
        if(id.rfind("tree_",0)==0)distinctTreeSources.insert(source);
    }
    for(const char* name:{"marker/ring","marker/pillar"}){
        const auto* marker=dx11::mesh(name);
        assert(marker&&marker->transparent&&!marker->castsShadow&&
            !marker->vertices.empty());
    }
    assert(distinctTreeSources.size()>=20);
    data_file::Ini catalog;
    assert(catalog.load(data_file::resourcePath("trees.ini"))&&catalog.version(1));
    int count=0;assert(catalog.integer("Trees","Count",count,30,128));
    std::set<std::string> treeModels;
    std::set<std::wstring> treeTextures;
    for(int index=0;index<count;++index){
        std::string name;assert(catalog.string("Tree"+std::to_string(index),"Model",name));
        assert(treeModels.insert(name).second&&manifestModels.count(name));
        std::string key="nature/"+name;
        assert(dx11::mesh(key));
        assert(dx11::mesh(key+"-lod"));
        const auto* tree=dx11::mesh(key);
        const auto* lod=dx11::mesh(key+"-lod");
        assert(tree->vertices.size()>1000&&lod->vertices.size()>100);
        assert(tree->textured&&tree->alphaTest&&
            std::filesystem::exists(tree->textureFile));
        assert(lod->textured&&lod->textureFile==tree->textureFile);
        assert(treeTextures.insert(tree->textureFile).second);
    }
    std::set<std::wstring> bushTextures;
    for(int index=0;index<36;++index){
        std::string name="bush_"+std::string(index<10?"0":"")+std::to_string(index);
        assert(manifestModels.count(name));
        const auto* bush=dx11::mesh("nature/"+name);
        const auto* lod=dx11::mesh("nature/"+name+"-lod");
        assert(bush&&lod&&bush->vertices.size()>1000&&lod->vertices.size()>100);
        assert(bush->textured&&bush->alphaTest&&
            std::filesystem::exists(bush->textureFile));
        assert(lod->textured&&lod->textureFile==bush->textureFile);
        assert(bushTextures.insert(bush->textureFile).second);
    }
    assert(catalog.load(data_file::resourcePath("biome_props.ini"))&&catalog.version(1));
    assert(catalog.integer("Props","Count",count,1,64));
    for(int index=0;index<count;++index){
        std::string name;assert(catalog.string("Prop"+std::to_string(index),"Model",name));
        assert(manifestModels.count(name));
        std::string key="nature/"+name;
        assert(dx11::mesh(key));
    }
    assert(dx11::mesh("primitive/box")->vertices.size()==30);
    assert(dx11::mesh("primitive/sphere")->vertices.size()==360);
    assert(dx11::mesh("primitive/cylinder")->vertices.size()==144);
    for(const char* style:{"wave","terrace","courtyard","bayfront"}){
        std::string name="marina/marina-"+std::string(style);
        const auto* building=dx11::mesh(name);
        const auto* lod=dx11::mesh(name+"-lod");
        assert(building&&lod&&building->vertices.size()>10000&&
            lod->vertices.size()>1000&&building->textured);
        assert(std::filesystem::exists(building->textureFile));
    }
    assert(std::ifstream("assets/models/baked/marina/MarinaFacade_NormalDX.png").good());
    std::puts("asset smoke passed");
}
