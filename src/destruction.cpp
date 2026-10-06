#include "destruction.h"
#include "jolt_world.h"
#include "masonry.h"
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
bool cut(std::size_t index,Box volume){
    if(index>=buildings.size())return false;auto& b=buildings[index];std::vector<BuildingPiece> remaining;bool changed=false;
    auto add=[&](Vec3 low,Vec3 high){if(high.x-low.x>.5f&&high.y-low.y>.5f&&high.z-low.z>.5f)remaining.push_back({low,high});};
    for(const auto& box:boxes(b)){
        Vec3 lo{std::max(box.low.x,volume.low.x),std::max(box.low.y,volume.low.y),std::max(box.low.z,volume.low.z)};
        Vec3 hi{std::min(box.high.x,volume.high.x),std::min(box.high.y,volume.high.y),std::min(box.high.z,volume.high.z)};
        if(hi.x<=lo.x||hi.y<=lo.y||hi.z<=lo.z){add(box.low,box.high);continue;}changed=true;
        add(box.low,{lo.x,box.high.y,box.high.z});add({hi.x,box.low.y,box.low.z},box.high);
        add({lo.x,box.low.y,box.low.z},{hi.x,lo.y,box.high.z});add({lo.x,hi.y,box.low.z},{hi.x,box.high.y,box.high.z});
        add({lo.x,lo.y,box.low.z},{hi.x,hi.y,lo.z});add({lo.x,lo.y,hi.z},{hi.x,hi.y,box.high.z});
    }
    if(!changed||remaining.size()>4096||b.cuts.size()>=4096)return false;
    b.damaged=true;b.pieces=std::move(remaining);b.cuts.push_back({volume.low,volume.high});jolt_world::rebuildBuilding(index);return true;
}
void blast(Vec3 p,float radius){
    float r=std::clamp(radius*(radius>200?.85f:.65f),24.0f,425.0f);
    for(std::size_t index=0;index<buildings.size();++index){
        auto& b=buildings[index];
        Vec3 nearest{std::clamp(p.x,b.x,b.x+b.w),std::clamp(p.y,0.0f,b.h),std::clamp(p.z,b.z,b.z+b.d)};
        if(len(p-nearest)>radius*(radius>200?1.0f:.55f))continue;
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
        Vec3 outward=norm(p-Vec3{b.x+b.w*.5f,nearest.y,b.z+b.d*.5f});
        if(len(outward)<.1f)outward={1,0,0};
        Vec3 tangent{-outward.z,0,outward.x};
        int count=std::clamp(int(radius*.16f),16,32);
        for(int n=0;n<count;++n){
            int kind=n%masonry::Count;float a=n*2.39996f;
            Vec3 size=kind==masonry::Brick?Vec3{12,5,6}:kind==masonry::BrokenBrick?Vec3{8,5,6}:
                kind==masonry::Block?Vec3{17,9,10}:kind==masonry::Slab?Vec3{22,7,17}:Vec3{11+float(n%3)*3,10,12};
            Vec3 position=nearest+tangent*(std::cos(a)*r*.65f)+outward*(size.x*.65f+float(n%3)*5);
            position.y=std::clamp(nearest.y+std::sin(a)*r*.45f,size.y*.6f,std::max(size.y*.6f,b.h-size.y*.6f));
            // Eject from the removed facade into the street instead of trapping
            // masonry in surviving wall colliders.
            for(int attempt=0;attempt<12&&contains(b,position,std::max(size.x,size.z)*.6f);++attempt)position=position+outward*10;
            jolt_world::spawnFragment(position,size,outward*(65+float(n%4)*20)+tangent*(std::sin(a)*60)+Vec3{0,70+float(n%3)*25,0},b.c,0,kind);
        }
    }
}
}
