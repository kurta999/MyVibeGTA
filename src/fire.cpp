#include "fire.h"
#include "data_file.h"
#include "weather.h"
#include "regions.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

namespace fire {
namespace {
constexpr int cellSize=24;
constexpr int columns=int(regions::WIDTH/cellSize);
constexpr int rows=int(regions::DEPTH/cellSize);
struct Cell {
    float burn=0,heat=0,wet=0,charred=0;
    Material material=Material::Grass;
};
std::array<Rule,int(Material::Count)> rules{{
    {1,12,8},{0.02f,1.5f,2},{0.04f,2,3},{0.04f,3,5},
    {0.7f,14,10},{0,0,0},{0,0,0},{0,0,0}
}};
std::unordered_map<int,Cell> cells;
std::vector<Flame> flames;
std::string error;
float stepRemainder=0,damageRemainder=0;
constexpr int maxFlames=500;
int cellIndex(game::Vec2 p){
    if(p.x<0||p.x>=regions::WIDTH||p.z<0||p.z>=regions::DEPTH)return -1;
    int x=int(p.x/cellSize),z=int(p.z/cellSize);
    return z*columns+x;
}
game::Vec2 center(int index){
    return {(index%columns+0.5f)*cellSize,(index/columns+0.5f)*cellSize};
}
void refresh(){
    flames.clear();
    for(const auto& [index,cell]:cells)if(cell.burn>0){
        float duration=rules[int(cell.material)].burnSeconds;
        flames.push_back({center(index),std::min(1.0f,cell.burn/
            std::max(1.0f,duration*0.3f)),cell.material});
    }
}
void hurtNearby(){
    const bool hadSurfaceFlames=!flames.empty();
    if(flames.empty()&&
       std::none_of(game::peds.begin(),game::peds.end(),
           [](const game::Ped& ped){return ped.alive&&ped.burnTime>0;})&&
       std::none_of(game::vehicles.begin(),game::vehicles.end(),
           [](const game::Vehicle& vehicle){return !vehicle.exploded&&vehicle.burnTime>0;}))
        return;
    float playerDamage=0;
    for(const auto& flame:flames)if(game::len(game::player-flame.p)<19)
        playerDamage=std::max(playerDamage,rule(flame.material).damagePerSecond*flame.intensity);
    if(playerDamage>0&&game::health>0&&!game::swimming)
        game::applyDamage(playerDamage*0.5f);
    for(auto& ped:game::peds)if(ped.alive){
        float damage=0;
        for(const auto& flame:flames)if(game::len(ped.p-flame.p)<18)
            damage=std::max(damage,rule(flame.material).damagePerSecond*flame.intensity);
        if(damage>0)ignitePed(ped);
        if(ped.burnTime<=0)continue;
        ped.burnTime=std::max(0.0f,ped.burnTime-0.5f);
        ped.health-=6;
        ped.hitFlash=std::max(ped.hitFlash,0.2f);
        if(ped.health<=0){ped.alive=false;ped.respawn=ped.police?999999:45;
            ped.burnTime=0;ped.corpseVisualDelay=6;
#ifdef MINI_CITY_JOLT
            jolt_world::spawnRagdoll(ped,{0,0,0});
#endif
        }
    }
    for(int index=0;index<int(game::vehicles.size());++index){
        auto& vehicle=game::vehicles[index];
        if(vehicle.exploded)continue;
        bool touchingFlame=false;
        for(const auto& flame:flames)if(game::len(game::vehicles[index].p-flame.p)<25)
            touchingFlame=true;
        if(touchingFlame)igniteVehicle(vehicle);
        if(vehicle.burnTime<=0)continue;
        vehicle.burnTime=std::max(0.0f,vehicle.burnTime-0.5f);
        game::damageVehicle(index,12.0f);
    }
    for(auto& prop:game::props)if(prop.alive){
        for(const auto& flame:flames)if(game::len(prop.p-flame.p)<18){
            prop.health-=std::max(1,int(rule(flame.material).damagePerSecond*0.5f));
            break;
        }
    }
    for(const auto& flame:flames)
        for(int index:regions::nearbyTreeIndices(flame.p,23)){
            if(index<0||std::size_t(index)>=game::trees.size())continue;
            auto& tree=game::trees[index];
            if(!tree.destroyed&&game::len(tree.p-flame.p)<23)tree.burning=true;
        }
    for(auto& tree:game::trees)if(hadSurfaceFlames&&!tree.destroyed){
        if(!tree.burning)continue;
        tree.health-=5;
        if(tree.health<=0){tree.health=0;tree.destroyed=true;tree.burning=false;}
        else ignite(tree.p,Material::Wood);
    }
}
void step(float dt){
    std::unordered_map<int,float> heatGain;
    const auto& conditions=weather::current();
    float rain=conditions.snow?conditions.precipitation*0.35f:
        conditions.precipitation;
    for(auto& [index,source]:cells){
        source.wet=std::max(0.0f,source.wet-dt);
        if(rain>0)source.wet=std::max(source.wet,rain*1.5f);
        source.charred=std::max(0.0f,source.charred-dt);
        if(source.burn<=0){source.heat=std::max(0.0f,source.heat-dt*0.12f);continue;}
        float spread=rules[int(source.material)].spread;
        int x=index%columns,z=index/columns;
        const int neighbors[4]={index-1,index+1,index-columns,index+columns};
        for(int side=0;side<4;++side){
            if((side==0&&x==0)||(side==1&&x==columns-1)||
               (side==2&&z==0)||(side==3&&z==rows-1))continue;
            int targetIndex=neighbors[side];
            auto found=cells.find(targetIndex);
            if(found!=cells.end()&&(found->second.burn>0||found->second.wet>0||
               found->second.charred>0))continue;
            if(rain>0)continue;
            float receptivity=rules[int(groundAt(center(targetIndex)))].spread;
            float downwind=side==0?-conditions.wind.x:side==1?conditions.wind.x:
                side==2?-conditions.wind.z:conditions.wind.z;
            heatGain[targetIndex]+=dt*spread*receptivity*
                std::clamp(1.0f+downwind*0.65f,0.2f,2.0f);
        }
        source.burn=std::max(0.0f,source.burn-dt*(1.0f+rain*4.0f));
        if(source.burn==0)source.charred=30;
    }
    int activeCount=int(flames.size());
    for(const auto& [index,gain]:heatGain)if(gain>0){
        Cell& target=cells[index];
        if(target.wet>0||target.charred>0||target.burn>0)continue;
        target.heat+=gain;
        if(target.heat>=0.8f&&activeCount<maxFlames){
            target.material=groundAt(center(index));
            target.burn=rules[int(target.material)].burnSeconds;
            target.heat=0;
            if(target.burn>0)++activeCount;
        }
    }
    for(auto it=cells.begin();it!=cells.end();)
        if(it->second.burn<=0&&it->second.heat<=0&&it->second.wet<=0&&
           it->second.charred<=0)it=cells.erase(it);
        else ++it;
    refresh();
}
}
bool load(const char* path){
    error.clear();
    data_file::Ini file;
    if(!file.load(path?path:data_file::resourcePath("surfaces.ini"))||!file.version(1)){
        error=file.lastError();return false;
    }
    const char* names[]={"grass","sand","asphalt","metal","wood","concrete","water","snow"};
    std::array<Rule,int(Material::Count)> parsed{};
    for(int index=0;index<int(Material::Count);++index){
        std::string section=std::string("Surface.")+names[index];
        Rule& item=parsed[index];
        if(!file.real(section,"Spread",item.spread,0,10)||
           !file.real(section,"BurnSeconds",item.burnSeconds,0,120)||
           !file.real(section,"DamagePerSecond",item.damagePerSecond,0,100)){
            error=file.lastError();return false;
        }
        if(item.spread>0&&item.burnSeconds<=0){
            error="Invalid ["+section+"] BurnSeconds";return false;
        }
    }
    rules=parsed;return true;
}
const std::string& lastError(){return error;}
Rule rule(Material material){return rules[std::clamp(int(material),0,int(Material::Count)-1)];}
Material groundAt(game::Vec2 p){
    if(regions::causewayAt(p))return Material::Asphalt;
    if(p.x>=game::WORLD_W||p.z>=game::WORLD_D){
        if(regions::waterAt(p))return Material::Water;
        if(regions::roadAt(p)||regions::bridgeAt(p))return Material::Asphalt;
        switch(regions::biomeAt(p)){
        case regions::Biome::Desert:return Material::Sand;
        case regions::Biome::Snow:return Material::Snow;
        default:return Material::Grass;
        }
    }
    if(p.z>=game::SHORE)return Material::Water;
    if(p.z>=game::BEACH_START)return Material::Sand;
    for(int column=0;column<5;++column)
        if(std::abs(p.x-(300+column*450.0f))<game::ROAD_W*0.5f)
            return Material::Asphalt;
    for(int row=0;row<4;++row)
        if(std::abs(p.z-(250+row*390.0f))<game::ROAD_W*0.5f)
            return Material::Asphalt;
    return Material::Grass;
}
Material surfaceAt(game::Vec3 p){
    game::Vec2 horizontal{p.x,p.z};
    for(const auto& vehicle:game::vehicles)
        if(game::len(vehicle.p-horizontal)<27&&p.y>=0&&p.y<45)
            return Material::Metal;
    for(const auto& prop:game::props)
        if(prop.alive&&game::len(prop.p-horizontal)<14&&p.y>=0&&p.y<28)
            return prop.barrel?Material::Metal:Material::Wood;
    for(int index:regions::nearbyDecorationIndices(horizontal,50)){
        if(index<0||std::size_t(index)>=regions::decorations().size())continue;
        const auto& decoration=regions::decorations()[index];
        if(decoration.modelId.rfind("bush_",0)==0)continue;
        if(game::len(decoration.p-horizontal)<decoration.width*0.45f&&
           p.y>=0&&p.y<decoration.height)
            return decoration.modelId.rfind("cactus_",0)==0?
                Material::Wood:Material::Concrete;
    }
    for(int index:regions::nearbyTreeIndices(horizontal,25)){
        if(index<0||std::size_t(index)>=game::trees.size())continue;
        const auto& tree=game::trees[index];
        if(!tree.destroyed&&game::len(tree.p-horizontal)<
               11*std::min(tree.scale,2.2f)&&p.y>=0&&
           p.y<tree.height*tree.scale)
            return Material::Wood;
    }
    for(const auto& building:game::buildings)
        if(p.x>=building.x&&p.x<=building.x+building.w&&
           p.z>=building.z&&p.z<=building.z+building.d&&p.y<=building.h)
            return Material::Concrete;
    return groundAt(horizontal);
}
void reset(){cells.clear();flames.clear();stepRemainder=0;damageRemainder=0;}
bool ignite(game::Vec2 p,Material material){
    int index=cellIndex(p);
    if(index<0)return false;
    Cell& cell=cells[index];
    if(cell.wet>0)return false;
    bool newFire=cell.burn<=0;
    if(newFire&&flames.size()>=maxFlames)return false;
    if(material==Material::Count)material=groundAt(p);
    float duration=rule(material).burnSeconds;
    if(duration<=0)return false;
    cell.material=material;cell.burn=std::max(cell.burn,duration);
    cell.heat=0;cell.charred=0;
    if(newFire)refresh();
    return true;
}
void ignitePed(game::Ped& ped){
    if(!ped.alive)return;
    if(ped.burnTime<=0){
        ped.state=game::PedState::Flee;
        ped.target=ped.p+game::forward(ped.angle+game::PI)*150.0f;
    }
    ped.panic=std::max(ped.panic,4.0f);
    ped.alertTime=std::max(ped.alertTime,4.0f);
    ped.burnTime=std::max(ped.burnTime,12.0f);
}
void igniteVehicle(game::Vehicle& vehicle){
    if(!vehicle.exploded)vehicle.burnTime=std::max(vehicle.burnTime,16.0f);
}
void extinguish(game::Vec2 p,float radius,float strength){
    if(radius<=0||strength<=0)return;
    int x0=std::max(0,int(std::floor((p.x-radius)/cellSize)));
    int x1=std::min(columns-1,int(std::floor((p.x+radius)/cellSize)));
    int z0=std::max(0,int(std::floor((p.z-radius)/cellSize)));
    int z1=std::min(rows-1,int(std::floor((p.z+radius)/cellSize)));
    for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x){
        int index=z*columns+x;
        if(game::len(center(index)-p)>radius)continue;
        Cell& cell=cells[index];
        cell.burn=std::max(0.0f,cell.burn-strength);
        cell.heat=0;cell.wet=std::max(cell.wet,5.0f);
    }
    for(int index:regions::nearbyTreeIndices(p,radius)){
        if(index<0||std::size_t(index)>=game::trees.size())continue;
        auto& tree=game::trees[index];
        if(game::len(tree.p-p)<=radius)tree.burning=false;
    }
    for(auto& ped:game::peds)if(game::len(ped.p-p)<=radius)ped.burnTime=0;
    for(auto& vehicle:game::vehicles)
        if(game::len(vehicle.p-p)<=radius)vehicle.burnTime=0;
    refresh();
}
void update(float dt){
    stepRemainder+=std::clamp(dt,0.0f,0.25f);
    while(stepRemainder>=0.1f){step(0.1f);stepRemainder-=0.1f;}
    damageRemainder+=std::clamp(dt,0.0f,0.25f);
    while(damageRemainder>=0.5f){hurtNearby();damageRemainder-=0.5f;}
}
float intensityAt(game::Vec2 p){
    int index=cellIndex(p);
    auto found=cells.find(index);
    if(index<0||found==cells.end()||found->second.burn<=0)return 0;
    return std::min(1.0f,found->second.burn/
        std::max(1.0f,rule(found->second.material).burnSeconds*0.3f));
}
const std::vector<Flame>& active(){return flames;}
}
