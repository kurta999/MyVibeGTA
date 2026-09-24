#include "regions.h"
#include "content.h"
#include "commerce.h"
#include "data_file.h"
#include "game_internal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <random>
#include <set>
#include <unordered_map>

namespace regions {
namespace {
std::vector<Region> definitions;
std::vector<TreeSpecies> treeSpecies;
std::vector<TreeSpecies> propSpecies;
std::vector<Decoration> regionalDecorations;
std::vector<Road> roadSegments;
std::vector<Hub> populationHubs;
std::unordered_map<std::string,game::Vec2> pedHomes;
constexpr float treeCellSize=256;
constexpr int treeColumns=int(WIDTH/treeCellSize)+1;
constexpr int treeRows=int(DEPTH/treeCellSize)+1;
std::vector<std::vector<int>> treeBuckets(treeColumns*treeRows);
std::vector<std::vector<int>> decorationBuckets(treeColumns*treeRows);
std::string error;
float cityX=10700,cityZ=7000,citySpacing=500;
int cityRows=6,cityColumns=7,regionalTrees=6000,regionalProps=900;
int climbFrequency=6,cityPedestrians=48,cityVehicles=12;
std::vector<game::Kind> cityVehicleCycle;
bool parseVehicleCycle(const std::string& source,std::vector<game::Kind>& result){
    result.clear();
    std::size_t start=0;
    while(start<source.size()){
        std::size_t end=source.find(',',start);
        std::string token=data_file::trim(source.substr(start,end-start));
        if(token=="car")result.push_back(game::Kind::Car);
        else if(token=="sport")result.push_back(game::Kind::SportCar);
        else if(token=="bike")result.push_back(game::Kind::Bike);
        else return false;
        if(result.size()>32)return false;
        if(end==std::string::npos)return true;
        start=end+1;
    }
    return false;
}
bool parseBiome(const std::string& value,Biome& biome){
    if(value=="city")biome=Biome::City;
    else if(value=="countryside")biome=Biome::Countryside;
    else if(value=="snow")biome=Biome::Snow;
    else if(value=="savanna")biome=Biome::Savanna;
    else if(value=="desert")biome=Biome::Desert;
    else return false;
    return true;
}
}
bool load(const char* path){
    error.clear();data_file::Ini file;
    if(!file.load(path?path:data_file::resourcePath("regions.ini"))||!file.version(2)){
        error=file.lastError();return false;
    }
    int count=0;
    float parsedCityX=cityX,parsedCityZ=cityZ,parsedSpacing=citySpacing;
    int parsedRows=cityRows,parsedColumns=cityColumns;
    int parsedTrees=regionalTrees,parsedPropCount=regionalProps;
    int parsedClimbFrequency=climbFrequency,parsedCityPedestrians=cityPedestrians;
    int parsedCityVehicles=cityVehicles;
    if(!file.integer("Regions","Count",count,5,20)||
       !file.real("SecondCity","X",parsedCityX,9000,14500)||
       !file.real("SecondCity","Z",parsedCityZ,5500,11000)||
       !file.real("SecondCity","Spacing",parsedSpacing,250,700)||
       !file.integer("SecondCity","Rows",parsedRows,3,8)||
       !file.integer("SecondCity","Columns",parsedColumns,3,8)||
       !file.integer("Population","Trees",parsedTrees,100,10000)||
       !file.integer("Population","Props",parsedPropCount,0,3000)||
       !file.integer("Population","ClimbFrequency",parsedClimbFrequency,1,30)||
       !file.integer("Population","CityPedestrians",parsedCityPedestrians,10,150)||
       !file.integer("Population","CityVehicles",parsedCityVehicles,1,100)){
        error=file.lastError();return false;
    }
    std::string cityCycleText;
    std::vector<game::Kind> parsedCityCycle;
    if(!file.string("SecondCity","VehicleCycle",cityCycleText)){
        error=file.lastError();return false;
    }
    if(!parseVehicleCycle(cityCycleText,parsedCityCycle)){
        error="Invalid [SecondCity] VehicleCycle";return false;
    }
    if(parsedCityX+parsedColumns*parsedSpacing>WIDTH-100||
       parsedCityZ+parsedRows*parsedSpacing>DEPTH-100){
        error="Second city exceeds world bounds";return false;
    }
    std::vector<Region> parsed;std::set<std::string> ids;
    for(int index=0;index<count;++index){
        std::string section="Region"+std::to_string(index),kind;
        Region region{};
        if(!file.string(section,"Id",region.id)||
           !file.string(section,"Name",region.name)||
           !file.string(section,"Biome",kind)||
           !file.real(section,"X0",region.x0,0,WIDTH)||
           !file.real(section,"Z0",region.z0,0,DEPTH)||
           !file.real(section,"X1",region.x1,0,WIDTH)||
           !file.real(section,"Z1",region.z1,0,DEPTH)){
            error=file.lastError();return false;
        }
        if(!data_file::validId(region.id)||!ids.insert(region.id).second||
           !parseBiome(kind,region.biome)||region.x1<=region.x0||region.z1<=region.z0){
            error="Invalid ["+section+"] definition";return false;
        }
        parsed.push_back(region);
    }
    data_file::Ini catalog;
    if(!catalog.load(data_file::resourcePath("trees.ini"))||!catalog.version(1)){
        error=catalog.lastError();return false;
    }
    int speciesCount=0;
    if(!catalog.integer("Trees","Count",speciesCount,20,128)){
        error=catalog.lastError();return false;
    }
    std::vector<TreeSpecies> parsedSpecies;std::set<std::string> models;
    for(int index=0;index<speciesCount;++index){
        const std::string section="Tree"+std::to_string(index);
        TreeSpecies entry{};std::string kind;
        if(!catalog.string(section,"Model",entry.modelId)||
           !catalog.string(section,"Biome",kind)||
           !catalog.real(section,"Width",entry.width,8,150)||
           !catalog.real(section,"Height",entry.height,8,160)||
           !catalog.real(section,"ClimbHeight",entry.climbHeight,0,70)){
            error=catalog.lastError();return false;
        }
        if(entry.modelId.rfind("tree_",0)!=0||
           !models.insert(entry.modelId).second||!parseBiome(kind,entry.biome)||
           entry.biome==Biome::City){
            error="Invalid ["+section+"] tree model or biome";return false;
        }
        parsedSpecies.push_back(entry);
    }
    data_file::Ini propCatalog;
    if(!propCatalog.load(data_file::resourcePath("biome_props.ini"))||
       !propCatalog.version(1)){error=propCatalog.lastError();return false;}
    int propCount=0;
    if(!propCatalog.integer("Props","Count",propCount,1,64)){
        error=propCatalog.lastError();return false;
    }
    std::vector<TreeSpecies> parsedProps;models.clear();
    for(int index=0;index<propCount;++index){
        const std::string section="Prop"+std::to_string(index);
        TreeSpecies entry{};std::string kind;
        if(!propCatalog.string(section,"Model",entry.modelId)||
           !propCatalog.string(section,"Biome",kind)||
           !propCatalog.real(section,"Width",entry.width,3,100)||
           !propCatalog.real(section,"Height",entry.height,1,100)){
            error=propCatalog.lastError();return false;
        }
        if((entry.modelId.rfind("cactus_",0)!=0&&
            entry.modelId.rfind("rock_",0)!=0)||
           !models.insert(entry.modelId).second||!parseBiome(kind,entry.biome)||
           entry.biome!=Biome::Desert){
            error="Invalid ["+section+"] prop model or biome";return false;
        }
        parsedProps.push_back(entry);
    }
    data_file::Ini roadFile;
    if(!roadFile.load(data_file::resourcePath("roads.ini"))||!roadFile.version(1)){
        error=roadFile.lastError();return false;
    }
    int roadCount=0;
    if(!roadFile.integer("Roads","Count",roadCount,2,64)){
        error=roadFile.lastError();return false;
    }
    std::vector<Road> parsedRoads;ids.clear();
    for(int index=0;index<roadCount;++index){
        const std::string section="Road"+std::to_string(index);
        Road road{};
        if(!roadFile.string(section,"Id",road.id)||
           !roadFile.real(section,"X0",road.start.x,0,WIDTH)||
           !roadFile.real(section,"Z0",road.start.z,0,DEPTH)||
           !roadFile.real(section,"X1",road.end.x,0,WIDTH)||
           !roadFile.real(section,"Z1",road.end.z,0,DEPTH)||
           !roadFile.real(section,"Width",road.width,30,180)){
            error=roadFile.lastError();return false;
        }
        if(!data_file::validId(road.id)||!ids.insert(road.id).second||
           !((road.start.x==road.end.x&&road.start.z!=road.end.z)||
             (road.start.z==road.end.z&&road.start.x!=road.end.x))){
            error="Invalid ["+section+"] road";return false;
        }
        parsedRoads.push_back(road);
    }
    int hubCount=0;
    if(!file.integer("Hubs","Count",hubCount,1,20)){
        error=file.lastError();return false;
    }
    std::vector<Hub> parsedHubs;ids.clear();
    for(int index=0;index<hubCount;++index){
        const std::string section="Hub"+std::to_string(index);
        Hub hub{};std::string vehicleCycleText;
        if(!file.string(section,"Id",hub.id)||
           !file.string(section,"RegionId",hub.regionId)||
           !file.real(section,"X",hub.p.x,0,WIDTH)||
           !file.real(section,"Z",hub.p.z,0,DEPTH)||
           !file.integer(section,"Pedestrians",hub.pedestrians,1,40)||
           !file.integer(section,"Vehicles",hub.vehicles,1,10)||
           !file.string(section,"VehicleCycle",vehicleCycleText)){
            error=file.lastError();return false;
        }
        if(!parseVehicleCycle(vehicleCycleText,hub.vehicleCycle)){
            error="Invalid ["+section+"] VehicleCycle";return false;
        }
        auto region=std::find_if(parsed.begin(),parsed.end(),
            [&](const Region& item){return item.id==hub.regionId;});
        if(!data_file::validId(hub.id)||!ids.insert(hub.id).second||
           region==parsed.end()||hub.p.x<region->x0||hub.p.x>=region->x1||
           hub.p.z<region->z0||hub.p.z>=region->z1||
           region->biome==Biome::City){
            error="Invalid ["+section+"] population hub";return false;
        }
        parsedHubs.push_back(hub);
    }
    definitions=std::move(parsed);treeSpecies=std::move(parsedSpecies);
    propSpecies=std::move(parsedProps);roadSegments=std::move(parsedRoads);
    cityX=parsedCityX;cityZ=parsedCityZ;citySpacing=parsedSpacing;
    cityRows=parsedRows;cityColumns=parsedColumns;
    regionalTrees=parsedTrees;regionalProps=parsedPropCount;
    climbFrequency=parsedClimbFrequency;cityPedestrians=parsedCityPedestrians;
    cityVehicles=parsedCityVehicles;
    cityVehicleCycle=std::move(parsedCityCycle);
    populationHubs=std::move(parsedHubs);
    return true;
}
const std::string& lastError(){return error;}
const std::vector<Region>& all(){return definitions;}
const std::vector<TreeSpecies>& species(){return treeSpecies;}
const std::vector<Decoration>& decorations(){return regionalDecorations;}
const std::vector<Road>& roads(){return roadSegments;}
const std::vector<Hub>& hubs(){return populationHubs;}
bool homeForPed(const std::string& id,game::Vec2& point){
    auto found=pedHomes.find(id);
    if(found==pedHomes.end())return false;
    point=found->second;return true;
}
std::vector<int> nearbyIndices(const std::vector<std::vector<int>>& buckets,
    game::Vec2 point,float radius){
    std::vector<int> result;
    if(radius<0)return result;
    int x0=std::clamp(int((point.x-radius)/treeCellSize),0,treeColumns-1);
    int x1=std::clamp(int((point.x+radius)/treeCellSize),0,treeColumns-1);
    int z0=std::clamp(int((point.z-radius)/treeCellSize),0,treeRows-1);
    int z1=std::clamp(int((point.z+radius)/treeCellSize),0,treeRows-1);
    for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x){
        const auto& bucket=buckets[z*treeColumns+x];
        result.insert(result.end(),bucket.begin(),bucket.end());
    }
    std::sort(result.begin(),result.end());
    return result;
}
std::vector<int> nearbyTreeIndices(game::Vec2 point,float radius){
    return nearbyIndices(treeBuckets,point,radius);
}
std::vector<int> nearbyDecorationIndices(game::Vec2 point,float radius){
    return nearbyIndices(decorationBuckets,point,radius);
}
CityLayout secondCity(){return {cityX,cityZ,citySpacing,cityRows,cityColumns};}
const Region* at(game::Vec2 point){
    for(auto it=definitions.rbegin();it!=definitions.rend();++it)
        if(point.x>=it->x0&&point.x<it->x1&&point.z>=it->z0&&point.z<it->z1)
            return &*it;
    return nullptr;
}
Biome biomeAt(game::Vec2 point){
    const Region* region=at(point);return region?region->biome:Biome::Countryside;
}
game::Color groundColor(game::Vec2 point){
    switch(biomeAt(point)){
    case Biome::City:return game::rgb(130,165,124);
    case Biome::Snow:return game::rgb(220,227,225);
    case Biome::Savanna:return game::rgb(176,170,98);
    case Biome::Desert:return game::rgb(221,190,127);
    default:return game::rgb(128,173,106);
    }
}
bool bridgeAt(game::Vec2 point){
    return point.x>=7600&&point.x<8000&&std::abs(point.z-8500)<100;
}
bool causewayAt(game::Vec2 point){
    return std::abs(point.x-1200)<60&&point.z>=1600&&point.z<2220;
}
bool marinaBayAt(game::Vec2 point){
    if(point.x<8000||point.x>8780||point.z<10000||point.z>10720)return false;
    float east=(point.x-8010)/770.0f;
    float northSouth=(point.z-10360)/360.0f;
    return east*east+northSouth*northSouth<1.0f;
}
bool marinaPierAt(game::Vec2 point){
    struct Pier {float x0,x1,z;};
    constexpr Pier piers[]={{8300,8730,10220},{8350,8780,10390},
        {8260,8670,10560}};
    for(const auto& pier:piers)
        if(point.x>=pier.x0&&point.x<=pier.x1&&
           std::abs(point.z-pier.z)<=11)return true;
    return false;
}
bool waterAt(game::Vec2 point){
    if(marinaBayAt(point))return !marinaPierAt(point);
    if(point.x>=0&&point.x<game::WORLD_W&&point.z>=game::SHORE&&
       point.z<game::WORLD_D)return !causewayAt(point);
    return point.x>=7600&&point.x<8000&&!bridgeAt(point);
}
bool roadAt(game::Vec2 point){
    if(point.x<game::WORLD_W&&point.z<game::WORLD_D)return causewayAt(point);
    for(const Road& road:roadSegments){
        float half=road.width*0.5f;
        if(road.start.x==road.end.x){
            if(std::abs(point.x-road.start.x)<half&&
               point.z>=std::min(road.start.z,road.end.z)-half&&
               point.z<=std::max(road.start.z,road.end.z)+half)return true;
        }else if(std::abs(point.z-road.start.z)<half&&
                 point.x>=std::min(road.start.x,road.end.x)-half&&
                 point.x<=std::max(road.start.x,road.end.x)+half)return true;
    }
    if(point.x>=cityX-100&&point.x<=cityX+cityColumns*citySpacing+100&&
       point.z>=cityZ-100&&point.z<=cityZ+cityRows*citySpacing+100){
        for(int column=0;column<=cityColumns;++column)
            if(std::abs(point.x-(cityX+column*citySpacing))<52)return true;
        for(int row=0;row<=cityRows;++row)
            if(std::abs(point.z-(cityZ+row*citySpacing))<52)return true;
    }
    return false;
}
void populate(){
    std::mt19937 random(6221);
    regionalDecorations.clear();
    pedHomes.clear();
    for(auto& bucket:treeBuckets)bucket.clear();
    for(auto& bucket:decorationBuckets)bucket.clear();
    std::uniform_real_distribution<float> height(125,300);
    for(int row=0;row<cityRows;++row)for(int column=0;column<cityColumns;++column){
        float x=cityX+column*citySpacing+82;
        float z=cityZ+row*citySpacing+82;
        game::buildings.push_back({x,z,citySpacing-164,citySpacing-164,height(random),
            game::rgb(150+column%4*14,157+row%3*13,159+column%3*11),
            "east-city-"+std::to_string(row)+"-"+std::to_string(column)});
    }
    // The west-facing riverfront follows Marina Part's Danubius street and
    // promenade, with lower courtyard blocks set back from the Danube.
    struct MarinaBlock {float x,z,w,d,h;const char* style;};
    const MarinaBlock marinaBlocks[]={
        {8175,8940,185,170,166,"wave"},{8180,9130,175,118,138,"terrace"},
        {8175,9360,185,168,160,"wave"},{8185,9550,175,158,143,"terrace"},
        {8175,9820,185,166,170,"wave"},
        {8600,8945,150,270,145,"courtyard"},{8810,8945,160,270,154,"courtyard"},
        {9060,8945,155,270,150,"courtyard"},{9300,8945,160,270,140,"terrace"},
        {8600,9380,150,275,155,"courtyard"},{8810,9380,160,275,150,"courtyard"},
        {9060,9380,155,275,158,"courtyard"},{9300,9380,160,275,145,"terrace"},
        {8890,9840,250,195,142,"bayfront"},{9300,9840,165,205,151,"courtyard"}
    };
    for(int index=0;index<int(std::size(marinaBlocks));++index){
        const auto& b=marinaBlocks[index];
        game::buildings.push_back({b.x,b.z,b.w,b.d,b.h,
            game::rgb(224,216,196),"marina-"+std::string(b.style)+"-"+std::to_string(index)});
    }
    for(const auto& shop:commerce::shops)
        if(biomeAt(shop.p)!=Biome::City)
            game::buildings.push_back({shop.p.x-15,shop.p.z+45,30,28,27,
                game::rgb(159,129,93),"outpost-shop-"+shop.id});
    for(const auto& house:commerce::houses)
        if(biomeAt(house.p)!=Biome::City)
            game::buildings.push_back({house.p.x-15,house.p.z+45,30,30,31,
                game::rgb(172,158,133),"outpost-house-"+house.id});
    std::uniform_real_distribution<float> xChoice(100,WIDTH-100),zChoice(100,DEPTH-100);
    std::array<int,5> biomeCounts{};
    for(int index=0;index<regionalTrees;){
        game::Vec2 point{xChoice(random),zChoice(random)};
        if(waterAt(point)||roadAt(point)||biomeAt(point)==Biome::City||
           game::solid(point,24))continue;
        Biome biome=biomeAt(point);
        if(biome==Biome::Desert&&random()%4!=0)continue;
        std::vector<const TreeSpecies*> candidates;
        for(const auto& entry:treeSpecies)if(entry.biome==biome)candidates.push_back(&entry);
        if(candidates.empty())continue;
        int slot=biomeCounts[int(biome)]++%int(candidates.size());
        const TreeSpecies& selected=*candidates[slot];
        game::Tree tree{};tree.id="regional-tree-"+std::to_string(index);
        tree.p=point;tree.variant=index%3;
        tree.modelId=selected.modelId;tree.crownWidth=selected.width;
        tree.height=selected.height;
        tree.palm=selected.modelId.rfind("tree_palm",0)==0;
        tree.scale=0.68f+0.11f*float(index%9);
        if(index%climbFrequency==0)
            tree.climbHeight=selected.climbHeight*tree.scale;
        game::trees.push_back(tree);++index;
    }
    int marinaTree=0;
    for(float z=8910;z<=9960;z+=95){
        for(float x:{8085.0f,8420.0f,8775.0f,9255.0f}){
            game::Vec2 point{x,z};
            if(game::solid(point,17)||roadAt(point))continue;
            game::Tree tree{};tree.id="marina-tree-"+std::to_string(marinaTree++);
            tree.p=point;tree.modelId=(marinaTree%3==0?"tree_oak":"tree_detailed");
            tree.crownWidth=47;tree.height=65;tree.scale=0.75f+0.07f*float(marinaTree%4);
            game::trees.push_back(tree);
        }
    }
    // Concentrate additional trees in the open countryside rather than
    // spreading them across roads, buildings, or other biomes.
    constexpr std::array<float,5> forestXs{3000,4000,5000,6000,7000};
    constexpr std::array<float,4> forestZs{1300,3900,6400,9000};
    auto forestCenter=[&](int grove){
        return game::Vec2{forestXs[grove%int(forestXs.size())],
                          forestZs[grove/int(forestXs.size())]};
    };
    auto openForestGround=[&](game::Vec2 point,float clearance){
        if(biomeAt(point)!=Biome::Countryside||game::solid(point,clearance))return false;
        for(game::Vec2 offset: {game::Vec2{0,0}, {clearance,0},
                                {-clearance,0}, {0,clearance}, {0,-clearance}})
            if(roadAt(point+offset))return false;
        return true;
    };
    std::vector<const TreeSpecies*> forestSpecies;
    for(const auto& entry:treeSpecies)
        if(entry.biome==Biome::Countryside&&
           entry.modelId!="tree_fat"&&entry.modelId!="tree_urbanCherry")
            forestSpecies.push_back(&entry);
    auto giantOak=std::find_if(treeSpecies.begin(),treeSpecies.end(),
        [](const TreeSpecies& entry){return entry.modelId=="tree_oak";});
    auto giantTall=std::find_if(treeSpecies.begin(),treeSpecies.end(),
        [](const TreeSpecies& entry){return entry.modelId=="tree_tall";});
    std::mt19937 forestRandom(90627);
    std::uniform_real_distribution<float> forestUnit(0.0f,1.0f);
    constexpr float fullTurn=6.28318530718f;
    for(int grove=0;grove<int(forestXs.size()*forestZs.size());++grove){
        const game::Vec2 center=forestCenter(grove);
        for(int local=0;local<130;++local){
            float angle=fullTurn*forestUnit(forestRandom);
            float radius=305.0f*std::sqrt(forestUnit(forestRandom));
            game::Vec2 point{center.x+radius*std::cos(angle),
                             center.z+radius*std::sin(angle)};
            bool giant=local<3&&giantOak!=treeSpecies.end()&&giantTall!=treeSpecies.end();
            float scale=giant?(local==0?10.0f:5.2f+float(grove%3)*0.4f):
                0.58f+0.13f*float((grove*3+local)%10);
            const TreeSpecies& selected=giant?
                (local==0?*giantTall:*giantOak):
                *forestSpecies[(grove*13+local)%forestSpecies.size()];
            if(!openForestGround(point,giant?selected.width*scale*0.25f:28))continue;
            game::Tree tree{};
            tree.id="forest-tree-"+std::to_string(grove)+"-"+std::to_string(local);
            tree.p=point;tree.modelId=selected.modelId;
            tree.crownWidth=selected.width*(giant?0.56f:1.0f);
            tree.height=selected.height;tree.scale=scale;
            if(!giant&&local%climbFrequency==0)
                tree.climbHeight=selected.climbHeight*tree.scale;
            game::trees.push_back(tree);
        }
    }
    for(int index=0;index<int(game::trees.size());++index){
        const auto& point=game::trees[index].p;
        int x=std::clamp(int(point.x/treeCellSize),0,treeColumns-1);
        int z=std::clamp(int(point.z/treeCellSize),0,treeRows-1);
        treeBuckets[z*treeColumns+x].push_back(index);
    }
    for(int index=0;index<regionalProps;){
        game::Vec2 point{xChoice(random),zChoice(random)};
        if(waterAt(point)||roadAt(point)||biomeAt(point)!=Biome::Desert||
           game::solid(point,18))continue;
        const auto& selected=propSpecies[index%propSpecies.size()];
        regionalDecorations.push_back({point,"desert-prop-"+std::to_string(index),
            selected.modelId,selected.width,selected.height,selected.width});
        int x=std::clamp(int(point.x/treeCellSize),0,treeColumns-1);
        int z=std::clamp(int(point.z/treeCellSize),0,treeRows-1);
        decorationBuckets[z*treeColumns+x].push_back(index);
        ++index;
    }
    // Decorations are rendered and indexed for culling, but never enter the
    // building/tree collision or physics collections.
    for(int grove=0;grove<int(forestXs.size()*forestZs.size());++grove){
        const game::Vec2 center=forestCenter(grove);
        for(int local=0;local<300;++local){
            float angle=fullTurn*forestUnit(forestRandom);
            float radius=325.0f*std::sqrt(forestUnit(forestRandom));
            game::Vec2 point{center.x+radius*std::cos(angle),
                             center.z+radius*std::sin(angle)};
            if(!openForestGround(point,9))continue;
            int variant=(grove*19+local*7)%36;
            std::string modelId="bush_"+
                std::string(variant<10?"0":"")+std::to_string(variant);
            float width=7.5f+float((grove+local)%8)*1.35f;
            float height=4.5f+float((local*3+grove)%7)*1.1f;
            int index=int(regionalDecorations.size());
            regionalDecorations.push_back({point,
                "forest-bush-"+std::to_string(grove)+"-"+std::to_string(local),
                modelId,width,height,width});
            int x=std::clamp(int(point.x/treeCellSize),0,treeColumns-1);
            int z=std::clamp(int(point.z/treeCellSize),0,treeRows-1);
            decorationBuckets[z*treeColumns+x].push_back(index);
        }
    }
    for(int index=0;index<cityPedestrians;++index){
        game::Vec2 point{};
        for(int attempt=0;attempt<1000;++attempt){
            point={cityX+float(random()%unsigned(cityColumns*int(citySpacing))),
                cityZ+float(random()%unsigned(cityRows*int(citySpacing)))};
            if(!game::solid(point,13))break;
        }
        game::Ped ped{};ped.id="east-ped-"+std::to_string(index);
        ped.p=ped.target=point;ped.speed=28+float(index%27);ped.style=index%4;
        ped.shirt=game::rgb(90+index%130,85+(index*13)%140,90+(index*19)%130);
        ped.armed=index%7==0;ped.cash=content::rollPedCash(ped.armed);
        pedHomes.emplace(ped.id,ped.p);
        game::peds.push_back(ped);
    }
    for(int index=0;index<28;++index){
        game::Vec2 point{index%2==0?8125.0f:8580.0f+float((index%4)*200),
            8900.0f+float((index*139)%1050)};
        if(game::solid(point,13))point={8500.0f,9000.0f+float(index%10)*95};
        game::Ped ped{};ped.id="marina-ped-"+std::to_string(index);
        ped.p=ped.target=point;ped.speed=26+float(index%18);ped.style=index%4;
        ped.shirt=game::rgb(85+(index*29)%145,91+(index*37)%125,96+(index*17)%130);
        ped.cash=content::rollPedCash(false);pedHomes.emplace(ped.id,point);
        game::peds.push_back(ped);
    }
    for(int index=0;index<8;++index){
        game::Vehicle vehicle{};vehicle.id="marina-car-"+std::to_string(index);
        vehicle.kind=index%4==0?game::Kind::SportCar:game::Kind::Car;
        vehicle.p={index<4?8500.0f:9680.0f,
            8900.0f+float(index%4)*250.0f};
        vehicle.c=game::rgb(83+(index*37)%140,94+(index*41)%130,107+(index*53)%120);
        game::vehicles.insert(game::vehicles.begin(),vehicle);
    }
    for(int index=0;index<3;++index){
        game::Vehicle boat{};boat.id="marina-boat-"+std::to_string(index);
        boat.kind=game::Kind::Boat;boat.p={8170.0f+index*125.0f,10310.0f+index*65.0f};
        boat.angle=game::PI*0.5f;
        boat.c=game::rgb(217-index*31,226-index*17,231-index*9);
        game::vehicles.insert(game::vehicles.begin(),boat);
    }
    for(int index=0;index<cityVehicles;++index){
        game::Vehicle vehicle{};vehicle.id="east-vehicle-"+std::to_string(index);
        vehicle.kind=cityVehicleCycle[index%cityVehicleCycle.size()];
        vehicle.p={cityX+float(index%cityColumns)*citySpacing+20,
            cityZ+float(index/cityColumns)*citySpacing+140};
        vehicle.c=game::rgb(85+(index*23)%145,85+(index*37)%145,85+(index*53)%145);
        game::vehicles.insert(game::vehicles.begin(),vehicle);
    }
    for(const Hub& hub:populationHubs){
        for(int index=0;index<hub.pedestrians;++index){
            float angle=index*2.399963f;
            float radius=55.0f+float(index%3)*26.0f;
            game::Vec2 point=hub.p+game::Vec2{std::cos(angle)*radius,
                std::sin(angle)*radius};
            if(game::solid(point,13))point=hub.p+game::Vec2{0,95+index*4.0f};
            game::Ped ped{};ped.id=hub.id+"-ped-"+std::to_string(index);
            ped.p=ped.target=point;ped.speed=24+float(index%25);
            ped.style=index%4;ped.shirt=game::rgb(86+(index*29)%120,
                93+(index*31)%120,91+(index*17)%120);
            ped.armed=index%5==0;ped.cash=content::rollPedCash(ped.armed);
            pedHomes.emplace(ped.id,ped.p);
            game::peds.push_back(ped);
        }
        for(int index=0;index<hub.vehicles;++index){
            game::Vehicle vehicle{};
            vehicle.id=hub.id+"-vehicle-"+std::to_string(index);
            vehicle.kind=hub.vehicleCycle[index%hub.vehicleCycle.size()];
            vehicle.p=hub.p+game::Vec2{-70.0f+index*140.0f,0};
            vehicle.c=game::rgb(90+(index*57)%130,93+(index*83)%130,
                99+(index*43)%130);
            game::vehicles.insert(game::vehicles.begin(),vehicle);
        }
    }
}
}
