#include "surface_work.h"
#include "terrain.h"
#include "excavation.h"
#include "regions.h"
#include "data_file.h"
#include "dx11_damage.h"
#include <cmath>
#include <algorithm>

namespace surface_work {
namespace {
using builder::Cell;using game::Vec3;using dx11::Vertex;
std::set<Cell> soil,cleared;std::uint64_t generation=1;std::string error;
int modulo=24;float maximumSlope=.8f;
struct Reward {int item,weight;};std::vector<Reward> rewards;int totalWeight=0;
struct Patch {dx11::Mesh mesh;std::uint64_t version=0,terrain=0,excavation=0;};std::map<Cell,Patch> meshes;
unsigned hash(Cell cell){unsigned value=unsigned(cell.x)*73856093u^unsigned(cell.z)*19349663u^0x641dc4e1u;value^=value>>16;value*=0x7feb352du;value^=value>>15;return value;}
Cell top(int x,int z){float height=terrain::baseHeight({x*40.0f+20,z*40.0f+20});return {x,int(std::floor((height-.01f)/40)),z};}
bool surface(Cell cell){if(cell.x<0||cell.z<0||cell.x>=420||cell.z>=420||cell.y<-10||cell.y>=75||!(cell==top(cell.x,cell.z)))return false;
    game::Vec2 center{cell.x*40.0f+20,cell.z*40.0f+20};
    return !regions::waterAt(center)&&!regions::roadAt(center)&&terrain::normal(center).y>=maximumSlope;
}
dx11::ModelInstance instance(const Deposit& d,const dx11::Mesh* source){return {source,2,16/std::max(.01f,source->maxX-source->minX),3/std::max(.01f,source->maxY-source->minY),16/std::max(.01f,source->maxZ-source->minZ),
    std::cos(d.yaw),std::sin(d.yaw),d.position.x,d.position.y,d.position.z,(source->minX+source->maxX)*.5f,source->minY,(source->minZ+source->maxZ)*.5f,1,1,1};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
bool ray(Vec3 origin,Vec3 direction,const Vertex& a,const Vertex& b,const Vertex& c,float limit,float& distance){
    Vec3 pa{a.x,a.y,a.z},edge1=Vec3{b.x,b.y,b.z}-pa,edge2=Vec3{c.x,c.y,c.z}-pa,p=cross(direction,edge2);float determinant=dot(edge1,p);
    if(std::abs(determinant)<.000001f)return false;auto delta=origin-pa;float u=dot(delta,p)/determinant;if(u<0||u>1)return false;
    auto q=cross(delta,edge1);float v=dot(direction,q)/determinant;if(v<0||u+v>1)return false;distance=dot(edge2,q)/determinant;return distance>.001f&&distance<limit;
}
Vertex mix(Vertex a,Vertex b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.nx+(b.nx-a.nx)*t,a.ny+(b.ny-a.ny)*t,a.nz+(b.nz-a.nz)*t,0,0,1,1,1,1};}
template<class F> std::vector<Vertex> clip(const std::vector<Vertex>& polygon,F distance){std::vector<Vertex> result;if(polygon.empty())return result;
    auto previous=polygon.back();float before=distance(previous);for(auto current:polygon){float after=distance(current);
        if((before>=0)!=(after>=0))result.push_back(mix(previous,current,before/(before-after)));if(after>=0)result.push_back(current);previous=current;before=after;}return result;}
void prepare(Cell cell,Patch& patch){if(patch.version==generation&&patch.terrain==terrain::revision()&&patch.excavation==excavation::revision())return;
    auto revision=patch.mesh.revision+1;patch.mesh={};patch.mesh.revision=revision;patch.mesh.textured=true;patch.mesh.allowTessellation=false;patch.mesh.temporalStable=false;patch.mesh.wrapTextures=true;
    if(auto* source=dx11::mesh("builder/tilled-soil"))patch.mesh.textureFile=source->textureFile;
    auto low=builder::cellLow(cell);patch.mesh.minX=low.x;patch.mesh.maxX=low.x+40;patch.mesh.minZ=low.z;patch.mesh.maxZ=low.z+40;patch.mesh.minY=-400;patch.mesh.maxY=3000;
    auto vertex=[](float x,float z){auto n=terrain::normal({x,z});return Vertex{x,terrain::baseHeight({x,z})+.06f,z,n.x,n.y,n.z,0,0,1,1,1,1};};
    for(int z=int(low.z/50);z<=int((low.z+39.99f)/50);++z)for(int x=int(low.x/50);x<=int((low.x+39.99f)/50);++x){auto a=vertex(x*50.0f,z*50.0f),b=vertex((x+1)*50.0f,z*50.0f),c=vertex((x+1)*50.0f,(z+1)*50.0f),d=vertex(x*50.0f,(z+1)*50.0f);
        for(const auto& triangle:{std::array<Vertex,3>{a,d,c},std::array<Vertex,3>{a,c,b}}){std::vector<Vertex> polygon{triangle.begin(),triangle.end()};
            polygon=clip(polygon,[&](Vertex v){return v.x-low.x;});polygon=clip(polygon,[&](Vertex v){return low.x+40-v.x;});
            polygon=clip(polygon,[&](Vertex v){return v.z-low.z;});polygon=clip(polygon,[&](Vertex v){return low.z+40-v.z;});
            for(auto& v:polygon){v.u=(v.x-low.x)/40;v.v=(v.z-low.z)/40;}
            for(std::size_t n=1;n+1<polygon.size();++n)excavation::clipTriangle({polygon[0],polygon[n],polygon[n+1]},patch.mesh.vertices);
        }}patch.version=generation;patch.terrain=terrain::revision();patch.excavation=excavation::revision();
}
}
bool load(const char* path){data_file::Ini file;int count=0,parsedModulo=0;float slope=0;error.clear();
    if(!file.load(path?path:data_file::resourcePath("builder-actions.ini"))||!file.version(1)||!file.integer("Surface","DepositModulo",parsedModulo,2,1000)||
        !file.real("Surface","MinimumUpNormal",slope,.7f,1)||!file.integer("Rewards","Count",count,1,16)){error=file.lastError();return false;}
    std::vector<Reward> parsed;int total=0;
    for(int n=0;n<count;++n){std::string id;int weight=0;auto section="Reward"+std::to_string(n);
        if(!file.string(section,"Item",id)||!file.integer(section,"Weight",weight,1,1000)){error=file.lastError();return false;}
        int item=builder::itemIndex(id);if(item<0||builder::items()[item].tool!=builder::Tool::None){error="Invalid surface reward";return false;}parsed.push_back({item,weight});total+=weight;}
    rewards=std::move(parsed);totalWeight=total;modulo=parsedModulo;maximumSlope=slope;return true;
}
const std::string& lastError(){return error;}
void clear(){soil.clear();cleared.clear();++generation;}
const std::set<Cell>& tilledCells(){return soil;}
const std::set<Cell>& brushedCells(){return cleared;}
void restore(std::set<Cell> tilled,std::set<Cell> brushed){soil=std::move(tilled);cleared=std::move(brushed);++generation;}
bool validSoil(Cell cell){if(!surface(cell))return false;int material=excavation::material(cell);return material>=0&&builder::items()[material].id=="soil";}
bool validDeposit(Cell cell){if(!surface(cell)||hash(cell)%unsigned(modulo)!=0||totalWeight==0)return false;
    int material=excavation::material(cell);if(material<0)return false;const auto& id=builder::items()[material].id;return id=="soil"||id=="sand"||id=="gravel";
}
bool deposit(Cell cell,Deposit& result){if(!builder::active()||!validDeposit(cell)||cleared.count(cell)||soil.count(cell)||excavation::removed(cell))return false;
    auto low=builder::cellLow(cell);Vec3 position{low.x+20,terrain::baseHeight({low.x+20,low.z+20}),low.z+20};
    if(builder::contains(position+Vec3{0,1,0}))return false;
    for(const auto& b:game::buildings)if(position.x>b.x&&position.x<b.x+b.w&&position.z>b.z&&position.z<b.z+b.d&&position.y<b.h)return false;
    unsigned value=(hash(cell)>>8)%unsigned(totalWeight);int resource=-1;for(const auto& reward:rewards){if(value<unsigned(reward.weight)){resource=reward.item;break;}value-=reward.weight;}
    result={cell,position,float(hash(cell)%628)/100,resource};return resource>=0;
}
std::vector<Deposit> nearby(game::Vec2 focus,float radius){std::vector<Deposit> result;if(!builder::active())return result;Deposit d;
    int left=std::max(0,int(std::floor((focus.x-radius)/40))),right=std::min(419,int((focus.x+radius)/40));
    int topZ=std::max(0,int(std::floor((focus.z-radius)/40))),bottom=std::min(419,int((focus.z+radius)/40));
    for(int z=topZ;z<=bottom;++z)for(int x=left;x<=right;++x)if(deposit(top(x,z),d)&&game::len(game::Vec2{d.position.x,d.position.z}-focus)<=radius+12)result.push_back(d);return result;
}
bool tilled(Cell cell){return builder::active()&&soil.count(cell)&&!excavation::removed(cell);}
bool till(const builder::Target& target){if(!builder::active()||target.source!=builder::Source::Ground||target.normal.y<.7f||!validSoil(target.cell)||excavation::removed(target.cell)||soil.count(target.cell)||soil.size()>=16384)return false;
    auto low=builder::cellLow(target.cell);auto center=Vec3{low.x+20,terrain::baseHeight({low.x+20,low.z+20})+1,low.z+20};if(builder::contains(center))return false;
    for(const auto& b:game::buildings)if(center.x>b.x&&center.x<b.x+b.w&&center.z>b.z&&center.z<b.z+b.d&&center.y<b.h)return false;
    soil.insert(target.cell);++generation;return true;
}
bool brush(Cell cell,int& resource){Deposit d;if(cleared.size()>=16384||!deposit(cell,d)||!cleared.insert(cell).second)return false;resource=d.resource;++generation;return true;}
void removed(Cell cell){if(soil.erase(cell))++generation;}
bool trace(Vec3 origin,Vec3 direction,float range,builder::Target& target){if(!builder::active())return false;auto* source=dx11::mesh("builder/surface-deposit");if(!source)return false;direction=game::norm(direction);bool changed=false;
    for(const auto& deposit:nearby({origin.x,origin.z},range)){auto mesh=dx11::clipBuildingMesh(instance(deposit,source),{});
        for(std::size_t n=0;n+2<mesh.vertices.size();n+=3){float distance=0;auto a=mesh.vertices[n],b=mesh.vertices[n+1],c=mesh.vertices[n+2];
            if(!ray(origin,direction,a,b,c,target.distance,distance))continue;Vec3 normal=game::norm(Vec3{a.nx,a.ny,a.nz});if(dot(normal,direction)>0)normal=normal*-1;
            target.source=builder::Source::Deposit;target.cell=deposit.cell;target.item=builder::itemIndex("surface-deposit");target.point=origin+direction*distance;target.normal=normal;
            target.adjacent=builder::cellAt(target.point+normal*.02f);target.distance=distance;target.index=-1;target.objectId.clear();changed=true;
        }}return changed;
}
void append(std::vector<dx11::ModelInstance>& instances,float radius){if(!builder::active())return;radius=std::min(radius,600.0f);
    if(auto* source=dx11::mesh("builder/surface-deposit"))for(const auto& deposit:nearby(game::player,radius))instances.push_back(instance(deposit,source));
    for(auto cell:soil){if(!tilled(cell))continue;auto low=builder::cellLow(cell);if(game::len(game::Vec2{low.x+20,low.z+20}-game::player)>radius)continue;
        auto& patch=meshes[cell];prepare(cell,patch);if(!patch.mesh.vertices.empty())instances.push_back({&patch.mesh,2,1,1,1,1,0,0,0,0,0,0,0,1,1,1});}
}
}
