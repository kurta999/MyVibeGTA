#include "../src/dx11_assets.h"
#include "../src/dx11_damage.h"
#include "../src/data_file.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <set>
#include <sstream>

int main(){
    // Subtract a central hole from a textured facade: area, UV interpolation,
    // and the source PBR material survive both the first and overlapping blasts.
    dx11::Mesh facade;
    auto fv=[](float x,float y){return dx11::Vertex{x,y,0,0,0,1,(x+2)/4,(y+2)/4,1,1,1,1};};
    facade.vertices={fv(-2,-2),fv(2,-2),fv(2,2),fv(-2,-2),fv(2,2),fv(-2,2)};
    dx11::MaterialRange material;material.count=6;material.roughness=.19f;material.metallic=.7f;
    facade.materialRanges={material};
    dx11::ModelInstance face{};face.source=&facade;face.scaleX=face.scaleY=face.scaleZ=face.cosYaw=1;
    std::vector<game::BuildingPiece> cuts{{{-1,-1,-1},{1,1,1}}};
    auto area=[](const dx11::Mesh& m){float a=0;for(std::size_t n=0;n<m.vertices.size();n+=3){
        auto p=m.vertices[n],q=m.vertices[n+1],r=m.vertices[n+2];
        a+=std::abs((q.x-p.x)*(r.y-p.y)-(q.y-p.y)*(r.x-p.x))*.5f;}return a;};
    auto cut=dx11::clipBuildingMesh(face,cuts);assert(std::abs(area(cut)-12)<.001f);
    assert(cut.materialRanges.size()==1&&cut.materialRanges[0].roughness==.19f);
    for(const auto& v:cut.vertices){assert(std::abs(v.u-(v.x+2)/4)<.001f);assert(std::abs(v.v-(v.y+2)/4)<.001f);}
    cuts.push_back({{0,-1,-1},{2,1,1}});cut=dx11::clipBuildingMesh(face,cuts);assert(std::abs(area(cut)-10)<.001f);
    static_assert(sizeof(dx11::SkinVertex)==68);
    assert(dx11::chooseDetailedLod(101,80,100,200,false,false));
    assert(dx11::chooseDetailedLod(94,90,100,200,true,true));
    assert(!dx11::chooseDetailedLod(87,90,100,200,true,true));
    assert(!dx11::chooseDetailedLod(105,90,100,200,true,false));
    assert(dx11::chooseDetailedLod(115,90,100,200,true,false));
    assert(!dx11::chooseDetailedLod(200,221,100,200,true,true));
    assert(!dx11::chooseDetailedLod(200,181,100,200,true,false));
    const std::array<float,4> thresholds{500,240,80,0};
    assert(dx11::chooseLodLevel(500,thresholds,false,0)==0);
    assert(dx11::chooseLodLevel(300,thresholds,false,0)==1);
    assert(dx11::chooseLodLevel(130,thresholds,false,0)==2);
    assert(dx11::chooseLodLevel(50,thresholds,false,0)==3);
    assert(dx11::chooseLodLevel(475,thresholds,true,0)==0);
    assert(dx11::chooseLodLevel(430,thresholds,true,0)==1);
    assert(dx11::chooseLodLevel(520,thresholds,true,1)==1);
    assert(dx11::chooseLodLevel(565,thresholds,true,1)==0);
    assert(dx11::chooseLodLevel(205,thresholds,true,1)==2);
    assert(dx11::chooseLodLevel(250,thresholds,true,2)==2);
    assert(dx11::chooseLodLevel(280,thresholds,true,2)==1);
    assert(dx11::chooseLodLevel(70,thresholds,true,2)==3);
    assert(dx11::chooseLodLevel(85,thresholds,true,3)==3);
    assert(dx11::chooseLodLevel(90,thresholds,true,3)==2);
    {
        const auto fixture=std::filesystem::temp_directory_path()/
            ("minicity-m3d2-"+std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(fixture/"weapons");
        const auto meshPath=fixture/"weapons/pistol.m3d";
        const auto materialPath=fixture/"weapons/pistol.pbr";
        auto writeMesh=[&](std::uint32_t lastIndex){
            std::ofstream file(meshPath,std::ios::binary);
            const std::uint32_t counts[2]={3,3};
            const dx11::Vertex vertices[3]={{0,0,0,0,1,0,0,0,1,1,1,1},
                {1,0,0,0,1,0,1,0,1,1,1,1},
                {0,0,1,0,1,0,0,1,1,1,1,1}};
            const std::uint32_t indices[3]={0,1,lastIndex};
            file.write("M3D2",4);
            file.write(reinterpret_cast<const char*>(counts),sizeof(counts));
            file.write(reinterpret_cast<const char*>(vertices),sizeof(vertices));
            file.write(reinterpret_cast<const char*>(indices),sizeof(indices));
        };
        writeMesh(2);
        {std::ofstream file(materialPath);file<<"0 3 0.7 0 0 - - - - - MASK 0.37\n";}
        dx11::loadMeshes(fixture.wstring());
        const auto* masked=dx11::mesh("weapons/pistol");
        assert(masked&&masked->indices.size()==3&&masked->materialRanges.size()==1);
        assert(masked->materialRanges[0].alphaTest&&
            std::abs(masked->materialRanges[0].alphaCutoff-0.37f)<0.0001f);
        const auto catalogPath=fixture/"weapons/pistol.lod";
        {std::ofstream catalog(catalogPath);catalog<<"MCLOD1\nweapons/pistol 500\n../outside 240\nweapons/missing 80\nweapons/missing2 0\n";}
        dx11::loadMeshes(fixture.wstring());
        assert(dx11::mesh("weapons/pistol")&&!dx11::lodChain("weapons/pistol"));
        assert(dx11::assetIssues().size()==1);
        std::filesystem::remove(catalogPath);
        {std::ofstream file(materialPath);file<<"0 3 0.7 0 0 - - - - - BLEND 0.5\n";}
        dx11::loadMeshes(fixture.wstring());
        assert(!dx11::mesh("weapons/pistol"));
        {std::ofstream file(materialPath);file<<"0 3 0.7 0 0 - - - - - MASK 0.37\n";}
        writeMesh(3);
        dx11::loadMeshes(fixture.wstring());
        assert(!dx11::mesh("weapons/pistol"));
        std::filesystem::remove_all(fixture);
    }
    {
        dx11::Mesh opaqueA{},opaqueB{},glass{};
        glass.transparent=true;
        auto instance=[](const dx11::Mesh* source,int material,float x){
            dx11::ModelInstance result{};
            result.source=source;result.material=material;result.x=x;
            result.scaleX=result.scaleY=result.scaleZ=1;
            result.cosYaw=result.cosPitch=1;
            return result;
        };
        std::vector<dx11::ModelInstance> items{
            instance(&glass,3,10),instance(&opaqueA,2,0),
            instance(&glass,3,30),instance(&opaqueB,1,0),
            instance(&glass,3,20)};
        dx11::sortInstancesForRendering(items,0,0,0);
        assert(items[0].source==&opaqueB&&items[1].source==&opaqueA);
        assert(items[2].x==30&&items[3].x==20&&items[4].x==10);
        dx11::sortInstancesForRendering(items,40,0,0);
        assert(items[2].x==10&&items[3].x==20&&items[4].x==30);
        opaqueA.minX=-1;opaqueA.maxX=3;
        opaqueA.minY=0;opaqueA.maxY=4;
        opaqueA.minZ=-2;opaqueA.maxZ=2;
        auto transformed=instance(&opaqueA,2,10);
        transformed.scaleX=2;transformed.centerX=0;
        auto bounds=dx11::instanceBounds(transformed);
        assert(bounds.x==12&&bounds.y==2&&bounds.z==0);
        assert(bounds.radius>6&&bounds.radius<7);
        transformed.qz=std::sqrt(.5f);transformed.qw=std::sqrt(.5f);
        bounds=dx11::instanceBounds(transformed);
        assert(std::abs(bounds.x-8)<.001f&&std::abs(bounds.y-2)<.001f);
        assert(bounds.radius>6&&bounds.radius<7);
    }
    dx11::loadMeshes(L"assets/models/baked");
    for(const char* id:{"pistol","silenced-pistol","smg","shotgun","rifle","sniper","rpg",
        "flamethrower","fire-extinguisher","water-cannon","katana","knife","machete",
        "novelty-toy","rolling-pin","bat","bow","minigun","shovel","grapple-hook",
        "c4","remote-trigger","grenade","smoke-grenade","molotov","flashbang","timed-bomb"}){
        const auto* icon=dx11::mesh(std::string("icons/")+id);
        assert(icon&&icon->textured&&icon->transparent&&icon->unlit&&!icon->castsShadow);
        assert(!icon->allowTessellation&&icon->vertices.size()==6&&icon->maxY>0);
        assert(std::filesystem::exists(icon->textureFile));
    }
    for(const auto& issue:dx11::assetIssues())std::fprintf(stderr,"%s\n",issue.c_str());
    assert(dx11::assetIssues().empty());
    for(const char* id:{"c4","remote-trigger","grenade","smoke-grenade","molotov","flashbang","timed-bomb"}){
        const auto* asset=dx11::mesh(std::string("weapons/")+id);
        assert(asset&&!asset->vertices.empty()&&asset->maxY>asset->minY);
    }
    assert(dx11::mesh("weapons/c4")->textured);
    assert(dx11::mesh("vehicles/sedan")->vehicleWear);
    // Original modern meshes must arrive with contiguous indexed material
    // sections, complete maps, valid LOD bounds and safe glass/history routing.
    for(const char* name:{"coastal-office","terrace-apartments",
            "compact-pistol","carbine","street-lamp","twin-lamp",
            "bench","bin","bollard","bike-rack","planter","hydrant"}){
        const auto* asset=dx11::mesh(std::string("modern/")+name);
        assert(asset&&asset->textured&&asset->wrapTextures&&!asset->indices.empty());
        assert(!asset->allowTessellation);
        assert(!asset->materialRanges.empty());unsigned covered=0;
        for(const auto& range:asset->materialRanges){
            assert(range.start==covered&&range.count%3==0);
            covered+=range.count;
            assert(std::filesystem::exists(range.baseFile));
            assert(std::filesystem::exists(range.normalFile));
            assert(std::filesystem::exists(range.ormFile));
        }
        assert(covered==asset->indices.size());
    }
    for(const char* name:{"coastal-office","terrace-apartments"}){
        const auto* chain=dx11::lodChain(std::string("modern/")+name);
        assert(chain&&chain->meshes[0]&&chain->meshes[3]);
    }
    assert(!dx11::mesh("modern/compact-pistol")->temporalStable);
    assert(!dx11::mesh("modern/carbine")->temporalStable);
    for(const char* name:{"traffic-1","traffic-2","traffic-3","traffic-4","traffic-5","sports-car","sedan"}){
        std::string key=std::string("vehicles/")+name;
        const auto* body=dx11::mesh(key);const auto* glass=dx11::mesh(key+"-glass");
        assert(body&&glass&&glass->transparent&&!glass->castsShadow&&!glass->textured);
        assert(!body->temporalStable&&!glass->temporalStable);
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
            assert(!bird->temporalStable);
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
        assert(!base->temporalStable);
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
    const auto* grass=dx11::mesh("nature/grass_meadow");
    const auto* bullet=dx11::mesh("primitive/bullet");
    assert(grass&&grass->textured&&grass->alphaTest&&grass->grassFoliage);
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
        assert(!gun->indices.empty()&&gun->vertices.size()<gun->indices.size());
        for(auto index:gun->indices)assert(index<gun->vertices.size());
        assert(!gun->materialRanges.empty());
        for(const auto& range:gun->materialRanges)
            assert(range.count>0&&range.start+range.count<=gun->indices.size()&&
                std::filesystem::exists(range.baseFile));
    }
    const auto* indexedPistol=dx11::mesh("weapons/pistol");
    assert(indexedPistol&&indexedPistol->indices.size()==7611);
    assert(indexedPistol->vertices.size()<indexedPistol->indices.size());
    for(auto index:indexedPistol->indices)
        assert(index<indexedPistol->vertices.size());
    assert(indexedPistol->materialRanges[0].count==indexedPistol->indices.size());
    assert(!indexedPistol->materialRanges[0].ormFile.empty());
    assert(std::filesystem::exists(indexedPistol->materialRanges[0].ormFile));
    assert(dx11::mesh("vehicles/sedan")->indices.empty());
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
        const auto* chain=dx11::lodChain(name);
        assert(chain&&chain->meshes[0]==building&&chain->minPixels==thresholds);
        for(int level=1;level<4;++level){
            const auto* reduced=chain->meshes[level];
            assert(reduced&&!reduced->indices.empty()&&reduced->textureFile==building->textureFile);
            float ratio=float(reduced->indices.size())/building->vertices.size();
            assert(level==1?ratio>0.45f&&ratio<0.55f:
                   level==2?ratio>0.17f&&ratio<0.23f:ratio>0.07f&&ratio<0.13f);
            assert(reduced->minX==building->minX&&reduced->maxX==building->maxX&&
                reduced->minY==building->minY&&reduced->maxY==building->maxY&&
                reduced->minZ==building->minZ&&reduced->maxZ==building->maxZ);
        }
    }
    assert(std::ifstream("assets/models/baked/marina/MarinaFacade_NormalDX.png").good());
    std::puts("asset smoke passed");
}
