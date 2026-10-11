#pragma once
#include "game.h"
#include "regions.h"
#include <string>
#include <vector>
namespace world_placements {
enum class VisualKind { Ground, Beam, Model, Prop, Glow, Plant };
struct Visual {
    VisualKind kind=VisualKind::Ground;
    std::string id,model;
    game::Vec3 a{},b{};
    game::Color color{1,1,1};
    float width=0,height=0,yaw=0,scale=1,range=0;
    int tile=-1,material=0;
    bool base=false;
};
struct Light {
    std::string id;
    game::Vec3 p{};
    game::Color color{};
    float radius=0,brightness=0;
    bool street=false;
};
struct Outcrop {
    std::string id,editId,model;
    game::Vec2 p{};
    float width=0,height=0,yaw=0;
};
struct Scene {
    std::vector<game::Building> buildings;
    std::vector<game::Tree> trees;
    std::vector<regions::Decoration> decorations;
    std::vector<game::Ped> pedestrians;
    std::vector<game::Vehicle> vehicles;
    std::vector<Visual> visuals;
    std::vector<Light> lights;
    std::vector<Outcrop> outcrops;
};
bool Load(const char* path=nullptr,unsigned roadCount=1000);
const Scene& Data();
const std::string& LastError();
}
