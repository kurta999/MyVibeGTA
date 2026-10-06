#pragma once
#include "builder.h"
#include "dx11_assets.h"
#include <set>

// Negative cells subtract volume from the generated ground, retaining its original surface.
namespace excavation {
constexpr float PATCH_SIZE=200;
struct Patch {int x=0,z=0;bool operator<(const Patch& b)const{return x!=b.x?x<b.x:z<b.z;}};
struct Face {int item=-1;std::array<dx11::Vertex,3> vertices;};
void clear();
bool validCell(builder::Cell cell);
bool cut(builder::Cell cell);
void restore(const std::set<builder::Cell>& cells);
const std::set<builder::Cell>& cells();
const std::map<Patch,std::uint64_t>& patches();
std::uint64_t revision();
std::uint64_t maskRevision();
bool removed(builder::Cell cell);
bool solid(game::Vec3 point);
float floorBelow(game::Vec3 point);
game::Vec3 surfaceNormal(game::Vec3 point,game::Vec3 direction);
int material(builder::Cell cell);
bool maskedNode(int x,int z);
bool converted(Patch patch);
void clipTriangle(const std::array<dx11::Vertex,3>& triangle,std::vector<dx11::Vertex>& output);
std::vector<Face> faces(Patch patch);
// Replaces precisely the heightfield triangles disabled by masked nodes, plus exposed interiors.
std::vector<dx11::Vertex> collisionTriangles(Patch patch);
}
