#pragma once
#include <string>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dx11 {
constexpr int MATERIAL_GROUPS=10;
struct Vertex {
    float x,y,z,nx,ny,nz,u,v,r,g,b,a;
};
struct Mesh {
    std::vector<Vertex> vertices;
    float minX=0,minY=0,minZ=0,maxX=0,maxY=0,maxZ=0;
    bool textured=false;
};
struct SkinVertex {Vertex base;std::uint8_t joints[4];float weights[4];};
struct SkinClip {std::string name;float duration=1;std::vector<std::array<float,16>> palettes;unsigned frames=0;};
struct SkinMesh {std::vector<SkinVertex> vertices;std::vector<SkinClip> clips;
    std::vector<std::uint8_t> bodyPartForJoint;unsigned jointCount=0;};
struct ModelInstance {
    const Mesh* source;
    int material;
    float scaleX,scaleY,scaleZ,cosYaw,sinYaw;
    float x,y,z,centerX,minY,centerZ;
    float r,g,b;
};
void loadMeshes(const std::wstring& folder);
const Mesh* mesh(const std::string& name);
const SkinMesh* skinMesh(const std::string& name);
void buildScene(std::vector<Vertex> groups[MATERIAL_GROUPS],std::vector<ModelInstance>& instances);
void buildHud(unsigned char* pixels,int width,int height);
void shutdownHud();
}
