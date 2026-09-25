#pragma once
#include <string>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dx11 {
constexpr int MATERIAL_GROUPS=16;
struct Vertex {
    float x,y,z,nx,ny,nz,u,v,r,g,b,a;
};
struct MaterialRange {
    unsigned start=0,count=0;
    float roughness=0.82f,metallic=0,emissive=0;
    std::wstring baseFile,normalFile,ormFile,occlusionFile,emissiveFile;
};
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<MaterialRange> materialRanges;
    float minX=0,minY=0,minZ=0,maxX=0,maxY=0,maxZ=0;
    bool textured=false;
    bool alphaTest=false;
    bool castsShadow=true;
    bool transparent=false;
    std::wstring textureFile;
    float roughness=0.82f;
    float metallic=0.0f;
};
struct SkinVertex {Vertex base;std::uint8_t joints[4];float weights[4];};
struct SkinClip {std::string name;float duration=1;std::vector<std::array<float,16>> palettes;
    std::vector<std::array<float,3>> rightHands;unsigned frames=0;};
struct SkinMesh {std::vector<SkinVertex> vertices;std::vector<SkinClip> clips;
    std::vector<std::uint8_t> bodyPartForJoint;unsigned jointCount=0;};
struct ModelInstance {
    const Mesh* source;
    int material;
    float scaleX,scaleY,scaleZ,cosYaw,sinYaw;
    float x,y,z,centerX,minY,centerZ;
    float r,g,b;
    float sinPitch=0,cosPitch=1;
};
void loadMeshes(const std::wstring& folder);
const Mesh* mesh(const std::string& name);
const SkinMesh* skinMesh(const std::string& name);
void buildScene(std::vector<Vertex> groups[MATERIAL_GROUPS],std::vector<ModelInstance>& instances);
void buildStaticScene(std::vector<Vertex> groups[MATERIAL_GROUPS]);
void buildHud(unsigned char* pixels,int width,int height);
void shutdownHud();
}
