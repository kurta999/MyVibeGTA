#pragma once
#include "builder.h"
#include "dx11_assets.h"
#include "destruction.h"
#include <set>

namespace scenery_edits {
enum class Kind {Tree,Plant,Rock};
enum class TraceMask {All,Trees,NonTrees};
struct Object {
    std::string id,model;Kind kind=Kind::Plant;int index=-1;
    game::Vec3 position{},size{};float yaw=0;game::Color tint{1,1,1};
};
using Records=std::map<std::string,std::set<builder::Cell>>;
void clear();
const Records& records();
void restore(Records records);
bool find(const std::string& id,Object& object);
bool treeObject(std::size_t index,Object& object);
bool outcropObject(int x,int z,Object& object);
std::vector<Object> nearby(game::Vec2 focus,float radius);
std::string treeId(std::size_t index);
std::string treeBushId(std::size_t index);
bool edited(const std::string& id);
bool removed(const std::string& id,builder::Cell cell);
std::uint64_t revision(const std::string& id);
bool validCut(const std::string& id,builder::Cell cell);
bool cut(const std::string& id,builder::Cell cell);
bool trace(game::Vec3 origin,game::Vec3 direction,float range,builder::Target& target,TraceMask mask=TraceMask::All);
bool segment(game::Vec3 start,game::Vec3 end,float& fraction,TraceMask mask=TraceMask::All);
bool intersects(const Object& object,game::Vec3 low,game::Vec3 high);
bool appendIfEdited(const std::string& id,std::vector<dx11::ModelInstance>& instances);
std::vector<destruction::Box> trunkPieces(std::size_t index);
// World-space surviving rock surfaces and cut caps, with outward winding for Jolt.
std::vector<dx11::Vertex> collisionTriangles(const Object& object);
}
