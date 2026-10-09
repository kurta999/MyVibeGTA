#include "excavation.h"
#include "terrain.h"
#include "regions.h"
#include <algorithm>
#include <cmath>

namespace excavation {
namespace {
using game::Vec3;using dx11::Vertex;using Polygon=std::vector<Vertex>;
std::set<builder::Cell> cuts;
std::map<Patch,std::uint64_t> changed;
std::uint64_t generation=1,maskGeneration=1;
Patch patchOf(builder::Cell c){return {c.x/int(PATCH_SIZE/builder::BLOCK_SIZE),c.z/int(PATCH_SIZE/builder::BLOCK_SIZE)};}
Vec3 position(const Vertex& v){return {v.x,v.y,v.z};}
Vertex mix(const Vertex& a,const Vertex& b,float t){
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,
        a.nx+(b.nx-a.nx)*t,a.ny+(b.ny-a.ny)*t,a.nz+(b.nz-a.nz)*t,
        a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t,a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t,a.a+(b.a-a.a)*t};
}
template<class F> Polygon clip(const Polygon& input,F distance){
    Polygon output;if(input.empty())return output;
    auto previous=input.back();float before=distance(previous);
    for(const auto& current:input){float after=distance(current);
        if((before>=0)!=(after>=0))output.push_back(mix(previous,current,before/(before-after)));
        if(after>=0)output.push_back(current);previous=current;before=after;
    }return output;
}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
void triangles(const Polygon& polygon,std::vector<Vertex>& out){
    for(std::size_t i=1;i+1<polygon.size();++i)
        if(game::len(cross(position(polygon[i])-position(polygon[0]),position(polygon[i+1])-position(polygon[0])))>.00001f)
            for(const auto& v:{polygon[0],polygon[i],polygon[i+1]})out.push_back(v);
}
std::vector<Polygon> subtract(const Polygon& original,builder::Cell cell){
    auto low=builder::cellLow(cell),high=low+Vec3{builder::BLOCK_SIZE,(builder::BLOCK_SIZE+.5f),builder::BLOCK_SIZE};
    Polygon remaining=original;std::vector<Polygon> outside;
    for(int plane=0;plane<6&&!remaining.empty();++plane){
        auto distance=[&](const Vertex& v){const float p[]={v.x,v.y,v.z},lo[]={low.x,low.y,low.z},hi[]={high.x,high.y,high.z};
            int axis=plane/2;return plane%2?hi[axis]-p[axis]:p[axis]-lo[axis];};
        auto part=clip(remaining,[&](const Vertex& v){return -distance(v);});
        if(part.size()>=3)outside.push_back(std::move(part));remaining=clip(remaining,distance);
    }return outside;
}
std::vector<Patch> nodeOwners(int x,int z){
    std::vector<Patch> result;float px=x*terrain::spacing,pz=z*terrain::spacing;
    int cx=int(px/PATCH_SIZE),cz=int(pz/PATCH_SIZE);
    for(int a=cx-1;a<=cx;++a)for(int b=cz-1;b<=cz;++b){Patch p{a,b};
        if(changed.count(p)&&px>=a*PATCH_SIZE&&px<=(a+1)*PATCH_SIZE&&pz>=b*PATCH_SIZE&&pz<=(b+1)*PATCH_SIZE)result.push_back(p);}
    return result;
}
Vertex groundVertex(float x,float z,bool water=false){
    float y=water&&regions::waterAt({x,z})?-15:terrain::baseHeight({x,z});
    auto n=terrain::normal({x,z});return {x,y,z,n.x,n.y,n.z,x/70,z/70,1,1,1,1};
}
void belowSurface(const Polygon& face,Vec3 outward,std::vector<Vertex>& output){
    float minX=face[0].x,maxX=minX,minZ=face[0].z,maxZ=minZ;
    for(const auto& v:face){minX=std::min(minX,v.x);maxX=std::max(maxX,v.x);minZ=std::min(minZ,v.z);maxZ=std::max(maxZ,v.z);}
    int left=int(std::floor((minX+.0001f)/50)),right=int(std::floor((maxX-.0001f)/50));
    int top=int(std::floor((minZ+.0001f)/50)),bottom=int(std::floor((maxZ-.0001f)/50));
    if(right<left)right=left;if(bottom<top)bottom=top;
    for(int z=top;z<=bottom;++z)for(int x=left;x<=right;++x){
        auto a=groundVertex(x*50.0f,z*50.0f),b=groundVertex((x+1)*50.0f,z*50.0f);
        auto c=groundVertex((x+1)*50.0f,(z+1)*50.0f),d=groundVertex(x*50.0f,(z+1)*50.0f);
        for(const auto& triangle:{std::array<Vertex,3>{a,b,c},std::array<Vertex,3>{a,c,d}}){
            Polygon part=face;
            for(int i=0;i<3&&!part.empty();++i){auto p=triangle[i],q=triangle[(i+1)%3];
                part=clip(part,[&](const Vertex& v){return (q.x-p.x)*(v.z-p.z)-(q.z-p.z)*(v.x-p.x);});}
            auto n=cross(position(triangle[1])-position(triangle[0]),position(triangle[2])-position(triangle[0]));
            if(std::abs(n.y)<.0001f)continue;
            part=clip(part,[&](const Vertex& v){float px=v.x+outward.x*.01f,pz=v.z+outward.z*.01f;
                float h=triangle[0].y-(n.x*(px-triangle[0].x)+n.z*(pz-triangle[0].z))/n.y;
                return h-v.y-outward.y*.01f;});
            triangles(part,output);
        }
    }
}
}
void clear(){cuts.clear();changed.clear();++generation;++maskGeneration;}
bool validCell(builder::Cell cell){
    if(cell.x<0||cell.z<0||cell.x>=int(regions::WIDTH/builder::BLOCK_SIZE)||cell.z>=int(regions::DEPTH/builder::BLOCK_SIZE)||cell.y<int(-400/builder::BLOCK_SIZE)||cell.y>=int(3000/builder::BLOCK_SIZE))return false;
    auto lo=builder::cellLow(cell);game::Vec2 center{lo.x+builder::BLOCK_SIZE*.5f,lo.z+builder::BLOCK_SIZE*.5f};
    if(regions::waterAt(center))return false;
    float highest=-10000;
    for(float x:{lo.x+.01f,lo.x+builder::BLOCK_SIZE*.5f,lo.x+(builder::BLOCK_SIZE-.01f)})for(float z:{lo.z+.01f,lo.z+builder::BLOCK_SIZE*.5f,lo.z+(builder::BLOCK_SIZE-.01f)}){
        if(regions::waterAt({x,z}))return false;highest=std::max(highest,terrain::baseHeight({x,z}));}
    return highest>lo.y+.001f;
}
bool cut(builder::Cell cell){
    if(!builder::active()||cuts.size()>=131072||!validCell(cell)||!cuts.insert(cell).second)return false;
    auto p=patchOf(cell);if(!changed.count(p)){changed[p]=0;++maskGeneration;}
    ++generation;
    for(int x=p.x-1;x<=p.x+1;++x)for(int z=p.z-1;z<=p.z+1;++z){auto found=changed.find({x,z});if(found!=changed.end())found->second=generation;}
    return true;
}
void restore(const std::set<builder::Cell>& cells){clear();cuts=cells;for(auto cell:cuts)changed[patchOf(cell)]=generation;}
const std::set<builder::Cell>& cells(){return cuts;}
const std::map<Patch,std::uint64_t>& patches(){return changed;}
std::uint64_t revision(){return generation*2+(builder::active()?1:0);}
std::uint64_t maskRevision(){return maskGeneration*2+(builder::active()?1:0);}
bool removed(builder::Cell cell){return builder::active()&&cuts.count(cell);}
bool solid(Vec3 p){return p.y<=terrain::baseHeight({p.x,p.z})&&!removed(builder::cellAt(p-Vec3{0,.001f,0}));}
float floorBelow(Vec3 p){
    float baseline=terrain::baseHeight({p.x,p.z});if(!builder::active()||cuts.empty())return baseline;
    float candidate=p.y<baseline-.001f?p.y+.001f:baseline-.001f;auto cell=builder::cellAt({p.x,candidate,p.z});
    if(!removed(cell))return baseline;
    do {candidate=cell.y*builder::BLOCK_SIZE;--cell.y;}while(removed(cell));return candidate;
}
Vec3 surfaceNormal(Vec3 p,Vec3 direction){
    if(builder::active()&&!cuts.empty()){
        Vec3 best{};float weight=-1;
        for(Vec3 axis:{Vec3{1,0,0},Vec3{0,1,0},Vec3{0,0,1}}){
            bool plus=solid(p+axis*.05f),minus=solid(p-axis*.05f);
            float alignment=std::abs(axis.x*direction.x+axis.y*direction.y+axis.z*direction.z);
            if(plus!=minus&&alignment>weight){best=axis*(plus?-1.0f:1.0f);weight=alignment;}
        }if(weight>=0)return best;
    }return terrain::normal({p.x,p.z});
}
int material(builder::Cell cell){
    auto center=builder::cellLow(cell)+Vec3{builder::BLOCK_SIZE*.5f,builder::BLOCK_SIZE*.5f,builder::BLOCK_SIZE*.5f};float depth=terrain::baseHeight({center.x,center.z})-center.y;
    if(depth<45){auto biome=regions::biomeAt({center.x,center.z});return builder::itemIndex(
        biome==regions::Biome::Snow?"snow":biome==regions::Biome::Desert?"sand":"soil");}
    unsigned hash=unsigned(cell.x)*73856093u^unsigned(cell.z)*19349663u^unsigned(cell.y)*83492791u;
    if(depth>180&&hash%127==0)return builder::itemIndex("diamond-ore");
    if(depth>100&&hash%67==0)return builder::itemIndex("gold-ore");
    if(depth>55&&hash%43==0)return builder::itemIndex("iron-ore");
    if(hash%17==0)return builder::itemIndex("coal-ore");
    const char* rocks[]={"chalk","mudstone","shale","tuff","pumice","sandstone","limestone","travertine","dolostone","conglomerate","slate","marble","schist","gneiss","andesite","granite","diorite","gabbro","basalt","quartzite"};
    unsigned formation=unsigned(cell.x/12)*73856093u^unsigned(cell.z/12)*19349663u^unsigned((cell.y+10)/4)*83492791u;
    return builder::itemIndex(rocks[formation%20]);
}
bool maskedNode(int x,int z){return builder::active()&&!nodeOwners(x,z).empty();}
bool converted(Patch p){return builder::active()&&changed.count(p);}
void clipTriangle(const std::array<Vertex,3>& triangle,std::vector<Vertex>& output){
    if(!builder::active()||cuts.empty()){output.insert(output.end(),triangle.begin(),triangle.end());return;}
    float minX=triangle[0].x,maxX=minX,minY=triangle[0].y,maxY=minY,minZ=triangle[0].z,maxZ=minZ;
    for(const auto& v:triangle){minX=std::min(minX,v.x);maxX=std::max(maxX,v.x);minY=std::min(minY,v.y);maxY=std::max(maxY,v.y);minZ=std::min(minZ,v.z);maxZ=std::max(maxZ,v.z);}
    std::vector<Polygon> pieces{Polygon(triangle.begin(),triangle.end())};
    for(int x=int(std::floor(minX/builder::BLOCK_SIZE));x<=int(std::floor((maxX-.0001f)/builder::BLOCK_SIZE));++x)
        for(int z=int(std::floor(minZ/builder::BLOCK_SIZE));z<=int(std::floor((maxZ-.0001f)/builder::BLOCK_SIZE));++z)
            for(int y=int(std::floor((minY-.5f)/builder::BLOCK_SIZE));y<=int(std::floor(maxY/builder::BLOCK_SIZE));++y)if(cuts.count({x,y,z})){
                std::vector<Polygon> next;for(const auto& p:pieces){auto parts=subtract(p,{x,y,z});next.insert(next.end(),parts.begin(),parts.end());}pieces=std::move(next);}
    for(const auto& piece:pieces)triangles(piece,output);
}
std::vector<Face> faces(Patch patch){
    std::vector<Face> output;if(!builder::active())return output;
    for(auto cell:cuts){if(patchOf(cell).x!=patch.x||patchOf(cell).z!=patch.z)continue;
        auto low=builder::cellLow(cell),high=low+Vec3{builder::BLOCK_SIZE,builder::BLOCK_SIZE,builder::BLOCK_SIZE};
        for(int axis=0;axis<3;++axis)for(int sign:{-1,1}){
            builder::Cell adjacent=cell;Vec3 outward{};
            if(axis==0){adjacent.x+=sign;outward.x=float(sign);}else if(axis==1){adjacent.y+=sign;outward.y=float(sign);}else{adjacent.z+=sign;outward.z=float(sign);}
            if(removed(adjacent))continue;
            int u=(axis+1)%3,v=(axis+2)%3;float lo[]={low.x,low.y,low.z},hi[]={high.x,high.y,high.z};
            Polygon face;for(auto corner:{std::array<int,2>{0,0},{1,0},{1,1},{0,1}}){
                float p[]={low.x,low.y,low.z};p[axis]=sign<0?lo[axis]:hi[axis];p[u]=corner[0]?hi[u]:lo[u];p[v]=corner[1]?hi[v]:lo[v];
                face.push_back({p[0],p[1],p[2],-outward.x,-outward.y,-outward.z,float(corner[0]),float(corner[1]),1,1,1,1});}
            auto n=cross(position(face[1])-position(face[0]),position(face[2])-position(face[0]));
            if(n.x*outward.x+n.y*outward.y+n.z*outward.z>0)std::reverse(face.begin(),face.end());
            std::vector<Vertex> vertices;belowSurface(face,outward,vertices);int item=material(adjacent);
            for(std::size_t i=0;i<vertices.size();i+=3)output.push_back({item,{vertices[i],vertices[i+1],vertices[i+2]}});
        }
    }return output;
}
std::vector<Vertex> collisionTriangles(Patch patch){
    std::vector<Vertex> output;if(!builder::active())return output;
    for(int z=std::max(0,patch.z*4-1);z<=std::min(int(terrain::samples)-2,patch.z*4+4);++z)
        for(int x=std::max(0,patch.x*4-1);x<=std::min(int(terrain::samples)-2,patch.x*4+4);++x){
            std::array<std::array<std::array<int,2>,3>,2> indices{{{{{x,z},{x,z+1},{x+1,z+1}}},{{{x,z},{x+1,z+1},{x+1,z}}}}};
            for(const auto& triangle:indices){std::set<Patch> owners;
                for(auto node:triangle){auto candidates=nodeOwners(node[0],node[1]);owners.insert(candidates.begin(),candidates.end());}
                if(owners.empty()||owners.begin()->x!=patch.x||owners.begin()->z!=patch.z)continue;
                std::array<Vertex,3> vertices;for(int i=0;i<3;++i)vertices[i]=groundVertex(triangle[i][0]*50.0f,triangle[i][1]*50.0f,true);
                clipTriangle(vertices,output);
            }
        }
    for(const auto& face:faces(patch))output.insert(output.end(),face.vertices.begin(),face.vertices.end());return output;
}
}
