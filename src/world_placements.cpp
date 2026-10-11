#include "world_placements.h"
#include "data_file.h"
#include <fstream>
#include <sstream>
#include <set>
#include <cmath>
#include <cctype>

namespace world_placements {
namespace {
Scene scene;
std::string error;
bool Range(float value,float low,float high){return std::isfinite(value)&&value>=low&&value<=high;}
bool Point(game::Vec2 point){return Range(point.x,0,regions::WIDTH)&&Range(point.z,0,regions::DEPTH);}
bool Color(game::Color color){return Range(color.r,0,1)&&Range(color.g,0,1)&&Range(color.b,0,1);}
bool Model(const std::string& model){
    if(model.empty()||model.size()>128)return false;
    for(unsigned char c:model)if(!std::isalnum(c)&&c!='_'&&c!='-')return false;
    return true;
}
bool Asset(const std::string& model){
    if(model.empty()||model.size()>128||model.front()=='/'||model.back()=='/')return false;
    for(unsigned char c:model)if(!std::isalnum(c)&&c!='_'&&c!='-'&&c!='/')return false;
    return true;
}
bool Position(game::Vec3 p){return Point({p.x,p.z})&&Range(p.y,-1000,5000);}
}
bool Load(const char* path,unsigned roadCount){
    error.clear();std::string file=path?path:data_file::resourcePath("regional_placements.txt");
    std::ifstream input(file);if(!input){error="Cannot open "+file;return false;}
    std::string line;std::getline(input,line);
    if(data_file::trim(line)!="MINICITY_REGIONAL_PLACEMENTS 1"){error=file+": invalid placement header/version";return false;}
    Scene parsed;std::set<std::string> ids,editIds;int number=1;
    while(std::getline(input,line)){
        ++number;if(number>100001){error=file+": too many placements";return false;}
        line=data_file::trim(line);if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);std::string type,id;row>>type>>id;
        auto fail=[&](const char* detail){error=file+":"+std::to_string(number)+": "+detail;return false;};
        if(!data_file::validId(id)||!ids.insert(id).second)return fail("invalid/duplicate placement ID");
        bool valid=false;
        if(type=="building"){
            game::Building b{};b.id=id;row>>b.x>>b.z>>b.w>>b.d>>b.h>>b.c.r>>b.c.g>>b.c.b;
            valid=Point({b.x,b.z})&&Range(b.w,1,5000)&&Range(b.d,1,5000)&&Range(b.h,1,2000)&&Color(b.c)&&Point({b.x+b.w,b.z+b.d});
            parsed.buildings.push_back(std::move(b));
        }else if(type=="tree"){
            game::Tree t{};t.id=id;int palm=0;row>>t.p.x>>t.p.z>>palm>>t.variant>>t.scale>>t.modelId>>t.crownWidth>>t.height>>t.climbHeight;t.palm=palm!=0;
            valid=Point(t.p)&&palm>=0&&palm<=1&&t.variant>=0&&t.variant<=2&&Range(t.scale,.01f,100)&&Model(t.modelId)&&Range(t.crownWidth,.1f,1000)&&Range(t.height,.1f,1000)&&Range(t.climbHeight,0,1000);
            parsed.trees.push_back(std::move(t));
        }else if(type=="decoration"){
            regions::Decoration d{};d.id=id;row>>d.p.x>>d.p.z>>d.modelId>>d.width>>d.height>>d.depth;
            valid=Point(d.p)&&Model(d.modelId)&&Range(d.width,.1f,1000)&&Range(d.height,.1f,1000)&&Range(d.depth,.1f,1000);parsed.decorations.push_back(std::move(d));
        }else if(type=="pedestrian"){
            game::Ped p{};p.id=id;int armed=0;row>>p.p.x>>p.p.z>>p.speed>>p.style>>armed>>p.shirt.r>>p.shirt.g>>p.shirt.b;p.armed=armed!=0;p.target=p.p;
            valid=Point(p.p)&&Range(p.speed,1,300)&&p.style>=0&&p.style<4&&armed>=0&&armed<=1&&Color(p.shirt);parsed.pedestrians.push_back(std::move(p));
        }else if(type=="vehicle"){
            game::Vehicle v{};v.id=id;int kind=0;row>>v.p.x>>v.p.z>>v.angle>>kind>>v.trafficRoute>>v.c.r>>v.c.g>>v.c.b;
            valid=Point(v.p)&&Range(v.angle,-game::PI,game::PI)&&kind>=0&&kind<=int(game::Kind::Airplane)&&v.trafficRoute>=-1&&v.trafficRoute<int(roadCount)&&Color(v.c);
            v.kind=game::Kind(kind);parsed.vehicles.push_back(std::move(v));
        }else if(type=="outcrop"){
            Outcrop o;o.id=id;row>>o.editId>>o.p.x>>o.p.z>>o.model>>o.width>>o.height>>o.yaw;
            valid=Point(o.p)&&Asset(o.model)&&Range(o.width,.1f,1000)&&Range(o.height,.1f,1000)&&Range(o.yaw,0,game::PI*2)&&
                o.editId.rfind("outcrop:",0)==0&&o.editId.size()<128&&editIds.insert(o.editId).second;
            for(unsigned char c:o.editId)valid=valid&&(std::isalnum(c)||c==':'||c=='-'||c=='_');
            parsed.outcrops.push_back(std::move(o));
        }else if(type=="light"){
            Light l;l.id=id;int street=0;row>>l.p.x>>l.p.y>>l.p.z>>l.radius>>l.color.r>>l.color.g>>l.color.b>>l.brightness>>street;
            valid=Position(l.p)&&Range(l.radius,1,2000)&&Color(l.color)&&Range(l.brightness,0,100)&&street>=0&&street<=1;
            l.street=street!=0;parsed.lights.push_back(std::move(l));
        }else if(type=="base-ground"||type=="ground"||type=="beam"||type=="model"||type=="prop"||type=="glow"||type=="plant"){
            Visual v;v.id=id;v.base=type=="base-ground";
            if(type=="base-ground"||type=="ground"){
                v.kind=VisualKind::Ground;
                row>>v.a.x>>v.a.z>>v.b.x>>v.b.z>>v.a.y>>v.color.r>>v.color.g>>v.color.b>>v.tile>>v.material;
                valid=Position(v.a)&&Point({v.b.x,v.b.z})&&v.b.x>v.a.x&&v.b.z>v.a.z&&Color(v.color)&&v.tile>=-1&&v.tile<16&&v.material>=0&&v.material<16;
            }else if(type=="beam"){
                v.kind=VisualKind::Beam;
                row>>v.a.x>>v.a.y>>v.a.z>>v.b.x>>v.b.y>>v.b.z>>v.width>>v.height>>v.color.r>>v.color.g>>v.color.b;
                valid=Position(v.a)&&Position(v.b)&&Range(v.width,.01f,1000)&&Range(v.height,.01f,1000)&&Color(v.color)&&
                    (v.a.x!=v.b.x||v.a.y!=v.b.y||v.a.z!=v.b.z);
            }else{
                v.kind=type=="model"?VisualKind::Model:type=="glow"?VisualKind::Glow:type=="plant"?VisualKind::Plant:VisualKind::Prop;
                if(v.kind!=VisualKind::Glow)row>>v.model;
                row>>v.a.x>>v.a.y>>v.a.z;
                if(v.kind!=VisualKind::Prop)row>>v.b.x>>v.b.y>>v.b.z>>v.yaw>>v.color.r>>v.color.g>>v.color.b;
                else row>>v.yaw>>v.scale;
                valid=(v.kind==VisualKind::Glow||Asset(v.model))&&Position(v.a)&&Range(v.yaw,-game::PI*2,game::PI*2)&&Color(v.color)&&
                    (v.kind!=VisualKind::Prop?(Range(v.b.x,.01f,5000)&&Range(v.b.y,.01f,5000)&&Range(v.b.z,.01f,5000)):Range(v.scale,.01f,100));
            }
            row>>v.range;valid=valid&&Range(v.range,1,10000);
            parsed.visuals.push_back(std::move(v));
        }else return fail("unknown placement type");
        if(!row||!valid)return fail("invalid placement fields");
        row>>std::ws;if(!row.eof())return fail("unexpected trailing fields");
    }
    if(!input.eof()){error=file+": read error";return false;}
    scene=std::move(parsed);return true;
}
const Scene& Data(){return scene;}
const std::string& LastError(){return error;}
}
