#include "dx11_damage.h"
#include <algorithm>
#include <cmath>
namespace dx11 {
namespace {
using Polygon=std::vector<Vertex>;
float coordinate(const Vertex& v,int axis){return axis==0?v.x:axis==1?v.y:v.z;}
float coordinate(game::Vec3 v,int axis){return axis==0?v.x:axis==1?v.y:v.z;}
Vertex interpolate(const Vertex& a,const Vertex& b,float t){
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,
        a.nx+(b.nx-a.nx)*t,a.ny+(b.ny-a.ny)*t,a.nz+(b.nz-a.nz)*t,
        a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t,a.r+(b.r-a.r)*t,
        a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t,a.a+(b.a-a.a)*t};
}
void split(const Polygon& p,int axis,float plane,bool lower,Polygon& inside,Polygon& outside){
    if(p.empty())return;
    auto distance=[&](const Vertex& v){return (coordinate(v,axis)-plane)*(lower?1.0f:-1.0f);};
    for(std::size_t n=0;n<p.size();++n){
        const Vertex& a=p[n];const Vertex& b=p[(n+1)%p.size()];
        float da=distance(a),db=distance(b);bool ia=da>=0,ib=db>=0;
        (ia?inside:outside).push_back(a);
        if(ia!=ib){Vertex v=interpolate(a,b,da/(da-db));inside.push_back(v);outside.push_back(v);}
    }
}
bool overlaps(const Polygon& p,const game::BuildingPiece& box){
    for(int axis=0;axis<3;++axis){float lo=coordinate(p[0],axis),hi=lo;
        for(const auto& v:p){lo=std::min(lo,coordinate(v,axis));hi=std::max(hi,coordinate(v,axis));}
        if(hi<coordinate(box.low,axis)||lo>coordinate(box.high,axis))return false;
    }
    return true;
}
void subtract(const Polygon& input,const game::BuildingPiece& cut,std::vector<Polygon>& result){
    if(!overlaps(input,cut)){result.push_back(input);return;}
    Polygon remaining=input;
    for(int axis=0;axis<3&&!remaining.empty();++axis)for(int side=0;side<2&&!remaining.empty();++side){
        Polygon inside,outside;
        split(remaining,axis,coordinate(side?cut.high:cut.low,axis),side==0,inside,outside);
        if(outside.size()>=3)result.push_back(std::move(outside));
        remaining=std::move(inside);
    }
}
Vertex worldVertex(Vertex v,const ModelInstance& i){
    float x=(v.x-i.centerX)*i.scaleX,y=(v.y-i.minY)*i.scaleY,z=(v.z-i.centerZ)*i.scaleZ;
    v.x=i.x+i.cosYaw*x+i.sinYaw*z;v.y=i.y+y;v.z=i.z-i.sinYaw*x+i.cosYaw*z;
    x=v.nx/i.scaleX;y=v.ny/i.scaleY;z=v.nz/i.scaleZ;
    float length=std::max(.0001f,std::sqrt(x*x+y*y+z*z));
    v.nx=(i.cosYaw*x+i.sinYaw*z)/length;v.ny=y/length;v.nz=(-i.sinYaw*x+i.cosYaw*z)/length;
    return v;
}
}
Mesh clipBuildingMesh(const ModelInstance& instance,const std::vector<game::BuildingPiece>& cuts){
    const Mesh& source=*instance.source;Mesh result=source;result.vertices.clear();result.indices.clear();result.materialRanges.clear();
    result.shadowProxy=nullptr;result.temporalStable=false;result.allowTessellation=false;
    const std::size_t total=source.indices.empty()?source.vertices.size():source.indices.size();
    for(std::size_t range=0;range<std::max<std::size_t>(1,source.materialRanges.size());++range){
        unsigned start=source.materialRanges.empty()?0:source.materialRanges[range].start;
        unsigned count=source.materialRanges.empty()?unsigned(total):source.materialRanges[range].count;
        std::size_t outputStart=result.vertices.size();
        for(unsigned n=start;n+2<start+count;n+=3){
            Polygon triangle;for(unsigned corner=0;corner<3;++corner){
                unsigned vertex=source.indices.empty()?n+corner:source.indices[n+corner];
                triangle.push_back(worldVertex(source.vertices[vertex],instance));
            }
            std::vector<Polygon> polygons{std::move(triangle)};
            for(const auto& cut:cuts){std::vector<Polygon> surviving;
                for(const auto& polygon:polygons)subtract(polygon,cut,surviving);
                polygons=std::move(surviving);if(polygons.empty())break;
            }
            for(const auto& polygon:polygons)for(std::size_t k=1;k+1<polygon.size();++k)
                result.vertices.insert(result.vertices.end(),{polygon[0],polygon[k],polygon[k+1]});
        }
        if(!source.materialRanges.empty()&&result.vertices.size()>outputStart){
            auto material=source.materialRanges[range];material.start=unsigned(outputStart);
            material.count=unsigned(result.vertices.size()-outputStart);result.materialRanges.push_back(std::move(material));
        }
    }
    if(!result.vertices.empty()){
        result.minX=result.maxX=result.vertices[0].x;result.minY=result.maxY=result.vertices[0].y;result.minZ=result.maxZ=result.vertices[0].z;
        for(const auto& v:result.vertices){result.minX=std::min(result.minX,v.x);result.maxX=std::max(result.maxX,v.x);
            result.minY=std::min(result.minY,v.y);result.maxY=std::max(result.maxY,v.y);result.minZ=std::min(result.minZ,v.z);result.maxZ=std::max(result.maxZ,v.z);}
    }
    return result;
}
Mesh buildingInteriorFaces(const game::Building& b){
    Mesh result;result.allowTessellation=false;result.temporalStable=false;
    const game::Vec3 low{b.x,0,b.z},high{b.x+b.w,b.h,b.z+b.d};
    for(std::size_t n=0;n<b.cuts.size();++n){const auto& cut=b.cuts[n];
        for(int axis=0;axis<3;++axis)for(int side=0;side<2;++side){
            float plane=coordinate(side?cut.high:cut.low,axis);
            if(plane<=coordinate(low,axis)||plane>=coordinate(high,axis))continue;
            int u=(axis+1)%3,v=(axis+2)%3;
            float u0=std::max(coordinate(cut.low,u),coordinate(low,u)),u1=std::min(coordinate(cut.high,u),coordinate(high,u));
            float v0=std::max(coordinate(cut.low,v),coordinate(low,v)),v1=std::min(coordinate(cut.high,v),coordinate(high,v));
            if(u1<=u0||v1<=v0)continue;
            Polygon face;
            for(auto corner:{std::pair<float,float>{u0,v0},{u1,v0},{u1,v1},{u0,v1}}){
                float p[3]{},normal[3]{};p[axis]=plane;p[u]=corner.first;p[v]=corner.second;normal[axis]=side?-1.0f:1.0f;
                face.push_back({p[0],p[1],p[2],normal[0],normal[1],normal[2],0,0,b.c.r*.62f,b.c.g*.62f,b.c.b*.62f,1});
            }
            std::vector<Polygon> polygons{std::move(face)};
            for(std::size_t other=0;other<b.cuts.size();++other)if(other!=n){
                std::vector<Polygon> remaining;for(const auto& polygon:polygons)subtract(polygon,b.cuts[other],remaining);
                polygons=std::move(remaining);
            }
            for(const auto& polygon:polygons)for(std::size_t k=1;k+1<polygon.size();++k)
                result.vertices.insert(result.vertices.end(),{polygon[0],polygon[k],polygon[k+1]});
        }
    }
    result.minX=b.x;result.minY=0;result.minZ=b.z;result.maxX=b.x+b.w;result.maxY=b.h;result.maxZ=b.z+b.d;
    return result;
}
}
