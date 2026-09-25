#pragma once
#include "game.h"
#include <string>
#include <vector>

namespace regions {
constexpr float WIDTH=16800,DEPTH=16800;
enum class Biome { City, Countryside, Snow, Savanna, Desert };
struct Region {std::string id,name;Biome biome;float x0=0,z0=0,x1=0,z1=0;};
struct TreeSpecies {std::string modelId;Biome biome;float width=54,height=68,climbHeight=0;};
struct Decoration {game::Vec2 p;std::string id,modelId;float width=20,height=20,depth=20;};
struct Road {std::string id;game::Vec2 start,end;float width=100;};
struct Hub {std::string id,regionId;game::Vec2 p;int pedestrians=0,vehicles=0;
    std::vector<game::Kind> vehicleCycle;};
struct CityLayout {float x=0,z=0,spacing=0;int rows=0,columns=0;};
bool load(const char* path=nullptr);
const std::string& lastError();
const std::vector<Region>& all();
const std::vector<TreeSpecies>& species();
const std::vector<Decoration>& decorations();
const std::vector<Road>& roads();
const std::vector<Hub>& hubs();
bool homeForPed(const std::string& id,game::Vec2& point);
std::vector<int> nearbyTreeIndices(game::Vec2 point,float radius);
std::vector<int> nearbyDecorationIndices(game::Vec2 point,float radius);
CityLayout secondCity();
const Region* at(game::Vec2 point);
Biome biomeAt(game::Vec2 point);
game::Color groundColor(game::Vec2 point);
bool waterAt(game::Vec2 point);
bool marinaBayAt(game::Vec2 point);
bool marinaPierAt(game::Vec2 point);
bool roadAt(game::Vec2 point);
bool bridgeAt(game::Vec2 point);
bool causewayAt(game::Vec2 point);
void populate();
}
