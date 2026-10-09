#include "scenery_edits.h"
#include "dx11_damage.h"
#include "regions.h"
#include "terrain.h"
#include "jolt_world.h"
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>
#include <cstdio>

namespace scenery_edits {
namespace {
using game::Vec3;using dx11::Vertex;using Polygon=std::vector<Vertex>;
Records cuts;std::map<std::string,std::uint64_t> versions;std::uint64_t generation=1,baselineGeneration=1;std::size_t cutCount=0;
std::map<std::string,std::size_t> treeIndices,decorationIndices;
std::size_t indexedTrees=~std::size_t(0),indexedDecorations=~std::size_t(0);float crownRadius=100;
void indexObjects(){
    treeIndices.clear();crownRadius=100;
    for(std::size_t n=0;n<game::trees.size();++n){treeIndices[treeId(n)]=n;
        crownRadius=std::max(crownRadius,std::max(72.0f,game::trees[n].crownWidth)*game::trees[n].scale);}
    decorationIndices.clear();for(std::size_t n=0;n<regions::decorations().size();++n)decorationIndices["decoration:"+regions::decorations()[n].id]=n;
    indexedTrees=game::trees.size();indexedDecorations=regions::decorations().size();
}
struct Geometry {dx11::Mesh original,solid,exterior;std::map<int,dx11::Mesh> caps;std::uint64_t version=0;const dx11::Mesh* source=nullptr;Object object;};
// Stable addresses survive mode switches and edit revisions in the renderer's GPU cache.
std::map<std::string,Geometry> geometries;
Vec3 point(const Vertex& v){return {v.x,v.y,v.z};}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float coord(Vec3 p,int axis){return axis==0?p.x:axis==1?p.y:p.z;}
float coord(const Vertex& v,int axis){return coord(point(v),axis);}
Vertex mix(Vertex a,Vertex b,float t){
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,
        a.nx+(b.nx-a.nx)*t,a.ny+(b.ny-a.ny)*t,a.nz+(b.nz-a.nz)*t,
        a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t,a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t,a.a+(b.a-a.a)*t};
}
template<class F> Polygon clip(const Polygon& input,F distance){
    Polygon output;if(input.empty())return output;Vertex previous=input.back();float before=distance(previous);
    for(auto current:input){float after=distance(current);if((before>=0)!=(after>=0))output.push_back(mix(previous,current,before/(before-after)));
        if(after>=0)output.push_back(current);previous=current;before=after;}return output;
}
Polygon inside(Polygon polygon,Vec3 low,Vec3 high){
    for(int axis=0;axis<3;++axis){polygon=clip(polygon,[&](const Vertex& v){return coord(v,axis)-coord(low,axis);});
        polygon=clip(polygon,[&](const Vertex& v){return coord(high,axis)-coord(v,axis);});}return polygon;
}
void triangles(const Polygon& polygon,std::vector<Vertex>& output,Vec3 normal={}){
    for(std::size_t n=1;n+1<polygon.size();++n){auto a=polygon[0],b=polygon[n],c=polygon[n+1];auto area=cross(point(b)-point(a),point(c)-point(a));
        if(game::len(area)<.00001f)continue;if(dot(area,normal)<0)std::swap(b,c);output.insert(output.end(),{a,b,c});}
}
bool same(const Object& a,const Object& b){return a.model==b.model&&a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&
    a.size.x==b.size.x&&a.size.y==b.size.y&&a.size.z==b.size.z&&a.yaw==b.yaw;}
dx11::ModelInstance instance(const Object& o,const dx11::Mesh* source){
    return {source,o.kind==Kind::Rock?0:4,o.size.x/std::max(.01f,source->maxX-source->minX),o.size.y/std::max(.01f,source->maxY-source->minY),
        o.size.z/std::max(.01f,source->maxZ-source->minZ),std::cos(o.yaw),std::sin(o.yaw),o.position.x,o.position.y,o.position.z,
        (source->minX+source->maxX)*.5f,source->minY,(source->minZ+source->maxZ)*.5f,o.tint.r,o.tint.g,o.tint.b};
}
struct ImageStore {
    ULONG_PTR token=0;std::map<std::wstring,std::unique_ptr<Gdiplus::Bitmap>> images;
    ImageStore(){Gdiplus::GdiplusStartupInput options;Gdiplus::GdiplusStartup(&token,&options,nullptr);}
    ~ImageStore(){images.clear();if(token)Gdiplus::GdiplusShutdown(token);}
};
game::Color colorAt(const dx11::Mesh& source,unsigned triangle,const Vertex& v,float& alpha){
    std::wstring file=source.textureFile;bool masked=source.alphaTest;float cutoff=.5f;
    for(const auto& material:source.materialRanges)if(triangle>=material.start&&triangle<material.start+material.count){
        if(!material.baseFile.empty())file=material.baseFile;masked=material.alphaTest;cutoff=material.alphaCutoff;break;}
    game::Color color{v.r,v.g,v.b};alpha=v.a;
    if(!file.empty()){
        static ImageStore store;auto& bitmap=store.images[file];if(!bitmap&&store.token)bitmap=std::make_unique<Gdiplus::Bitmap>(file.c_str());
        if(bitmap&&bitmap->GetLastStatus()==Gdiplus::Ok&&bitmap->GetWidth()&&bitmap->GetHeight()){
            float u=source.wrapTextures?v.u-std::floor(v.u):std::clamp(v.u,0.0f,1.0f);
            float w=source.wrapTextures?v.v-std::floor(v.v):std::clamp(v.v,0.0f,1.0f);Gdiplus::Color pixel;
            bitmap->GetPixel(std::min(unsigned(u*bitmap->GetWidth()),bitmap->GetWidth()-1),std::min(unsigned(w*bitmap->GetHeight()),bitmap->GetHeight()-1),&pixel);
            color.r*=pixel.GetR()/255.0f;color.g*=pixel.GetG()/255.0f;color.b*=pixel.GetB()/255.0f;
            if(masked)alpha=pixel.GetA()/255.0f>=cutoff?alpha:0;
        }
    }return color;
}
int materialAt(const Object& o,const Vertex& v,game::Color color){
    if(o.kind==Kind::Plant)return builder::itemIndex("leaves");
    if(o.kind==Kind::Rock){std::vector<int> rocks;for(int n=0;n<int(builder::items().size());++n){const auto& item=builder::items()[n];
        if(item.craftGroup=="stone-material"&&item.id!="brick"&&item.id!="concrete")rocks.push_back(n);}
        unsigned hash=2166136261u;for(unsigned char c:o.id)hash=(hash^c)*16777619u;return rocks.empty()?-1:rocks[hash%rocks.size()];}
    if(color.g>color.r*1.12f&&color.g>color.b*1.1f)return builder::itemIndex("leaves");
    if(color.r>color.g*1.12f&&color.g>color.b*1.08f)return builder::itemIndex("log");
    float radius=o.index>=0?jolt_world::treeRadius(game::trees[o.index]):6;
    bool trunk=game::len(game::Vec2{v.x-o.position.x,v.z-o.position.z})<radius*1.5f&&v.y<o.position.y+o.size.y*.68f;
    return builder::itemIndex(trunk?"log":"leaves");
}
bool boundsRay(Vec3 origin,Vec3 direction,Vec3 low,Vec3 high,float range){
    float enter=0,leave=range;for(int a=0;a<3;++a){float p=coord(origin,a),d=coord(direction,a),lo=coord(low,a),hi=coord(high,a);
        if(std::abs(d)<.000001f){if(p<lo||p>hi)return false;}else{float first=(lo-p)/d,last=(hi-p)/d;if(first>last)std::swap(first,last);
            enter=std::max(enter,first);leave=std::min(leave,last);if(enter>leave)return false;}}return true;
}
bool rayTriangle(Vec3 origin,Vec3 direction,const Vertex& a,const Vertex& b,const Vertex& c,float limit,float& t,float& u,float& v){
    auto edge1=point(b)-point(a),edge2=point(c)-point(a),p=cross(direction,edge2);float determinant=dot(edge1,p);
    if(std::abs(determinant)<.000001f)return false;auto delta=origin-point(a);u=dot(delta,p)/determinant;
    if(u<-.00001f||u>1.00001f)return false;auto q=cross(delta,edge1);v=dot(direction,q)/determinant;
    if(v<-.00001f||u+v>1.00001f)return false;t=dot(edge2,q)/determinant;return t>.001f&&t<limit;
}
void earTriangles(Polygon polygon,int axis,Vec3 normal,std::vector<Vertex>& output){
    // Plane loops may be concave (branches or scanned rocks), so a triangle fan is insufficient.
    int a=(axis+1)%3,b=(axis+2)%3;
    auto turn=[&](const Vertex& p,const Vertex& q,const Vertex& r){return (coord(q,a)-coord(p,a))*(coord(r,b)-coord(p,b))-(coord(q,b)-coord(p,b))*(coord(r,a)-coord(p,a));};
    float area=0;for(std::size_t n=0;n<polygon.size();++n){auto p=polygon[n],q=polygon[(n+1)%polygon.size()];area+=coord(p,a)*coord(q,b)-coord(q,a)*coord(p,b);}
    if(area<0)std::reverse(polygon.begin(),polygon.end());
    while(polygon.size()>3){bool found=false;for(std::size_t n=0;n<polygon.size();++n){auto previous=(n+polygon.size()-1)%polygon.size(),next=(n+1)%polygon.size();
        float bend=turn(polygon[previous],polygon[n],polygon[next]);if(std::abs(bend)<.00001f){polygon.erase(polygon.begin()+n);found=true;break;}if(bend<0)continue;
        bool occupied=false;for(std::size_t k=0;k<polygon.size();++k)if(k!=previous&&k!=n&&k!=next&&turn(polygon[previous],polygon[n],polygon[k])>=-.00001f&&
            turn(polygon[n],polygon[next],polygon[k])>=-.00001f&&turn(polygon[next],polygon[previous],polygon[k])>=-.00001f){occupied=true;break;}
        if(occupied)continue;triangles({polygon[previous],polygon[n],polygon[next]},output,normal);polygon.erase(polygon.begin()+n);found=true;break;
    }if(!found)return;}if(polygon.size()==3)triangles(polygon,output,normal);
}
void makeCaps(Geometry& geometry,const std::vector<game::BuildingPiece>& volumes){
    const auto& source=geometry.solid.vertices.empty()?geometry.original:geometry.solid;struct Segment {Vertex a,b;int item;};
    for(std::size_t cut=0;cut<volumes.size();++cut)for(int axis=0;axis<3;++axis)for(int side=0;side<2;++side){
        float plane=coord(side?volumes[cut].high:volumes[cut].low,axis);Vec3 normal{};
        if(axis==0)normal.x=side?-1.0f:1.0f;else if(axis==1)normal.y=side?-1.0f:1.0f;else normal.z=side?-1.0f:1.0f;
        std::vector<Segment> segments;
        const auto count=source.indices.empty()?source.vertices.size():source.indices.size();
        auto vertex=[&](unsigned n)->const Vertex&{return source.vertices[source.indices.empty()?n:source.indices[n]];};
        for(unsigned n=0;n+2<count;n+=3){std::vector<Vertex> crossings;
            for(int edge=0;edge<3;++edge){auto a=vertex(n+edge),b=vertex(n+(edge+1)%3);float da=coord(a,axis)-plane,db=coord(b,axis)-plane;
                if((da<-.0001f&&db>.0001f)||(da>.0001f&&db<-.0001f))crossings.push_back(mix(a,b,da/(da-db)));
                else if(std::abs(da)<=.0001f&&std::abs(db)>.0001f)crossings.push_back(a);
            }
            if(crossings.size()!=2||game::len(point(crossings[0])-point(crossings[1]))<.0001f)continue;
            float alpha=1;auto sample=mix(crossings[0],crossings[1],.5f);auto color=colorAt(source,n,sample,alpha);
            segments.push_back({crossings[0],crossings[1],materialAt(geometry.object,sample,color)});
        }
        while(!segments.empty()){
            Segment first=segments.back();segments.pop_back();Polygon loop{first.a,first.b};int material=first.item;
            bool closed=false;while(loop.size()<4096){if(game::len(point(loop.back())-point(loop.front()))<.025f){loop.pop_back();closed=true;break;}
                auto next=std::find_if(segments.begin(),segments.end(),[&](const Segment& s){return game::len(point(s.a)-point(loop.back()))<.025f||game::len(point(s.b)-point(loop.back()))<.025f;});
                if(next==segments.end())break;loop.push_back(game::len(point(next->a)-point(loop.back()))<.025f?next->b:next->a);segments.erase(next);
            }
            if(!closed||loop.size()<3||material<0)continue;
            int a=(axis+1)%3,b=(axis+2)%3;
            for(auto& vertex:loop){if(axis==0)vertex.x=plane;else if(axis==1)vertex.y=plane;else vertex.z=plane;
                vertex.nx=normal.x;vertex.ny=normal.y;vertex.nz=normal.z;vertex.u=coord(vertex,a)/40;vertex.v=coord(vertex,b)/40;vertex.r=vertex.g=vertex.b=vertex.a=1;}
            // Triangulate first: clipping a concave loop can join disconnected branches.
            std::vector<Vertex> section;earTriangles(loop,axis,normal,section);dx11::Mesh planeMesh;
            for(std::size_t n=0;n+2<section.size();n+=3){Polygon polygon{section[n],section[n+1],section[n+2]};
                for(int coordinate:{a,b}){polygon=clip(polygon,[&](const Vertex& v){return coord(v,coordinate)-coord(volumes[cut].low,coordinate);});
                    polygon=clip(polygon,[&](const Vertex& v){return coord(volumes[cut].high,coordinate)-coord(v,coordinate);});}
                triangles(polygon,planeMesh.vertices,normal);}
            // A cap on the shared plane of two removed cells is internal.
            // Include that plane in the neighboring subtraction; otherwise
            // the zero-thickness cap survives the strict box overlap test.
            Vec3 pad{};if(axis==0)pad.x=.002f;else if(axis==1)pad.y=.002f;else pad.z=.002f;
            std::vector<game::BuildingPiece> others;for(std::size_t n=0;n<volumes.size();++n)if(n!=cut)others.push_back({volumes[n].low-pad,volumes[n].high+pad});
            auto clipped=dx11::clipBuildingMesh({&planeMesh,0,1,1,1,1,0,0,0,0,0,0,0,1,1,1},others);
            auto& mesh=geometry.caps[material];for(std::size_t n=0;n+2<clipped.vertices.size();n+=3)triangles({clipped.vertices[n],clipped.vertices[n+1],clipped.vertices[n+2]},mesh.vertices,normal);
        }
    }
}
Geometry* prepare(const Object& object){
    const auto* model=dx11::mesh(object.model);if(!model)return nullptr;auto& geometry=geometries[object.id];auto version=revision(object.id);
    if(geometry.source==model&&geometry.version==version&&same(geometry.object,object))return &geometry;
    geometry.source=model;geometry.object=object;geometry.original=dx11::clipBuildingMesh(instance(object,model),{});
    geometry.solid.vertices.clear();
    if(edited(object.id)&&object.kind==Kind::Tree)if(auto* solid=dx11::mesh(object.model+"-mining-solid")){
        auto placement=instance(object,model);geometry.solid=*solid;
        for(auto& vertex:geometry.solid.vertices){float x=(vertex.x-placement.centerX)*placement.scaleX,z=(vertex.z-placement.centerZ)*placement.scaleZ;
            vertex.x=placement.x+placement.cosYaw*x+placement.sinYaw*z;vertex.y=placement.y+(vertex.y-placement.minY)*placement.scaleY;vertex.z=placement.z-placement.sinYaw*x+placement.cosYaw*z;
            x=vertex.nx/placement.scaleX;float y=vertex.ny/placement.scaleY;z=vertex.nz/placement.scaleZ;float length=std::max(.0001f,std::sqrt(x*x+y*y+z*z));
            vertex.nx=(placement.cosYaw*x+placement.sinYaw*z)/length;vertex.ny=y/length;vertex.nz=(-placement.sinYaw*x+placement.cosYaw*z)/length;
        }
        geometry.solid.textureFile=model->textureFile;geometry.solid.textured=model->textured;
    }
    std::vector<game::BuildingPiece> volumes;if(builder::active()){auto found=cuts.find(object.id);if(found!=cuts.end())for(auto cell:found->second){auto low=builder::cellLow(cell);volumes.push_back({low,low+Vec3{builder::BLOCK_SIZE,builder::BLOCK_SIZE,builder::BLOCK_SIZE}});}}
    std::uint64_t meshRevision=geometry.exterior.revision+1;geometry.exterior=dx11::clipBuildingMesh(instance(object,model),volumes);geometry.exterior.revision=meshRevision;
    for(auto& group:geometry.caps){group.second.vertices.clear();++group.second.revision;}
    if(!volumes.empty())makeCaps(geometry,volumes);
    for(auto& group:geometry.caps){auto& mesh=group.second;const auto* material=dx11::mesh("builder/"+builder::items()[group.first].id);
        mesh.textured=true;mesh.wrapTextures=true;mesh.allowTessellation=false;mesh.temporalStable=false;mesh.roughness=.95f;if(material)mesh.textureFile=material->textureFile;
        if(!mesh.vertices.empty()){mesh.minX=mesh.maxX=mesh.vertices[0].x;mesh.minY=mesh.maxY=mesh.vertices[0].y;mesh.minZ=mesh.maxZ=mesh.vertices[0].z;
            for(const auto& v:mesh.vertices){mesh.minX=std::min(mesh.minX,v.x);mesh.maxX=std::max(mesh.maxX,v.x);mesh.minY=std::min(mesh.minY,v.y);mesh.maxY=std::max(mesh.maxY,v.y);mesh.minZ=std::min(mesh.minZ,v.z);mesh.maxZ=std::max(mesh.maxZ,v.z);}}
    }geometry.version=version;return &geometry;
}
bool meshIntersects(const dx11::Mesh& mesh,Vec3 low,Vec3 high,bool interior=true){
    if(mesh.maxX<low.x||mesh.minX>high.x||mesh.maxY<low.y||mesh.minY>high.y||mesh.maxZ<low.z||mesh.minZ>high.z)return false;
    for(std::size_t n=0;n+2<mesh.vertices.size();n+=3){auto polygon=inside({mesh.vertices[n],mesh.vertices[n+1],mesh.vertices[n+2]},low,high);
        if(polygon.size()>=3)for(std::size_t k=1;k+1<polygon.size();++k)if(game::len(cross(point(polygon[k])-point(polygon[0]),point(polygon[k+1])-point(polygon[0])))>.00001f)return true;}
    if(!interior)return false;
    // A placed block can sit completely inside a closed scanned rock.
    auto center=(low+high)*.5f;std::vector<float> hits;Vec3 ray=game::norm(Vec3{1,.173f,.319f});
    for(std::size_t n=0;n+2<mesh.vertices.size();n+=3){float t,u,v;if(rayTriangle(center,ray,mesh.vertices[n],mesh.vertices[n+1],mesh.vertices[n+2],5000,t,u,v))hits.push_back(t);}
    std::sort(hits.begin(),hits.end());int unique=0;float last=-1;for(float t:hits)if(std::abs(t-last)>.01f){++unique;last=t;}return unique%2!=0;
}
bool bushForTree(std::size_t index,Object& object){
    if(index>=game::trees.size())return false;const auto& tree=game::trees[index];
    if(tree.palm||!tree.modelId.empty()||regions::biomeAt(tree.p)!=regions::Biome::Countryside)return false;
    int variant=int(index%36);object={treeBushId(index),"nature/bush_"+std::string(variant<10?"0":"")+std::to_string(variant),Kind::Plant,int(index),
        {tree.p.x+18,terrain::baseHeight({tree.p.x+18,tree.p.z-15}),tree.p.z-15},{11,10,11},float(index)*.9f+.5f,game::rgb(185,217,176)};return true;
}
bool beachBush(int index,Object& object){
    if(index<0||index>=70||index%4)return false;float x=35+float((index*137)%2280),z=game::BEACH_START+35+float((index*67)%210);int variant=(index*13)%36;
    object={"beach-bush-"+std::to_string(index),"nature/bush_"+std::string(variant<10?"0":"")+std::to_string(variant),Kind::Plant,index,{x,0,z},{12,12,12},index*.76f};return true;
}
}
void clear(){cuts.clear();versions.clear();cutCount=0;baselineGeneration=++generation;indexedTrees=indexedDecorations=~std::size_t(0);}
const Records& records(){return cuts;}
void restore(Records records){clear();cuts=std::move(records);for(const auto& record:cuts){versions[record.first]=generation;cutCount+=record.second.size();}}
std::string treeId(std::size_t index){return index<game::trees.size()&&!game::trees[index].id.empty()?"tree:"+game::trees[index].id:"tree-index:"+std::to_string(index);}
std::string treeBushId(std::size_t index){return "tree-bush:"+treeId(index);}
bool treeObject(std::size_t index,Object& o){
    if(index>=game::trees.size())return false;const auto& tree=game::trees[index];o.id=treeId(index);o.index=int(index);o.kind=Kind::Tree;o.position={tree.p.x,terrain::baseHeight(tree.p),tree.p.z};
    if(!tree.modelId.empty()){o.model="nature/"+tree.modelId;o.size={tree.crownWidth*tree.scale,tree.height*tree.scale,tree.crownWidth*tree.scale};o.yaw=float(index)*.43f;}
    else if(tree.palm){o.model=tree.variant==0?"nature/tree_palmDetailedShort":"nature/tree_palmDetailedTall";o.size={72*tree.scale,(tree.variant==0?70.0f:86.0f)*tree.scale,72*tree.scale};o.yaw=float(index)*.43f;}
    else{o.model=tree.variant==0?"nature/tree_detailed":"nature/tree_oak";o.size={54*tree.scale,68*tree.scale,54*tree.scale};o.yaw=float(index)*.9f;}
    float wear=std::clamp(tree.health/100.0f,.25f,1.0f);o.tint={wear,wear,wear};return true;
}
bool outcropObject(int x,int z,Object& o){
    if(x<0||z<0||x>=42||z>=42)return false;unsigned hash=unsigned(x)*73856093u^unsigned(z)*19349663u;if(hash%3)return false;
    game::Vec2 p{(x+.2f+float(hash%50)/100)*400,(z+.2f+float((hash>>8)%50)/100)*400};
    if(regions::waterAt(p)||regions::roadAt(p))return false;float h=terrain::baseHeight(p);if(h<90||terrain::normal(p).y>.995f)return false;
    bool cliff=hash%4==0;float width=cliff?100+float(hash%60):22+float(hash%30),height=cliff?35:width*.65f;
    o={"outcrop:"+std::to_string(x)+":"+std::to_string(z),cliff?"nature/rock_coastal_cliff_02":"nature/rock_namaqualand_boulder_02",Kind::Rock,-1,
        {p.x,h-height*.22f,p.z},{width,height,width},float(hash%628)/100,regions::biomeAt(p)==regions::Biome::Desert?game::rgb(234,202,166):game::rgb(226,224,207)};return true;
}
bool find(const std::string& id,Object& object){
    if(id.rfind("outcrop:",0)==0){int x=-1,z=-1;char trailing=0;return std::sscanf(id.c_str(),"outcrop:%d:%d%c",&x,&z,&trailing)==2&&outcropObject(x,z,object)&&object.id==id;}
    if(id.rfind("beach-bush-",0)==0){int index=-1;char trailing=0;return std::sscanf(id.c_str(),"beach-bush-%d%c",&index,&trailing)==1&&beachBush(index,object)&&object.id==id;}
    if(indexedTrees!=game::trees.size()||indexedDecorations!=regions::decorations().size())indexObjects();
    bool bush=id.rfind("tree-bush:",0)==0;auto key=bush?id.substr(10):id;
    auto tree=treeIndices.find(key);if(tree!=treeIndices.end()&&treeId(tree->second)!=key){indexObjects();tree=treeIndices.find(key);}
    if(tree!=treeIndices.end())return bush?bushForTree(tree->second,object):treeObject(tree->second,object);
    auto found=decorationIndices.find(id);if(found!=decorationIndices.end()){
        auto n=found->second;const auto& prop=regions::decorations()[n];if("decoration:"+prop.id!=id){indexObjects();return find(id,object);}
        object={id,"nature/"+prop.modelId,prop.modelId.find("rock")!=std::string::npos?Kind::Rock:Kind::Plant,int(n),
            {prop.p.x,terrain::baseHeight(prop.p),prop.p.z},{prop.width,prop.height,prop.depth},float(n)*.73f};return true;}
    return false;
}
std::vector<Object> nearby(game::Vec2 focus,float radius){
    std::vector<Object> result;Object object;
    if(indexedTrees!=game::trees.size()||indexedDecorations!=regions::decorations().size())indexObjects();
    for(int index:regions::nearbyTreeIndices(focus,radius+crownRadius))if(index>=0&&std::size_t(index)<game::trees.size()&&!game::trees[index].destroyed){
        if(treeObject(index,object)&&game::len(game::Vec2{object.position.x,object.position.z}-focus)<radius+object.size.x)result.push_back(object);
        if(bushForTree(index,object))result.push_back(object);
    }
    for(int index:regions::nearbyDecorationIndices(focus,radius+100))if(index>=0&&std::size_t(index)<regions::decorations().size()){
        const auto& prop=regions::decorations()[index];result.push_back({"decoration:"+prop.id,"nature/"+prop.modelId,prop.modelId.find("rock")!=std::string::npos?Kind::Rock:Kind::Plant,index,
            {prop.p.x,terrain::baseHeight(prop.p),prop.p.z},{prop.width,prop.height,prop.depth},index*.73f});
    }
    int left=std::max(0,int((focus.x-radius-200)/400)),right=std::min(41,int((focus.x+radius+200)/400));
    int top=std::max(0,int((focus.z-radius-200)/400)),bottom=std::min(41,int((focus.z+radius+200)/400));
    for(int z=top;z<=bottom;++z)for(int x=left;x<=right;++x)if(outcropObject(x,z,object))result.push_back(object);
    for(int n=0;n<70;n+=4)if(beachBush(n,object)&&game::len(game::Vec2{object.position.x,object.position.z}-focus)<radius+12)result.push_back(object);
    return result;
}
bool edited(const std::string& id){auto found=cuts.find(id);return builder::active()&&found!=cuts.end()&&!found->second.empty();}
bool removed(const std::string& id,builder::Cell cell){auto found=cuts.find(id);return builder::active()&&found!=cuts.end()&&found->second.count(cell);}
std::uint64_t revision(const std::string& id){auto found=versions.find(id);return (found==versions.end()?baselineGeneration:found->second)*2+(builder::active()?1:0);}
bool validCut(const std::string& id,builder::Cell cell){
    if(cell.x<0||cell.z<0||cell.x>=int(regions::WIDTH/builder::BLOCK_SIZE)||cell.z>=int(regions::DEPTH/builder::BLOCK_SIZE)||cell.y<int(-400/builder::BLOCK_SIZE)||cell.y>=int(3000/builder::BLOCK_SIZE))return false;Object object;if(!find(id,object))return false;
    auto* geometry=prepare(object);if(!geometry)return false;auto low=builder::cellLow(cell);return meshIntersects(geometry->original,low+Vec3{.001f,.001f,.001f},low+Vec3{builder::BLOCK_SIZE-.001f,builder::BLOCK_SIZE-.001f,builder::BLOCK_SIZE-.001f});
}
bool cut(const std::string& id,builder::Cell cell){
    if(!builder::active()||cutCount>=131072||removed(id,cell)||!validCut(id,cell))return false;
    cuts[id].insert(cell);++cutCount;versions[id]=++generation;return true;
}
bool trace(Vec3 origin,Vec3 direction,float range,builder::Target& target,TraceMask mask){
    if(!builder::active())return false;bool changed=false;direction=game::norm(direction);
    auto publish=[&](const Object& object,Vertex sample,game::Color color,float t,int material){
        Vec3 normal=game::norm(Vec3{sample.nx,sample.ny,sample.nz});if(dot(normal,direction)>0)normal=normal*-1;
        auto hit=origin+direction*t;auto cell=builder::cellAt(hit-normal*.02f);if(removed(object.id,cell))return;
        target.source=object.kind==Kind::Tree?builder::Source::Tree:builder::Source::Scenery;target.objectId=object.id;target.index=object.index;
        target.point=hit;target.normal=normal;target.cell=cell;target.adjacent=builder::cellAt(hit+normal*.02f);target.item=material>=0?material:materialAt(object,sample,color);target.distance=t;changed=true;
    };
    auto check=[&](const Object& object,const dx11::Mesh& mesh,int material){
        if(!boundsRay(origin,direction,{mesh.minX,mesh.minY,mesh.minZ},{mesh.maxX,mesh.maxY,mesh.maxZ},target.distance))return;
        for(unsigned n=0;n+2<mesh.vertices.size();n+=3){const auto& a=mesh.vertices[n];const auto& b=mesh.vertices[n+1];const auto& c=mesh.vertices[n+2];float t,u,v;
            if(!rayTriangle(origin,direction,a,b,c,target.distance,t,u,v))continue;
            auto sample=mix(mix(a,b,u/(std::max(.000001f,1-v))),c,v);float alpha=1;auto color=colorAt(mesh,n,sample,alpha);if(alpha<.1f)continue;
            publish(object,sample,color,t,material);
        }
    };
    target.distance=std::min(target.distance,range);
    for(const auto& object:nearby({origin.x,origin.z},range)){
        if((mask==TraceMask::Trees&&object.kind!=Kind::Tree)||(mask==TraceMask::NonTrees&&object.kind==Kind::Tree))continue;
        float horizontal=(object.size.x+object.size.z)*.5f;
        if(!boundsRay(origin,direction,object.position-Vec3{horizontal,0,horizontal},object.position+Vec3{horizontal,object.size.y,horizontal},target.distance))continue;
        if(!edited(object.id)){
            // Ray-cast unchanged source geometry in model space; do not duplicate full meshes per tree.
            const auto* mesh=dx11::mesh(object.model);if(!mesh)continue;auto placement=instance(object,mesh);
            auto local=[&](Vec3 value,bool position){if(position)value=value-object.position;
                return Vec3{(placement.cosYaw*value.x-placement.sinYaw*value.z)/placement.scaleX+(position?placement.centerX:0),
                    value.y/placement.scaleY+(position?placement.minY:0),
                    (placement.sinYaw*value.x+placement.cosYaw*value.z)/placement.scaleZ+(position?placement.centerZ:0)};};
            auto start=local(origin,true),ray=local(direction,false);std::size_t count=mesh->indices.empty()?mesh->vertices.size():mesh->indices.size();
            if(!boundsRay(start,ray,{mesh->minX,mesh->minY,mesh->minZ},{mesh->maxX,mesh->maxY,mesh->maxZ},target.distance))continue;
            for(unsigned n=0;n+2<count;n+=3){auto vertex=[&](unsigned index)->const Vertex&{return mesh->vertices[mesh->indices.empty()?index:mesh->indices[index]];};
                auto a=vertex(n),b=vertex(n+1),c=vertex(n+2);float t,u,v;if(!rayTriangle(start,ray,a,b,c,target.distance,t,u,v))continue;
                auto sample=mix(mix(a,b,u/std::max(.000001f,1-v)),c,v);float alpha=1;auto color=colorAt(*mesh,n,sample,alpha);if(alpha<.1f)continue;
                Vec3 normal{sample.nx/placement.scaleX,sample.ny/placement.scaleY,sample.nz/placement.scaleZ};
                sample.nx=placement.cosYaw*normal.x+placement.sinYaw*normal.z;sample.ny=normal.y;sample.nz=-placement.sinYaw*normal.x+placement.cosYaw*normal.z;
                auto hit=origin+direction*t;sample.x=hit.x;sample.y=hit.y;sample.z=hit.z;publish(object,sample,color,t,-1);
            }continue;
        }
        auto* geometry=prepare(object);if(!geometry)continue;check(object,geometry->exterior,-1);
        for(const auto& group:geometry->caps)if(!group.second.vertices.empty())check(object,group.second,group.first);
    }return changed;
}
bool segment(Vec3 start,Vec3 end,float& fraction,TraceMask mask){builder::Target target;target.distance=game::len(end-start);
    if(target.distance<.001f)return false;float distance=target.distance;if(!trace(start,(end-start)*(1/distance),distance,target,mask))return false;fraction=target.distance/distance;return true;}
