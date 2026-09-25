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
    const auto* grass=dx11::mesh("primitive/grass-tuft");
    const auto* bullet=dx11::mesh("primitive/bullet");
    assert(grass&&grass->vertices.size()==9&&!grass->castsShadow);
    assert(bullet&&bullet->vertices.size()>=100&&!bullet->castsShadow);
    for(const char* name:{"effect/flame","effect/smoke","effect/flash",
                          "effect/shockwave"}){
        const auto* effect=dx11::mesh(name);
        assert(effect&&effect->transparent&&!effect->castsShadow&&
            effect->textured&&std::filesystem::exists(effect->textureFile));
    }
    for(const char* name:{"characters/beach-man","characters/casual-man",
                          "characters/casual-woman","characters/hoodie-man"}){
        const auto* skin=dx11::skinMesh(name);
        assert(skin&&skin->jointCount>0&&skin->vertices.size()>1000);
        assert(skin->bodyPartForJoint.size()==skin->jointCount);
        assert(skin->clips.size()==10);
        assert(skin->clips[8].name=="Fall");
        assert(skin->clips[9].name=="Enter");
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
