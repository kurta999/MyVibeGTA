#include "weather.h"
#include "data_file.h"
#include <algorithm>
#include <set>

namespace weather {
namespace {
std::vector<State> definitions{{"clear","CLEAR",0,0,1,{},90,false}};
std::string error;
int stateIndex=0;
float timeLeft=90;
}
bool load(const char* path){
    error.clear();data_file::Ini file;
    if(!file.load(path?path:data_file::resourcePath("weather.ini"))||!file.version(1)){
        error=file.lastError();return false;
    }
    int count=0;
    if(!file.integer("Weather","Count",count,1,12)){
        error=file.lastError();return false;
    }
    std::vector<State> parsed;std::set<std::string> ids;
    for(int index=0;index<count;++index){
        State state{};int snow=0;
        std::string section="Weather"+std::to_string(index);
        if(!file.string(section,"Id",state.id)||
           !file.string(section,"Name",state.name)||
           !file.real(section,"CloudCover",state.clouds,0,1)||
           !file.real(section,"Precipitation",state.precipitation,0,1)||
           !file.real(section,"Visibility",state.visibility,0.25f,1)||
           !file.real(section,"WindX",state.wind.x,-2,2)||
           !file.real(section,"WindZ",state.wind.z,-2,2)||
           !file.real(section,"Duration",state.duration,10,600)||
           !file.integer(section,"Snow",snow,0,1)){
            error=file.lastError();return false;
        }
        if(!data_file::validId(state.id)||!ids.insert(state.id).second){
            error="Invalid or duplicate ["+section+"] Id";return false;
        }
        state.snow=snow!=0;
        if(state.snow&&state.precipitation<=0){
            error="Snow state needs precipitation in ["+section+"]";return false;
        }
        parsed.push_back(state);
    }
    definitions=std::move(parsed);reset();return true;
}
const std::string& lastError(){return error;}
void reset(){stateIndex=0;timeLeft=definitions.front().duration;}
void update(float dt){
    if(dt<=0)return;
    timeLeft-=std::min(dt,0.25f);
    if(timeLeft<=0){stateIndex=(stateIndex+1)%int(definitions.size());
        timeLeft=definitions[stateIndex].duration;}
}
bool set(const std::string& id,float remaining){
    for(int index=0;index<int(definitions.size());++index)
        if(definitions[index].id==id){stateIndex=index;
            timeLeft=remaining>0?std::clamp(remaining,0.1f,definitions[index].duration):
                definitions[index].duration;
            return true;}
    return false;
}
const State& current(){return definitions[stateIndex];}
float remaining(){return timeLeft;}
const std::vector<State>& states(){return definitions;}
}
