#include "regions.h"
#include "world_placements.h"
#ifdef MINI_CITY_JOLT
#include "terrain.h"
#endif
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
std::size_t indexedTreeCount=0;
std::vector<std::vector<int>> decorationBuckets(treeColumns*treeRows);
std::string error;
Geography map;
float cityX=0,cityZ=0,citySpacing=0;
int cityRows=0,cityColumns=0;
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
bool load(const char* path,const char* placementsPath,const char* geographyPath){
    error.clear();data_file::Ini file;
    if(!file.load(path?path:data_file::resourcePath("regions.ini"))||!file.version(3)){
        error=file.lastError();return false;
    }
    int count=0;
    float parsedCityX=cityX,parsedCityZ=cityZ,parsedSpacing=citySpacing;
    int parsedRows=cityRows,parsedColumns=cityColumns;
    if(!file.integer("Regions","Count",count,5,20)||
       !file.real("SecondCity","X",parsedCityX,9000,14500)||
       !file.real("SecondCity","Z",parsedCityZ,5500,11000)||
       !file.real("SecondCity","Spacing",parsedSpacing,250,700)||
       !file.integer("SecondCity","Rows",parsedRows,3,8)||
       !file.integer("SecondCity","Columns",parsedColumns,3,8)){
        error=file.lastError();return false;
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
        Hub hub{};
        if(!file.string(section,"Id",hub.id)||
           !file.string(section,"RegionId",hub.regionId)||
           !file.real(section,"X",hub.p.x,0,WIDTH)||
           !file.real(section,"Z",hub.p.z,0,DEPTH)){
            error=file.lastError();return false;
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
    data_file::Ini geographyFile;Geography parsedMap;
    if(!geographyFile.load(geographyPath?geographyPath:data_file::resourcePath("geography.ini"))||!geographyFile.version(1)){
        error=geographyFile.lastError();return false;
    }
    auto rectangle=[&](const char* section,Rect& rectangle){
        return geographyFile.real(section,"X0",rectangle.x0,0,WIDTH)&&geographyFile.real(section,"Z0",rectangle.z0,0,DEPTH)&&
            geographyFile.real(section,"X1",rectangle.x1,0,WIDTH)&&geographyFile.real(section,"Z1",rectangle.z1,0,DEPTH)&&
            rectangle.x0<rectangle.x1&&rectangle.z0<rectangle.z1;
    };
    int pierCount=0;
    if(!rectangle("River",parsedMap.river)||!rectangle("Bridge",parsedMap.bridge)||
       !rectangle("Causeway",parsedMap.causeway)||!rectangle("Sea",parsedMap.sea)||
       !rectangle("BayBounds",parsedMap.bayBounds)||!rectangle("Promenade",parsedMap.promenade)||
       !geographyFile.real("Bay","X",parsedMap.bayCenter.x,0,WIDTH)||
       !geographyFile.real("Bay","Z",parsedMap.bayCenter.z,0,DEPTH)||
       !geographyFile.real("Bay","RadiusX",parsedMap.bayRadius.x,1,WIDTH)||
       !geographyFile.real("Bay","RadiusZ",parsedMap.bayRadius.z,1,DEPTH)||
       !geographyFile.real("Shoreline","Z0",parsedMap.shorelineZ0,0,DEPTH)||
       !geographyFile.real("Shoreline","Z1",parsedMap.shorelineZ1,0,DEPTH)||
       !geographyFile.real("Shoreline","Inner",parsedMap.shorelineInner,0,1000)||
       !geographyFile.real("Shoreline","Outer",parsedMap.shorelineOuter,0,1000)||
       !geographyFile.integer("Piers","Count",pierCount,0,128)||
       parsedMap.shorelineZ0>=parsedMap.shorelineZ1||parsedMap.shorelineInner>=parsedMap.shorelineOuter){
        error="Invalid geography: "+geographyFile.lastError();return false;
    }
    for(int i=0;i<pierCount;++i){
        const auto section="Pier"+std::to_string(i);Pier pier;
        if(!geographyFile.real(section,"X0",pier.x0,0,WIDTH)||!geographyFile.real(section,"X1",pier.x1,0,WIDTH)||
           !geographyFile.real(section,"Z",pier.z,0,DEPTH)||!geographyFile.real(section,"HalfWidth",pier.halfWidth,.1f,100)||pier.x0>=pier.x1){
            error="Invalid pier: "+geographyFile.lastError();return false;
        }
        parsedMap.piers.push_back(pier);
    }
    if(!rectangle("Downtown",parsedMap.downtown)||
       !geographyFile.real("DowntownRoads","X",parsedMap.roadOrigin.x,0,WIDTH)||
       !geographyFile.real("DowntownRoads","Z",parsedMap.roadOrigin.z,0,DEPTH)||
       !geographyFile.real("DowntownRoads","SpacingX",parsedMap.roadSpacing.x,1,2000)||
       !geographyFile.real("DowntownRoads","SpacingZ",parsedMap.roadSpacing.z,1,2000)||
       !geographyFile.real("DowntownRoads","Width",parsedMap.roadWidth,1,250)||
       !geographyFile.real("DowntownRoads","TrafficInset",parsedMap.trafficInset,1,500)||
       !geographyFile.integer("DowntownRoads","Columns",parsedMap.roadColumns,1,64)||
       !geographyFile.integer("DowntownRoads","Rows",parsedMap.roadRows,1,64)||
       parsedMap.roadOrigin.x+(parsedMap.roadColumns-1)*parsedMap.roadSpacing.x>parsedMap.downtown.x1||
       parsedMap.roadOrigin.z+(parsedMap.roadRows-1)*parsedMap.roadSpacing.z>parsedMap.downtown.z1){
        error="Invalid downtown road layout: "+geographyFile.lastError();return false;
    }
    auto centers=[&](const char* category,const char* prefix,std::vector<game::Vec2>& values){
        int count=0;if(!geographyFile.integer(category,"Count",count,0,512))return false;
        for(int i=0;i<count;++i){
            auto section=std::string(prefix)+std::to_string(i);game::Vec2 p;
            if(!geographyFile.real(section,"X",p.x,0,WIDTH)||!geographyFile.real(section,"Z",p.z,0,DEPTH))return false;
            values.push_back(p);
        }
        return true;
    };
    if(!centers("Groves","Grove",parsedMap.groves)||!centers("Flocks","Flock",parsedMap.flocks)){
        error=geographyFile.lastError();return false;
    }
    if(!world_placements::Load(placementsPath,unsigned(parsedRoads.size()))){error=world_placements::LastError();return false;}
    map=std::move(parsedMap);
    definitions=std::move(parsed);treeSpecies=std::move(parsedSpecies);
    propSpecies=std::move(parsedProps);roadSegments=std::move(parsedRoads);
    cityX=parsedCityX;cityZ=parsedCityZ;citySpacing=parsedSpacing;
    cityRows=parsedRows;cityColumns=parsedColumns;
    for(auto& hub:parsedHubs){
        for(const auto& pedestrian:world_placements::Data().pedestrians)
            if(pedestrian.id.rfind(hub.id+"-ped-",0)==0)++hub.pedestrians;
        for(const auto& vehicle:world_placements::Data().vehicles)
            if(vehicle.id.rfind(hub.id+"-vehicle-",0)==0){++hub.vehicles;hub.vehicleCycle.push_back(vehicle.kind);}
    }
    populationHubs=std::move(parsedHubs);
#ifdef MINI_CITY_JOLT
    if(!terrain::load()){error=terrain::lastError();return false;}
#endif
    return true;
}
const std::string& lastError(){return error;}
const Geography& geography(){return map;}
bool downtownRoadAt(game::Vec2 point,float clearance,float widthFactor){
    if(point.x<map.downtown.x0||point.x>=map.downtown.x1||point.z<map.downtown.z0||point.z>=map.downtown.z1)return false;
    float half=map.roadWidth*widthFactor+clearance;
    for(int column=0;column<map.roadColumns;++column)
        if(std::abs(point.x-(map.roadOrigin.x+column*map.roadSpacing.x))<half)return true;
    for(int row=0;row<map.roadRows;++row)
        if(std::abs(point.z-(map.roadOrigin.z+row*map.roadSpacing.z))<half)return true;
    return false;
}
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
    // Scenario staging and runtime replacement can change the population.
    if(indexedTreeCount!=game::trees.size()){
        std::vector<int> result;
        for(int i=0;i<int(game::trees.size());++i)
            if(game::len(game::trees[i].p-point)<=radius)result.push_back(i);
        return result;
    }
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
    return point.x>=map.bridge.x0&&point.x<map.bridge.x1&&point.z>map.bridge.z0&&point.z<map.bridge.z1;
}
bool causewayAt(game::Vec2 point){
    return point.x>map.causeway.x0&&point.x<map.causeway.x1&&point.z>=map.causeway.z0&&point.z<map.causeway.z1;
}
bool marinaBayAt(game::Vec2 point){
    if(point.x<map.bayBounds.x0||point.x>map.bayBounds.x1||point.z<map.bayBounds.z0||point.z>map.bayBounds.z1)return false;
    float east=(point.x-map.bayCenter.x)/map.bayRadius.x;
    float northSouth=(point.z-map.bayCenter.z)/map.bayRadius.z;
    return east*east+northSouth*northSouth<1.0f;
}
bool marinaPierAt(game::Vec2 point){
    for(const auto& pier:map.piers)
        if(point.x>=pier.x0&&point.x<=pier.x1&&
           std::abs(point.z-pier.z)<=pier.halfWidth)return true;
    return false;
}
bool waterAt(game::Vec2 point){
    if(marinaBayAt(point))return !marinaPierAt(point);
    if(point.x>=map.sea.x0&&point.x<map.sea.x1&&point.z>=map.sea.z0&&
       point.z<map.sea.z1)return !causewayAt(point);
    return point.x>=map.river.x0&&point.x<map.river.x1&&point.z>=map.river.z0&&point.z<map.river.z1&&!bridgeAt(point);
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
    const auto& scene=world_placements::Data();
    game::buildings.insert(game::buildings.end(),scene.buildings.begin(),scene.buildings.end());
    game::trees.insert(game::trees.end(),scene.trees.begin(),scene.trees.end());
    game::vehicles.insert(game::vehicles.begin(),scene.vehicles.begin(),scene.vehicles.end());
    regionalDecorations=scene.decorations;pedHomes.clear();
    for(auto pedestrian:scene.pedestrians){
        pedestrian.cash=content::rollPedCash(pedestrian.armed);
        pedHomes.emplace(pedestrian.id,pedestrian.p);game::peds.push_back(std::move(pedestrian));
    }
    for(auto& bucket:treeBuckets)bucket.clear();
    for(auto& bucket:decorationBuckets)bucket.clear();
    auto bucket=[&](game::Vec2 p){
        int x=std::clamp(int(p.x/treeCellSize),0,treeColumns-1);
        int z=std::clamp(int(p.z/treeCellSize),0,treeRows-1);return z*treeColumns+x;
    };
    for(int i=0;i<int(game::trees.size());++i)treeBuckets[bucket(game::trees[i].p)].push_back(i);
    for(int i=0;i<int(regionalDecorations.size());++i)decorationBuckets[bucket(regionalDecorations[i].p)].push_back(i);
    indexedTreeCount=game::trees.size();
}
}
