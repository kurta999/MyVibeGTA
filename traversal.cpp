#include "traversal.h"
#include "data_file.h"
#include "game_internal.h"
#include "ui.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif
#include <algorithm>
#include <set>

namespace traversal {
std::vector<Ladder> ladders;
std::vector<ClimbTree> trees;
namespace {
std::string error;
std::vector<Ladder> definitions;
std::vector<ClimbTree> treeDefinitions;
float generatedLadderHeight=180,generatedLadderOffset=0.5f;
enum class State { None, Ladder, Tree } state=State::None;
int activeIndex=-1;
float climbHeight=0;
void setPose(game::Vec2 point,float height){
    game::player=point;game::playerY=height;
    game::playerVelocity={};game::playerVerticalSpeed=0;
    game::grounded=false;game::swimming=false;
#ifdef MINI_CITY_JOLT
    jolt_world::teleportCharacter(point,height);
#endif
}
}
bool load(const char* path){
    error.clear();data_file::Ini file;
    if(!file.load(path?path:data_file::resourcePath("traversal.ini"))||!file.version(1)){
        error=file.lastError();return false;
    }
    int ladderCount=0,treeCount=0;
    if(!file.integer("Ladders","Count",ladderCount,1,30)||
       !file.integer("ClimbTrees","Count",treeCount,1,50)||
       !file.real("GeneratedLadders","MinHeight",generatedLadderHeight,90,500)||
       !file.real("GeneratedLadders","Offset",generatedLadderOffset,0.15f,0.85f)){
        error=file.lastError();return false;
    }
    std::vector<Ladder> parsedLadders;std::vector<ClimbTree> parsedTrees;
    std::set<std::string> ids;
    for(int index=0;index<ladderCount;++index){
        std::string section="Ladder"+std::to_string(index);Ladder ladder{};
        if(!file.string(section,"Id",ladder.id)||
           !file.string(section,"BuildingId",ladder.buildingId)||
           !file.real(section,"Offset",ladder.offset,0.15f,0.85f)){
            error=file.lastError();return false;
        }
        if(!data_file::validId(ladder.id)||!data_file::validId(ladder.buildingId)||
           !ids.insert(ladder.id).second){error="Invalid ["+section+"] Id";return false;}
        parsedLadders.push_back(ladder);
    }
    for(int index=0;index<treeCount;++index){
        std::string section="ClimbTree"+std::to_string(index);ClimbTree tree{};
        if(!file.string(section,"Id",tree.id)||
           !file.string(section,"TreeId",tree.treeId)||
           !file.real(section,"Height",tree.height,12,55)){
            error=file.lastError();return false;
        }
        if(!data_file::validId(tree.id)||!data_file::validId(tree.treeId)||
           !ids.insert(tree.id).second){error="Invalid ["+section+"] Id";return false;}
        parsedTrees.push_back(tree);
    }
    definitions=std::move(parsedLadders);treeDefinitions=std::move(parsedTrees);
    reset();return true;
}
const std::string& lastError(){return error;}
void reset(){
    state=State::None;activeIndex=-1;climbHeight=0;
    ladders.clear();trees.clear();
    if(definitions.empty())return;
    error.clear();
    for(auto ladder:definitions){
        auto found=std::find_if(game::buildings.begin(),game::buildings.end(),
            [&](const game::Building& building){return building.id==ladder.buildingId;});
        if(found==game::buildings.end()||found->h<100){
            error="Missing or short building for ladder "+ladder.id;continue;
        }
        ladder.bottom={found->x+found->w*ladder.offset,found->z-16};
        ladder.roof={ladder.bottom.x,found->z+22};
        ladder.height=found->h;
        if(game::solid(ladder.bottom,11)){
            error="Blocked ladder base "+ladder.id;continue;
        }
        ladders.push_back(ladder);
    }
    for(const auto& building:game::buildings){
        if(building.h<generatedLadderHeight)continue;
        bool authored=std::any_of(definitions.begin(),definitions.end(),
            [&](const Ladder& ladder){return ladder.buildingId==building.id;});
        if(authored)continue;
        Ladder ladder{};ladder.id="climb-"+building.id;
        ladder.buildingId=building.id;ladder.offset=generatedLadderOffset;
        ladder.bottom={building.x+building.w*ladder.offset,building.z-16};
        ladder.roof={ladder.bottom.x,building.z+22};ladder.height=building.h;
        if(!game::solid(ladder.bottom,11))ladders.push_back(ladder);
    }
    for(auto tree:treeDefinitions){
        auto found=std::find_if(game::trees.begin(),game::trees.end(),
            [&](const game::Tree& existing){return existing.id==tree.treeId;});
        if(found==game::trees.end()){
            error="Missing climb tree "+tree.id;continue;
        }
        tree.bottom=found->p+game::Vec2{0,-13};
        if(game::solid(tree.bottom,11)){
            error="Blocked climb tree "+tree.id;continue;
        }
        tree.treeIndex=int(std::distance(game::trees.begin(),found));
        trees.push_back(tree);
    }
    for(int index=0;index<int(game::trees.size());++index){
        const auto& source=game::trees[index];
        if(source.climbHeight<=0)continue;
        ClimbTree tree{};tree.id="climb-"+source.id;tree.treeId=source.id;
        tree.bottom=source.p+game::Vec2{0,-13};
        tree.height=source.climbHeight;tree.treeIndex=index;tree.generated=true;
        if(!game::solid(tree.bottom,11))trees.push_back(tree);
    }
}
bool active(){return state!=State::None;}
bool startLadder(int index){
    if(active()||index<0||index>=int(ladders.size())||game::health<=0||
       game::occupied>=0||game::carryingBody()||
       game::len(game::player-ladders[index].bottom)>48)return false;
    state=State::Ladder;activeIndex=index;climbHeight=0;
    setPose(ladders[index].bottom,0);return true;
}
bool startTree(int index){
    if(active()||index<0||index>=int(trees.size())||game::health<=0||
       game::occupied>=0||game::carryingBody()||
       game::len(game::player-trees[index].bottom)>43)return false;
    int sourceIndex=trees[index].treeIndex;
    if(sourceIndex<0||sourceIndex>=int(game::trees.size())||
       game::trees[sourceIndex].destroyed)return false;
    state=State::Tree;activeIndex=index;climbHeight=0;
    setPose(trees[index].bottom,0);return true;
}
void detach(){state=State::None;activeIndex=-1;climbHeight=0;}
void update(float dt){
    if(!active())return;
    if(game::health<=0){detach();return;}
    float direction=float(game::keys[ui::bindings[int(ui::Action::Forward)]])-
        float(game::keys[ui::bindings[int(ui::Action::Backward)]]);
    climbHeight=std::clamp(climbHeight+direction*95*dt,0.0f,
        state==State::Ladder?ladders[activeIndex].height:trees[activeIndex].height);
    if(state==State::Ladder){
        const Ladder& ladder=ladders[activeIndex];
        if(climbHeight>=ladder.height){
            setPose(ladder.roof,ladder.height+2);
            detach();return;
        }
        setPose(ladder.bottom,climbHeight);
    }else{
        const ClimbTree& tree=trees[activeIndex];
        if(tree.treeIndex<0||tree.treeIndex>=int(game::trees.size())||
           game::trees[tree.treeIndex].destroyed){detach();return;}
        setPose(tree.bottom,climbHeight);
        if(game::keys[VK_SPACE]){
            game::Vec2 exit=tree.bottom+game::forward(game::cameraYaw)*19;
            if(!game::solid(exit,11))setPose(exit,climbHeight+5);
            detach();
        }
    }
}
}
