#include "terrain.h"
#include "regions.h"
#include "commerce.h"
#include "data_file.h"
#include "excavation.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace terrain {
namespace {
std::vector<Landform> forms;
std::vector<float> grid(samples*samples,0);
std::uint64_t generation=0;
std::string error;
float smooth(float value){value=std::clamp(value,0.0f,1.0f);return value*value*(3-2*value);}
float distanceToSegment(game::Vec2 p,game::Vec2 a,game::Vec2 b){
    auto delta=b-a;
    float length=delta.x*delta.x+delta.z*delta.z;
    float t=length>0?std::clamp(((p-a).x*delta.x+(p-a).z*delta.z)/length,0.0f,1.0f):0;
    return game::len(p-(a+delta*t));
}
float authoredHeight(game::Vec2 p){
    const auto* region=regions::at(p);
    if(!region||region->biome==regions::Biome::City||regions::waterAt(p))return 0;
    float edge=std::min({p.x-region->x0,region->x1-p.x,p.z-region->z0,region->z1-p.z});
    float mask=smooth(edge/350);
    // Broad, smooth road verges retain the existing missions and traffic routes.
    for(const auto& road:regions::roads())
        mask=std::min(mask,smooth((distanceToSegment(p,road.start,road.end)-road.width*.5f-75)/500));
    for(const auto& hub:regions::hubs())mask=std::min(mask,smooth((game::len(p-hub.p)-350)/450));
    for(const auto& shop:commerce::shops)mask=std::min(mask,smooth((game::len(p-shop.p)-160)/300));
    for(const auto& house:commerce::houses)mask=std::min(mask,smooth((game::len(p-house.p)-160)/300));
    // Keep the riverbanks and harbor crossing level at their original height.
    mask=std::min(mask,smooth((std::abs(p.x-7800)-300)/450));
    float amplitude=region->biome==regions::Biome::Snow?36:
        region->biome==regions::Biome::Desert?22:region->biome==regions::Biome::Savanna?17:12;
    float result=amplitude*(std::sin(p.x*.0026f)*std::cos(p.z*.0021f)+
        .35f*std::sin(p.x*.008f+p.z*.004f));
    for(const auto& form:forms){
        float x=(p.x-form.center.x)/form.rx,z=(p.z-form.center.z)/form.rz;
        float r2=x*x+z*z;
        if(r2>=1)continue;
        float envelope=(1-r2)*(1-r2);
        float ridges=form.height>0?.82f+.18f*std::cos(x*11+std::sin(z*7)):1;
        result+=form.height*envelope*ridges;
    }
    return result*mask;
}
float sample(int x,int z){return grid[std::clamp(z,0,int(samples)-1)*samples+std::clamp(x,0,int(samples)-1)];}
}
bool load(const char* path){
    data_file::Ini file;error.clear();
    if(!file.load(path?path:data_file::resourcePath("terrain.ini"))||!file.version(1)){error=file.lastError();return false;}
    int count=0;
    if(!file.integer("Terrain","Count",count,1,64)){error=file.lastError();return false;}
    std::vector<Landform> parsed;std::set<std::string> ids;
    for(int i=0;i<count;++i){
        Landform form{};auto section="Landform"+std::to_string(i);
        if(!file.string(section,"Id",form.id)||!data_file::validId(form.id)||!ids.insert(form.id).second||
           !file.real(section,"X",form.center.x,0,regions::WIDTH)||
           !file.real(section,"Z",form.center.z,0,regions::DEPTH)||
           !file.real(section,"RadiusX",form.rx,250,2500)||
           !file.real(section,"RadiusZ",form.rz,250,2500)||
           !file.real(section,"Height",form.height,-250,1000)){
            error=file.lastError().empty()?"Invalid ["+section+"] landform":file.lastError();return false;
        }
        parsed.push_back(form);
    }
    forms=std::move(parsed);
    for(unsigned z=0;z<samples;++z)for(unsigned x=0;x<samples;++x)
        grid[z*samples+x]=authoredHeight({x*spacing,z*spacing});
    ++generation;return true;
}
const std::string& lastError(){return error;}
const std::vector<Landform>& landforms(){return forms;}
const std::vector<float>& heights(){return grid;}
std::uint64_t revision(){return generation;}
float baseHeight(game::Vec2 p){
    float gx=std::clamp(p.x/spacing,0.0f,float(samples-1)),gz=std::clamp(p.z/spacing,0.0f,float(samples-1));
    int x=std::min(int(gx),int(samples)-2),z=std::min(int(gz),int(samples)-2);
    float u=gx-x,v=gz-z,a=sample(x,z),b=sample(x+1,z),c=sample(x+1,z+1),d=sample(x,z+1);
    // Jolt splits each cell from its top-left to bottom-right corner.
    return v>=u?a+(d-a)*v+(c-d)*u:a+(b-a)*u+(c-b)*v;
}
float height(game::Vec2 p){return excavation::floorBelow({p.x,baseHeight(p)+1,p.z});}
bool contains(game::Vec3 p){return excavation::solid(p);}
game::Vec3 normal(game::Vec2 p){
    return game::norm(game::Vec3{baseHeight({p.x-25,p.z})-baseHeight({p.x+25,p.z}),50,
        baseHeight({p.x,p.z-25})-baseHeight({p.x,p.z+25})});
}
bool segmentHit(game::Vec3 start,game::Vec3 end,float& fraction){
    // Traverse no more than half a grid cell between probes, then bisect the
    // first crossing. This also catches horizontal shots into rising slopes.
    auto delta=end-start;
    int steps=std::max(1,int(std::ceil(std::max({std::abs(delta.x),std::abs(delta.z),std::abs(delta.y)})/20)));
    float previous=0;
    for(int i=0;i<=steps;++i){
        float t=float(i)/steps;auto p=start+delta*t;
        if(p.x<0||p.z<0||p.x>regions::WIDTH||p.z>regions::DEPTH){previous=t;continue;}
        if(contains(p)){
            float low=previous,high=t;
            for(int j=0;j<14;++j){float mid=(low+high)*.5f;auto q=start+delta*mid;
                if(contains(q))high=mid;else low=mid;}
            fraction=high;return true;
        }
        previous=t;
    }
    return false;
}
}
