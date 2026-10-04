#include "dx11_grass.h"
#include "game_internal.h"
#include "regions.h"
#include <algorithm>
#include <cmath>
namespace dx11::grass {
Surface surfaceAt(game::Vec2 point){
    constexpr float clearance=7;
    if(point.x<clearance||point.z<clearance||point.x>regions::WIDTH-clearance||
       point.z>regions::DEPTH-clearance||game::solid(point,clearance)||
       regions::marinaPierAt(point))return Surface::None;
    for(game::Vec2 offset:{game::Vec2{0,0},{clearance,0},{-clearance,0},
                           {0,clearance},{0,-clearance}})
        if(regions::roadAt(point+offset)||regions::waterAt(point+offset))return Surface::None;
    if(point.x<game::WORLD_W&&point.z<game::WORLD_D){
        for(int column=0;column<5;++column)
            if(std::abs(point.x-(300+column*450.0f))<game::ROAD_W*.5f+clearance)
                return Surface::None;
        for(int row=0;row<4;++row)
            if(std::abs(point.z-(250+row*390.0f))<game::ROAD_W*.5f+clearance)
                return Surface::None;
        if(point.z>=game::BEACH_START)
            return point.z<game::SHORE-25?Surface::Coastal:Surface::None;
        return Surface::Lawn;
    }
    // Match the visible near terrain's 100-unit tile material. A point on the
    // edge of a road/water tile must not sprout through the pavement or water.
    game::Vec2 tile{std::floor(point.x/100)*100+50,std::floor(point.z/100)*100+50};
    if(regions::waterAt(tile)||regions::roadAt(tile))return Surface::None;
    const auto* region=regions::at(point);
    if(region&&region->id=="marina-part"){
        if(point.z>=8810&&point.z<=10015&&point.x>=8040&&point.x<=8137)
            return Surface::None;
        if(point.z>=10035&&point.z<=10710){
            float q=(point.z-10360)/360;
            float edge=8010+770*std::sqrt(std::max(0.0f,1-q*q));
            if(point.x>=edge+7&&point.x<=edge+54)return Surface::None;
        }
    }
    switch(regions::biomeAt(tile)){
        case regions::Biome::City:return Surface::Lawn;
        case regions::Biome::Savanna:return Surface::Savanna;
        case regions::Biome::Desert:return Surface::Desert;
        case regions::Biome::Snow:return Surface::Snow;
        default:return Surface::Meadow;
    }
}
const Profile& profile(Surface surface){
    static const Profile profiles[]={
        {"",0,0,0},{"nature/grass_lawn",.92f,6.5f,5.0f},
        {"nature/grass_meadow",.96f,8.5f,13.0f},
        {"nature/grass_savanna",.74f,7.0f,19.0f},
        {"nature/grass_desert",.065f,5.5f,12.0f},
        {"nature/grass_snow",.18f,5.0f,7.0f},
        {"nature/grass_coastal",.16f,6.0f,10.0f}};
    return profiles[int(surface)];
}
std::uint32_t hash(int x,int z,int variant){
    std::uint32_t value=std::uint32_t(x)*0x9e3779b9u ^
        std::uint32_t(z)*0x85ebca6bu ^std::uint32_t(variant)*0xc2b2ae35u;
    value^=value>>16;value*=0x7feb352du;
    value^=value>>15;value*=0x846ca68bu;return value^(value>>16);
}
float unit(std::uint32_t value){return float(value&0xffffu)/65535.0f;}
float ringFade(float distance,int ring,float radius){
    const float outer=std::min(radius,ring==0?160.0f:ring==1?400.0f:800.0f);
    float fade=std::clamp((outer-distance)/28,0.0f,1.0f);
    if(ring>0)fade*=std::clamp((distance-(ring==1?130.0f:365.0f))/28,0.0f,1.0f);
    return fade;
}
}
