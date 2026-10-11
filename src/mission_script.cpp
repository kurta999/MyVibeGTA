#include "mission_script.h"
#include "data_file.h"
#include "game_internal.h"
#include "audio.h"
#include "logging.h"
#include <lua.hpp>
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <stdexcept>

namespace game { void completeMission(); void missionCard(const std::string&,const std::string&,float); }
namespace mission_script {
namespace {
struct Runtime {
    lua_State* state=luaL_newstate();
    std::vector<std::string> names;
    ~Runtime(){if(state)lua_close(state);}
};
std::unique_ptr<Runtime> runtime;
std::string error,objective;
void Budget(lua_State* state,lua_Debug*){luaL_error(state,"mission script instruction budget exceeded");}
bool Protected(lua_State* state,int arguments,int results){
    lua_sethook(state,Budget,LUA_MASKCOUNT,100000);
    int result=lua_pcall(state,arguments,results,0);
    lua_sethook(state,nullptr,0,0);
    if(result==LUA_OK)return true;
    const char* detail=lua_tostring(state,-1);
    error=std::string("Mission Lua: ")+(detail?detail:"unknown error");
    logging::write(error.c_str());lua_settop(state,0);return false;
}
std::string Text(lua_State* state,int table,const char* field){
    lua_getfield(state,table,field);
    if(lua_type(state,-1)!=LUA_TSTRING)throw std::runtime_error(std::string("Expected string: ")+field);
    std::string value=lua_tostring(state,-1);lua_pop(state,1);
    if(value.empty()||value.size()>512)throw std::runtime_error(std::string("Invalid string: ")+field);
    return value;
}
double Number(lua_State* state,int table,const char* field,double low,double high){
    lua_getfield(state,table,field);
    if(lua_type(state,-1)!=LUA_TNUMBER)throw std::runtime_error(std::string("Expected number: ")+field);
    double value=lua_tonumber(state,-1);lua_pop(state,1);
    if(!std::isfinite(value)||value<low||value>high)throw std::runtime_error(std::string("Out of range: ")+field);
    return value;
}
int Integer(lua_State* state,int table,const char* field,int low,int high){
    double value=Number(state,table,field,low,high);
    if(std::floor(value)!=value)throw std::runtime_error(std::string("Expected integer: ")+field);
    return int(value);
}
void Context(lua_State* state){
    lua_newtable(state);
    auto number=[&](const char* key,double value){lua_pushnumber(state,value);lua_setfield(state,-2,key);};
    number("active",game::activeMission+1);number("step",game::missionStep+1);number("time",game::missionTime);
    number("health",game::health);number("x",game::player.x);number("z",game::player.z);
    const char* vehicle="foot";
    if(game::occupied>=0&&game::occupied<int(game::vehicles.size())){
        switch(game::vehicles[game::occupied].kind){
        case game::Kind::Car:case game::Kind::SportCar:vehicle="car";break;
        case game::Kind::Bike:vehicle="bike";break;
        case game::Kind::Boat:vehicle="boat";break;
        default:vehicle="other";break;
        }
    }
    lua_pushstring(state,vehicle);lua_setfield(state,-2,"vehicle");
    lua_newtable(state);
    for(int i=0;i<int(game::missions.size());++i){lua_pushboolean(state,game::missionDone[i]);lua_rawseti(state,-2,i+1);}
    lua_setfield(state,-2,"done");
}
lua_State* Call(const char* function){
    if(!runtime)return nullptr;
    auto* state=runtime->state;lua_settop(state,0);
    lua_getglobal(state,"mission_module");lua_getfield(state,-1,function);lua_remove(state,-2);
    Context(state);return state;
}
void Apply(lua_State* state){
    try{
        if(!lua_istable(state,-1))throw std::runtime_error("callback must return a transition table");
        int active=Integer(state,1,"active",0,int(game::missions.size()))-1;
        int step=Integer(state,1,"step",1,31)-1;
        float time=float(Number(state,1,"time",0,3600));
        std::string event=Text(state,1,"event");
        const std::set<std::string> events={"none","start","locked","checkpoint","hit","fail","complete"};
        if(!events.count(event))throw std::runtime_error("unknown transition event");
        if(active>=0&&step>int(game::missions[active].goals.size()))throw std::runtime_error("invalid goal index");
        if(active>=0&&step==int(game::missions[active].goals.size())&&event!="complete")
            throw std::runtime_error("goal index past the active mission");
        std::string message,banner,detail;
        if(event!="none"&&event!="complete"){
            message=Text(state,1,"message");banner=Text(state,1,"banner");detail=Text(state,1,"detail");
        }
        if(event=="complete"){
            if(game::activeMission<0||active!=game::activeMission)throw std::runtime_error("completion without the current active mission");
            game::missionTime=time;
            game::completeMission();
        }else{
            game::activeMission=active;game::missionStep=step;game::missionTime=time;
        }
        if(event!="none"&&event!="complete"){
            game::announce(message,event=="start"?5.0f:3.0f);
            if(banner!="-")game::missionCard(banner,detail,event=="fail"?4.0f:2.5f);
            audio::play(event=="fail"?audio::Effect::Fail:event=="hit"?audio::Effect::Hit:audio::Effect::Pickup);
        }
        error.clear();
    }catch(const std::exception& exception){error=std::string("Mission Lua: ")+exception.what();logging::write(error.c_str());}
    lua_settop(state,0);
}
void Event(float dt,const char* event){
    auto* state=Call("update");if(!state)return;
    lua_pushnumber(state,dt);lua_pushstring(state,event);
    if(Protected(state,3,1))Apply(state);
}
}
bool Load(const char* path,std::vector<game::MissionDef>& definitions){
    error.clear();auto candidate=std::make_unique<Runtime>();auto* state=candidate->state;
    if(!state){error="Cannot allocate mission Lua runtime";return false;}
    luaL_requiref(state,"_G",luaopen_base,1);lua_pop(state,1);
    luaL_requiref(state,LUA_MATHLIBNAME,luaopen_math,1);lua_pop(state,1);
    luaL_requiref(state,LUA_TABLIBNAME,luaopen_table,1);lua_pop(state,1);
    luaL_requiref(state,LUA_STRLIBNAME,luaopen_string,1);lua_pop(state,1);
    for(const char* name:{"dofile","loadfile","load","collectgarbage"}){lua_pushnil(state);lua_setglobal(state,name);}
    std::string file=path?path:data_file::resourcePath("missions.lua");
    if(luaL_loadfilex(state,file.c_str(),"t")!=LUA_OK){error=file+": "+lua_tostring(state,-1);return false;}
    if(!Protected(state,0,1))return false;
    try{
        if(!lua_istable(state,1))throw std::runtime_error("script must return a module table");
        Number(state,1,"version",1,1);
        for(const char* function:{"start","start_candidate","update","objective","is_target","next"}){
            lua_getfield(state,1,function);bool valid=lua_isfunction(state,-1);lua_pop(state,1);
            if(!valid)throw std::runtime_error(std::string("Missing callback: ")+function);
        }
        lua_getfield(state,1,"missions");
        if(!lua_istable(state,2))throw std::runtime_error("Missing missions table");
        size_t count=lua_rawlen(state,2);
        if(count<1||count>game::missionDone.size())throw std::runtime_error("Expected 1-10 missions");
        candidate->names.resize(count);std::vector<game::MissionDef> parsed;std::set<std::string> ids;
        for(size_t i=0;i<count;++i){
            lua_rawgeti(state,2,i+1);if(!lua_istable(state,3))throw std::runtime_error("Invalid mission entry");
            game::MissionDef mission{};
            mission.id=Text(state,3,"id");
            if(!data_file::validId(mission.id)||!ids.insert(mission.id).second)throw std::runtime_error("Invalid/duplicate mission ID");
            candidate->names[i]=Text(state,3,"name");mission.name=candidate->names[i].c_str();
            mission.seconds=float(Number(state,3,"seconds",1,3600));
            double reward=Number(state,3,"reward",0,1000000);
            if(std::floor(reward)!=reward)throw std::runtime_error("Reward must be an integer");
            mission.reward=int(reward);
            std::string kind=Text(state,3,"kind");
            const std::vector<std::string> kinds={"drive","collect","boat","targets","bike","finale"};
            auto found=std::find(kinds.begin(),kinds.end(),kind);
            if(found==kinds.end())throw std::runtime_error("Unknown legacy mission category");
            mission.kind=game::MissionKind(found-kinds.begin());
            auto point=[&](int table){return game::Vec2{float(Number(state,table,"x",0,16800)),float(Number(state,table,"z",0,16800))};};
            lua_getfield(state,3,"start");if(!lua_istable(state,4))throw std::runtime_error("Missing start point");
            mission.start=point(4);lua_pop(state,1);
            lua_getfield(state,3,"goals");if(!lua_istable(state,4))throw std::runtime_error("Missing goals");
            size_t goals=lua_rawlen(state,4);if(goals<1||goals>30)throw std::runtime_error("Expected 1-30 goals");
            for(size_t g=0;g<goals;++g){lua_rawgeti(state,4,g+1);if(!lua_istable(state,5))throw std::runtime_error("Invalid goal");mission.goals.push_back(point(5));lua_pop(state,1);}
            lua_pop(state,2);parsed.push_back(std::move(mission));
        }
        lua_pop(state,1);lua_setglobal(state,"mission_module");
        runtime=std::move(candidate);definitions=std::move(parsed);return true;
    }catch(const std::exception& exception){error=file+": "+exception.what();return false;}
}
const std::string& LastError(){return error;}
void Start(){auto* state=Call("start");if(state&&Protected(state,1,1))Apply(state);}
void Update(float dt){Event(dt,"tick");}
void TargetHit(){Event(0,"target_hit");}
bool IsTarget(){
    auto* state=Call("is_target");if(!state||!Protected(state,1,1))return false;
    bool valid=lua_isboolean(state,1),result=valid&&lua_toboolean(state,1)!=0;
    if(!valid){error="Mission Lua: is_target must return a boolean";logging::write(error.c_str());}
    lua_settop(state,0);return result;
}
const char* Objective(){
    auto* state=Call("objective");if(!state||!Protected(state,1,1))return "Mission script unavailable";
    size_t size=0;const char* text=lua_type(state,1)==LUA_TSTRING?lua_tolstring(state,1,&size):nullptr;
    if(!text||size==0||size>512){error="Mission Lua: objective must return a nonempty string of at most 512 bytes";logging::write(error.c_str());objective="Invalid mission objective";}
    else objective.assign(text,size);
    lua_settop(state,0);return objective.c_str();
}
namespace {
int MissionIndex(const char* function){
    auto* state=Call(function);if(!state||!Protected(state,1,1))return -1;
    lua_Integer value=lua_isinteger(state,1)?lua_tointeger(state,1):-1;
    if(value<0||value>lua_Integer(game::missions.size())){
        error=std::string("Mission Lua: ")+function+" must return an integer mission index (0 means none)";logging::write(error.c_str());value=0;
    }
    lua_settop(state,0);return int(value)-1;
}
}
int Next(){return MissionIndex("next");}
int StartCandidate(){return MissionIndex("start_candidate");}
}
