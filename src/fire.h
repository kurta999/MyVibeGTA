#pragma once
#include "game.h"
#include <string>
#include <vector>

namespace fire {
enum class Material { Grass, Sand, Asphalt, Metal, Wood, Concrete, Water, Snow, Count };
struct Rule { float spread=0,burnSeconds=0,damagePerSecond=0; };
struct Flame { game::Vec2 p; float intensity=0; Material material=Material::Grass; };
bool load(const char* path=nullptr);
const std::string& lastError();
Rule rule(Material material);
Material groundAt(game::Vec2 p);
Material surfaceAt(game::Vec3 p);
void reset();
bool ignite(game::Vec2 p,Material material=Material::Count);
void ignitePed(game::Ped& ped);
void igniteVehicle(game::Vehicle& vehicle);
void extinguish(game::Vec2 p,float radius,float strength);
void update(float dt);
float intensityAt(game::Vec2 p);
const std::vector<Flame>& active();
}
