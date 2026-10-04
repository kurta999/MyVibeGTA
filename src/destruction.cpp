#include "destruction.h"
#include "jolt_world.h"
namespace destruction {
using namespace game;
std::vector<Box> boxes(const Building& b){
    if(!b.damaged)return {{{b.x,0,b.z},{b.x+b.w,b.h,b.z+b.d}}};
    std::vector<Box> result;for(const auto& piece:b.pieces)result.push_back({piece.low,piece.high});
    return result;
}
bool contains(const Building& b,Vec3 p,float pad){
    for(const auto& box:boxes(b))if(p.x>box.low.x-pad&&p.x<box.high.x+pad&&
        p.y>box.low.y-pad&&p.y<box.high.y+pad&&p.z>box.low.z-pad&&p.z<box.high.z+pad)return true;
    return false;
}
bool segment(const Building& b,Vec3 start,Vec3 end,float& entry){
    Vec3 d=end-start;float nearest=2;
    for(const auto& box:boxes(b)){
        float first=0,last=1;
        const float p[]={start.x,start.y,start.z},v[]={d.x,d.y,d.z};
        const float lo[]={box.low.x,box.low.y,box.low.z},hi[]={box.high.x,box.high.y,box.high.z};
        bool hit=true;
        for(int a=0;a<3;++a){
            if(std::abs(v[a])<0.000001f){if(p[a]<lo[a]||p[a]>hi[a]){hit=false;break;}}
            else{float l=(lo[a]-p[a])/v[a],h=(hi[a]-p[a])/v[a];if(l>h)std::swap(l,h);
                first=std::max(first,l);last=std::min(last,h);if(first>last){hit=false;break;}}
        }
        if(hit)nearest=std::min(nearest,first);
    }
    entry=nearest;return nearest<=1;
}
void blast(Vec3 p,float radius){
    float r=std::clamp(radius*0.65f,24.0f,90.0f);
    for(std::size_t index=0;index<buildings.size();++index){
        auto& b=buildings[index];
        Vec3 nearest{std::clamp(p.x,b.x,b.x+b.w),std::clamp(p.y,0.0f,b.h),std::clamp(p.z,b.z,b.z+b.d)};
        if(len(p-nearest)>radius*0.55f)continue;
        // Subtract a bounded blast volume. Repeated shots enlarge the hole;
        // all consumers use the remaining boxes, including streamed Jolt bodies.
        Box cut{p-Vec3{r,r,r},p+Vec3{r,r,r}};
        std::vector<BuildingPiece> remaining;
        bool changed=false;
        auto add=[&](Vec3 low,Vec3 high){if(high.x-low.x>0.5f&&high.y-low.y>0.5f&&high.z-low.z>0.5f)
            remaining.push_back({low,high});};
        for(const auto& box:boxes(b)){
            Vec3 lo{std::max(box.low.x,cut.low.x),std::max(box.low.y,cut.low.y),std::max(box.low.z,cut.low.z)};
            Vec3 hi{std::min(box.high.x,cut.high.x),std::min(box.high.y,cut.high.y),std::min(box.high.z,cut.high.z)};
            if(hi.x<=lo.x||hi.y<=lo.y||hi.z<=lo.z){add(box.low,box.high);continue;}
            changed=true;
            add(box.low,{lo.x,box.high.y,box.high.z});
            add({hi.x,box.low.y,box.low.z},box.high);
            add({lo.x,box.low.y,box.low.z},{hi.x,lo.y,box.high.z});
            add({lo.x,hi.y,box.low.z},{hi.x,box.high.y,box.high.z});
            add({lo.x,lo.y,box.low.z},{hi.x,hi.y,lo.z});
            add({lo.x,lo.y,hi.z},{hi.x,hi.y,box.high.z});
        }
        // Bound fragmentation without filling an existing hole back in.
        if(!changed||remaining.size()>192)continue;
        b.damaged=true;b.pieces=std::move(remaining);b.cuts.push_back({cut.low,cut.high});
        jolt_world::rebuildBuilding(index);
        for(int n=0;n<12;++n){
            float a=n*2.39996f;Vec3 position=nearest+Vec3{std::cos(a)*r*.3f,5+float(n%3)*7,std::sin(a)*r*.3f};
            jolt_world::spawnFragment(position,{7+float(n%4)*3,8,9},
                {std::cos(a)*90,100+float(n%3)*35,std::sin(a)*90},b.c,1);
        }
    }
}
}
