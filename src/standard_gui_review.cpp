#include "app_previews.h"
#include "builder.h"
#include "data_file.h"
#include "dx11_assets.h"
#include "game.h"
#include "imgui_menus.h"
#include "input.h"
#include "logging.h"
#include "mission_script.h"
#include "regions.h"
#include "savegame.h"
#include "traffic.h"
#include "ui.h"
#include <cstdio>
#include <filesystem>

namespace app {
namespace {
void press(int key){
    input::windowProc(game::win,WM_KEYDOWN,key,0);
    input::windowProc(game::win,WM_KEYUP,key,0);
}

bool check(bool passed,const char* detail){
    logging::write((std::string("Standard GUI review: ")+detail+(passed?" PASS":" FAIL")).c_str());
    return passed;
}

struct ReviewFiles {
    std::filesystem::path save,settings;
    ~ReviewFiles(){
        savegame::flush();
        std::error_code error;
        std::filesystem::remove(save,error);
        std::filesystem::remove(settings,error);
    }
};
}

bool RunStandardGuiReview(){
    using namespace game;
    const auto directory=std::filesystem::path(data_file::resourcePath("world.ini")).parent_path().parent_path();
    const auto save=directory/"savegame.ini",settings=directory/"settings.ini";
    if(!check(!directory.empty()&&!std::filesystem::exists(save)&&!std::filesystem::exists(settings),
              "isolated runtime"))return false;
    const ReviewFiles files{save,settings};
    if(!check(!menus::Enabled()&&nextMission()==0&&interactionPrompt().find("START Sunset Circuit")!=std::string::npos,
              "fresh spawn and standard menus"))return false;

    press(VK_ESCAPE);
    press(VK_DOWN);
    press(VK_RETURN);
    render();
    std::vector<unsigned char> hud(std::size_t(screenW)*screenH*4);
    dx11::buildHud(hud.data(),screenW,screenH);
    if(!check(ui::page==ui::Page::Graphics&&hud[0]==38&&hud[1]==29&&hud[2]==20&&hud[3]==255,
              "standard Graphics page is drawn"))return false;
    press(VK_ESCAPE);
    press(VK_ESCAPE);

    press('F');
    if(!check(activeMission==0&&std::string(missionObjective())=="Get in a car to continue.",
              "mission start and vehicle hint"))return false;
    missionTime=.005f;
    update(1.0f/60.0f);
    if(!check(activeMission==-1&&nextMission()==0&&interactionPrompt().find("RETRY Sunset Circuit")!=std::string::npos,
              "timeout keeps the retry waypoint"))return false;

    press(ui::bindings[int(ui::Action::Interact)]);
    for(int tick=0;tick<45;++tick)update(1.0f/60.0f);
    if(!check(occupied>=0&&vehicles[occupied].id=="starter-car","starter car entry"))return false;
    press('F');
    if(!check(activeMission==0&&missionStep==0,"retry from the driver's seat"))return false;
    const auto& circuit=missions[0];
    const auto route=traffic::roadRoute(circuit.goals[0],circuit.goals[2]);
    bool followsRoads=route.size()>=4;
    for(std::size_t i=1;i<route.size();++i){
        const auto mid=(route[i-1]+route[i])*.5f;
        followsRoads&=regions::downtownRoadAt(mid);
    }
    if(!check(followsRoads,"mission route uses the road network"))return false;
    const int reward=circuit.reward;
    const int startingMoney=money;
    const int accelerate=ui::bindings[int(ui::Action::Forward)];
    const auto startPosition=player;
    input::windowProc(win,WM_KEYDOWN,accelerate,0);
    for(int tick=0;tick<480&&activeMission==0&&missionStep==0;++tick)update(1.0f/60.0f);
    input::windowProc(win,WM_KEYUP,accelerate,0);
    if(activeMission!=0||missionStep!=1){
        char details[180]{};
        std::snprintf(details,sizeof(details),"Starter drive: %.1f,%.1f -> %.1f,%.1f, vehicle %d, health %.1f",
            startPosition.x,startPosition.z,player.x,player.z,occupied,health);
        logging::write(details);
    }
    if(!check(activeMission==0&&missionStep==1,"drive through the first checkpoint"))return false;
    for(std::size_t index=1;index<circuit.goals.size();++index){
        player=circuit.goals[index];
        mission_script::Update(1.0f/60.0f);
    }
    if(!check(missionDone[0]&&nextMission()==1&&money==startingMoney+reward&&
              message.find("Beach Cache")!=std::string::npos,"completion reward and next mission"))return false;

    auto pauseAction=[](int row){
        press(VK_ESCAPE);
        for(int index=0;index<row;++index)press(VK_DOWN);
        press(VK_RETURN);
    };
    pauseAction(4);
    if(!check(message=="Game saved.","save through the standard pause menu"))return false;
    money=0;
    pauseAction(5);
    if(!check(message=="Saved game loaded."&&missionDone[0]&&nextMission()==1&&money==startingMoney+reward,
              "load preserves the mission handoff"))return false;

    press(VK_F5);
    if(!check(FinishBuilderTransition()&&builder::active(),"enter builder mode"))return false;
    press('E');
    if(!check(builder::inventoryOpen(),"open standard builder inventory"))return false;
    render();
    press(VK_ESCAPE);
    press(VK_F5);
    return check(FinishBuilderTransition()&&!builder::active()&&nextMission()==1,"exit builder mode");
}
} // namespace app
