#pragma once
#include <string>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>
namespace cpu {class Pool;}

namespace dx11 {
constexpr int MATERIAL_GROUPS=16;
struct Vertex {
    float x,y,z,nx,ny,nz,u,v,r,g,b,a;
};
struct MaterialRange {
    unsigned start=0,count=0;
    float roughness=0.82f,metallic=0,emissive=0;
    bool alphaTest=false;
    float alphaCutoff=0.5f;
    float clearcoat=0,clearcoatRoughness=0.1f,glassIor=0;
    std::wstring baseFile,normalFile,ormFile,occlusionFile,emissiveFile;
};
struct Mesh {
    std::uint64_t revision=0; // Session meshes replace their GPU buffers after another blast.
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MaterialRange> materialRanges;
    float minX=0,minY=0,minZ=0,maxX=0,maxY=0,maxZ=0;
    bool textured=false;
    bool alphaTest=false;
    bool castsShadow=true;
    bool transparent=false;
    bool unlit=false;
    bool temporalStable=true;
    bool grassFoliage=false;
    bool wrapTextures=false;
    bool vehicleWear=false;
    bool allowTessellation=true;
    const Mesh* shadowProxy=nullptr;
    std::wstring textureFile;
    float roughness=0.82f;
    float metallic=0.0f;
};
struct SkinVertex {Vertex base;std::uint8_t joints[4];float weights[4];};
struct LodChain {
    std::array<const Mesh*,4> meshes{};
    std::array<float,4> minPixels{};
};
const LodChain* lodChain(const std::string& name);
// Shadow-map texel diameter, independent of the camera-selected mesh LOD.
const Mesh* shadowLodMesh(const Mesh* source,float texels);
const std::vector<std::string>& assetIssues();
unsigned chooseLodLevel(float pixels,const std::array<float,4>& minPixels,
                        bool hasPrevious,unsigned previousLevel);
struct SkinClip {std::string name;float duration=1;std::vector<std::array<float,16>> palettes;
    std::vector<std::array<float,3>> rightHands;unsigned frames=0;};
struct SkinMesh {std::vector<SkinVertex> vertices;std::vector<SkinClip> clips;
    std::vector<std::uint8_t> bodyPartForJoint;unsigned jointCount=0;};
constexpr unsigned MAX_GPU_SKIN_JOINTS=256;
struct SkinDeformation {
    // Motion kind, phase sine/cosine, rising speed; mesh height and aim pitch.
    std::array<float,4> motion{},parameters{};
    std::array<std::array<float,4>,6> bodyPosition{},bodyRest{},bodyRotation{};
    std::array<float,4> attachmentOrigin{},attachmentRotation{0,0,0,1};
};
struct SkinInstance {
    const SkinMesh* source=nullptr;
    std::uint64_t identity=0;
    std::vector<std::array<float,16>> palette;
    // Scale, origin in the source mesh, yaw and world translation.
    std::array<float,4> scale{},origin{},transform{};
    std::array<float,4> yaw{};
    SkinDeformation deformation{};
};
void deformSkinCpu(const SkinInstance& instance,std::vector<Vertex>& output);
struct ModelInstance {
    const Mesh* source;
    int material;
    float scaleX,scaleY,scaleZ,cosYaw,sinYaw;
    float x,y,z,centerX,minY,centerZ;
    float r,g,b;
    float sinPitch=0,cosPitch=1;
    // Rigid asset orientation, applied before the legacy pitch/yaw transform.
    float qx=0,qy=0,qz=0,qw=1;
};
struct BoundingSphere {float x,y,z,radius;};
bool chooseDetailedLod(float pixels,float distance,float threshold,float cap,
                       bool hasPrevious,bool previousDetailed);
BoundingSphere instanceBounds(const ModelInstance& instance);
void sortInstancesForRendering(std::vector<ModelInstance>& instances,
                               float eyeX,float eyeY,float eyeZ);
void loadMeshes(const std::wstring& folder);
const Mesh* mesh(const std::string& name);
std::vector<const Mesh*> regionalMeshes();
const SkinMesh* skinMesh(const std::string& name);
void buildScene(std::vector<Vertex> groups[MATERIAL_GROUPS],std::vector<ModelInstance>& instances);
void buildScene(std::vector<Vertex> groups[MATERIAL_GROUPS],std::vector<ModelInstance>& instances,
                float cameraX,float cameraY,float cameraZ,
                std::vector<SkinInstance>* gpuSkins=nullptr,bool staticOnly=false,cpu::Pool* jobs=nullptr,
                bool extendedGpuSkins=false);
struct SceneWorkStats {std::size_t skinVertices=0;unsigned jobBatches=0;double skinMs=0,grassMs=0;};
const SceneWorkStats& sceneWorkStats();
void buildStaticScene(std::vector<Vertex> groups[MATERIAL_GROUPS]);
void buildHud(unsigned char* pixels,int width,int height);
void shutdownHud();
}