bool intersects(const Object& object,Vec3 low,Vec3 high){
    if(object.kind==Kind::Tree&&object.index>=0){const auto& tree=game::trees[object.index];float radius=jolt_world::treeRadius(tree);
        for(auto box:trunkPieces(object.index)){Vec3 lo{std::max(low.x,box.low.x),std::max(low.y,box.low.y),std::max(low.z,box.low.z)},hi{std::min(high.x,box.high.x),std::min(high.y,box.high.y),std::min(high.z,box.high.z)};
            if(hi.x<=lo.x||hi.y<=lo.y||hi.z<=lo.z)continue;
            float x=std::clamp(tree.p.x,lo.x,hi.x),z=std::clamp(tree.p.z,lo.z,hi.z);if(game::len(game::Vec2{x,z}-tree.p)<radius)return true;}}
    if(!edited(object.id)){auto* source=dx11::mesh(object.model);if(!source)return false;
        auto world=dx11::clipBuildingMesh(instance(object,source),{});return meshIntersects(world,low,high,object.kind==Kind::Rock);}
    auto* geometry=prepare(object);if(!geometry)return false;
    if(meshIntersects(geometry->exterior,low,high,false))return true;for(const auto& cap:geometry->caps)if(meshIntersects(cap.second,low,high,false))return true;
    if(object.kind==Kind::Rock){auto closed=geometry->exterior;for(const auto& cap:geometry->caps)closed.vertices.insert(closed.vertices.end(),cap.second.vertices.begin(),cap.second.vertices.end());return meshIntersects(closed,low,high);}
    return false;}
