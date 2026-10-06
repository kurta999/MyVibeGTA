#include "builder_feedback.h"
#include "audio.h"
#include "terrain.h"
#include "scenery_edits.h"
#include "destruction.h"
#include <algorithm>
#include <cmath>

namespace builder_feedback {
namespace {
std::vector<Particle> chips;Contact latest{};std::uint32_t randomState=0x65742311u;
float random(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return float(randomState&65535)/65535;}
game::Vec3 cross(game::Vec3 a,game::Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
bool project(const builder::Target& target,game::Vec3 point,game::Vec3& result){
    using namespace game;
    if(target.source==builder::Source::Tree||target.source==builder::Source::Scenery){builder::Target hit;hit.distance=24;
        if(!scenery_edits::trace(point+target.normal*12,target.normal*-1,24,hit)||hit.objectId!=target.objectId||!(hit.cell==target.cell))return false;
        // Keep the ray's surface coordinates: a smoothed vertex normal can move
        // the decal sideways into sampled-mesh gaps or across an alpha edge.
        result=hit.point+target.normal*.18f;return true;
    }
    if(target.source==builder::Source::Ground){
        if(target.normal.y>.5f&&std::abs(target.point.y-terrain::baseHeight({target.point.x,target.point.z}))<1)point.y=terrain::baseHeight({point.x,point.z});
        if(!terrain::contains(point-target.normal*.3f)||!(builder::cellAt(point-target.normal*.3f)==target.cell))return false;
    }else if(target.source==builder::Source::Building){
        if(target.index<0||target.index>=int(buildings.size()))return false;bool inside=false;auto p=point-target.normal*.3f;
        for(const auto& box:destruction::boxes(buildings[target.index]))inside|=p.x>box.low.x&&p.x<box.high.x&&p.y>box.low.y&&p.y<box.high.y&&p.z>box.low.z&&p.z<box.high.z;
        if(!inside||!(builder::cellAt(p)==target.cell))return false;
    }else if(target.source==builder::Source::Block){if(!(builder::cellAt(point-target.normal*.3f)==target.cell))return false;}
    else return false;
    result=point+target.normal*.18f;return true;
}
}
void clear(){chips.clear();latest={};randomState=0x65742311u;}
Cue cue(int item,builder::Tool tool){
    if(tool==builder::Tool::Brush)return Cue::Brush;if(item<0||item>=int(builder::items().size()))return Cue::Stone;
    const auto& material=builder::items()[item];const auto& id=material.id;
    if(id=="snow")return Cue::Snow;if(id=="sand"||id=="gravel")return Cue::Sand;if(id=="leaves")return Cue::Foliage;
    if(id=="soil"||id=="tilled-soil")return Cue::Soil;
    if(id.find("ore")!=std::string::npos||id.find("ingot")!=std::string::npos||id=="diamond")return Cue::Metal;
    if(material.harvestTool==builder::Tool::Axe)return Cue::Wood;return Cue::Stone;
}
float cycle(builder::Tool tool){const float periods[]{.45f,.5f,.55f,.65f,.4f,.4f,.35f};return periods[int(tool)];}
void contact(const builder::Target& target,builder::Tool tool,bool completed){
    if(!builder::active()||builder::modal()||target.source==builder::Source::None||target.item<0)return;
    latest={target,tool,cue(target.item,tool),latest.serial+1};bool dust=latest.cue==Cue::Brush;
    audio::playAt(audio::Effect::BuilderContact,target.point.x,target.point.z,int(latest.cue));
    int count=dust?4:completed?14:5;
    for(int n=0;n<count;++n){if(chips.size()>=192)chips.erase(chips.begin());auto normal=game::norm(target.normal);float life=dust?.45f+random()*.35f:.32f+random()*.45f;
        game::Vec3 spread{random()*2-1,random()*1.4f+.2f,random()*2-1};float speed=dust?10:completed?48:28;
        // Brushing should lift a small readable cloud clear of the deposit,
        // rather than leave translucent specks buried against its surface.
        auto origin=target.point+normal*(dust?2.0f:.5f);
        if(dust)origin=origin+game::Vec3{spread.x*2,0,spread.z*2};
        chips.push_back({origin,spread*speed+normal*(dust?12:18),life,life,dust?6+random()*4:1+random()*1.3f,target.item,latest.cue,dust});
    }
}
void update(float dt){if(!builder::active()){clear();return;}dt=std::clamp(dt,0.0f,.1f);
    for(auto& p:chips){p.life-=dt;p.v.y-=(p.dust?-5:p.cue==Cue::Foliage?40:180)*dt;p.p=p.p+p.v*dt;}
    chips.erase(std::remove_if(chips.begin(),chips.end(),[](const Particle& p){return p.life<=0;}),chips.end());
}
const std::vector<Particle>& particles(){return chips;}
const Contact& lastContact(){return latest;}
std::vector<Line> cracks(){std::vector<Line> lines;float progress=builder::miningProgress();const auto& target=builder::target();
    if(!builder::active()||builder::modal()||progress<=0||target.source==builder::Source::Deposit||target.item<0||cue(target.item,builder::Tool::None)==Cue::Foliage)return lines;
    auto n=game::norm(target.normal);auto u=game::norm(cross(std::abs(n.y)>.8f?game::Vec3{1,0,0}:game::Vec3{0,1,0},n)),v=cross(n,u);
    const float pattern[][4]={{0,0,3,4},{3,4,1,8},{1,8,5,12},{0,0,-4,2},{-4,2,-7,0},{-7,0,-12,3},{0,0,2,-4},{2,-4,-1,-8},{-1,-8,3,-13},{3,4,8,5},{8,5,12,9},{-4,2,-5,8},{-5,8,-10,12},{2,-4,8,-5},{8,-5,13,-2},{-1,-8,-7,-9},{-7,-9,-12,-13},{1,8,-2,13},{-7,0,-9,-5},{8,5,13,4}};
    int count=std::min(20,2+int(progress*18));
    for(int i=0;i<count;++i){const auto& path=pattern[i];for(int part=0;part<3;++part){game::Vec3 points[2];bool valid=true;
        for(int end=0;end<2;++end){float t=float(part+end)/3;auto p=target.point+u*(path[0]+(path[2]-path[0])*t)+v*(path[1]+(path[3]-path[1])*t);valid&=project(target,p,points[end]);}
        if(valid)lines.push_back({points[0],points[1],.3f+progress*.35f});}}
    return lines;
}
}
