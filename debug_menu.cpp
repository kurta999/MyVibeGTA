#include "debug_menu.h"
#include "game.h"
#include "weapons.h"
#include "traversal.h"
#include "jolt_world.h"
#include <algorithm>

namespace debug_menu {
bool open=false;
bool godMode=false;
bool flyMode=false;
int selection=0;

int entryCount(){return 3+weapons::count();}
void reset(){open=false;godMode=false;flyMode=false;selection=0;}
void toggle(){open=!open;selection=std::clamp(selection,0,std::max(0,entryCount()-1));}

void handleKey(int key){
    if(!open)return;
    if(key==VK_ESCAPE||key==VK_F4){open=false;return;}
    if(key==VK_UP){selection=(selection+entryCount()-1)%entryCount();return;}
    if(key==VK_DOWN){selection=(selection+1)%entryCount();return;}
    if(key==VK_PRIOR){selection=std::max(0,selection-8);return;}
    if(key==VK_NEXT){selection=std::min(entryCount()-1,selection+8);return;}
    if(key!=VK_RETURN&&key!=VK_SPACE&&key!=VK_LEFT&&key!=VK_RIGHT)return;
    if(selection==0){godMode=!godMode;return;}
    if(selection==1){
        if(game::occupied>=0||game::enteringVehicle>=0||traversal::active()){
            game::message="Fly mode requires being on foot and off a ladder.";
            game::messageTime=3;
            return;
        }
        flyMode=!flyMode;
        game::playerVerticalSpeed=0;
        game::airTime=0;
        game::grounded=false;
        jolt_world::teleportCharacter(game::player,game::playerY);
        return;
    }
    if(selection==2){game::health=game::PLAYER_MAX_HEALTH;return;}
    int chosen=selection-3;
    if(chosen<0||chosen>=weapons::count())return;
    const auto& stats=weapons::stats(chosen);
    game::unlocked[chosen]=true;
    game::magazine[chosen]=stats.melee?1:stats.magazine;
    game::ammo[chosen]=stats.melee?-1:std::max(game::ammo[chosen],stats.magazine*10);
    game::weapon=chosen;
    open=false;
}
}