bool appendIfEdited(const std::string& id,std::vector<dx11::ModelInstance>& instances){
    if(!edited(id))return false;Object object;if(!find(id,object))return false;auto* geometry=prepare(object);if(!geometry)return false;
    if(!geometry->exterior.vertices.empty())instances.push_back({&geometry->exterior,object.kind==Kind::Rock?0:4,1,1,1,1,0,0,0,0,0,0,0,object.tint.r,object.tint.g,object.tint.b});
    for(const auto& cap:geometry->caps)if(!cap.second.vertices.empty())instances.push_back({&cap.second,2,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1});return true;
}
std::vector<destruction::Box> trunkPieces(std::size_t index){
    if(index>=game::trees.size()||game::trees[index].destroyed)return {};const auto& tree=game::trees[index];float radius=jolt_world::treeRadius(tree),height=std::max(16.0f,tree.height*tree.scale*.6f),root=terrain::baseHeight(tree.p);
    std::vector<destruction::Box> pieces{{{tree.p.x-radius,root,tree.p.z-radius},{tree.p.x+radius,root+height,tree.p.z+radius}}};
    auto found=cuts.find(treeId(index));if(!builder::active()||found==cuts.end())return pieces;
    for(auto cell:found->second){auto low=builder::cellLow(cell),high=low+Vec3{builder::BLOCK_SIZE,builder::BLOCK_SIZE,builder::BLOCK_SIZE};std::vector<destruction::Box> remaining;
        auto add=[&](Vec3 a,Vec3 b){if(b.x-a.x>.01f&&b.y-a.y>.01f&&b.z-a.z>.01f)remaining.push_back({a,b});};
        for(auto box:pieces){Vec3 lo{std::max(box.low.x,low.x),std::max(box.low.y,low.y),std::max(box.low.z,low.z)},hi{std::min(box.high.x,high.x),std::min(box.high.y,high.y),std::min(box.high.z,high.z)};
            if(hi.x<=lo.x||hi.y<=lo.y||hi.z<=lo.z){remaining.push_back(box);continue;}
            add(box.low,{lo.x,box.high.y,box.high.z});add({hi.x,box.low.y,box.low.z},box.high);
            add({lo.x,box.low.y,box.low.z},{hi.x,lo.y,box.high.z});add({lo.x,hi.y,box.low.z},{hi.x,box.high.y,box.high.z});
            add({lo.x,lo.y,box.low.z},{hi.x,hi.y,lo.z});add({lo.x,lo.y,hi.z},{hi.x,hi.y,box.high.z});
        }pieces=std::move(remaining);
    }return pieces;
}
std::vector<Vertex> collisionTriangles(const Object& object){std::vector<Vertex> output;auto* geometry=prepare(object);if(!geometry)return output;
    auto add=[&](const dx11::Mesh& mesh){for(std::size_t n=0;n+2<mesh.vertices.size();n+=3){auto a=mesh.vertices[n],b=mesh.vertices[n+1],c=mesh.vertices[n+2];
        triangles({a,b,c},output,{a.nx+b.nx+c.nx,a.ny+b.ny+c.ny,a.nz+b.nz+c.nz});}};
    add(geometry->exterior);for(const auto& group:geometry->caps)add(group.second);return output;
}
}
