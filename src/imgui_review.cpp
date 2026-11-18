#include "app_previews.h"
#ifdef MINI_CITY_IMGUI
#include "builder.h"
#include "commerce.h"
#include "debug_menu.h"
#include "game.h"
#include "imgui_menus.h"
#include "input.h"
#include "jolt_world.h"
#include "logging.h"
#include "ordnance.h"
#include "police.h"
#include "savegame.h"
#include "ui.h"
#include "weapons.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>

namespace app {
bool RunImGuiReview(){
    using namespace game;
    struct ReviewFiles {
        struct File {std::filesystem::path path;std::string bytes;bool exists;};
        std::vector<File> files;
        std::filesystem::path directory;
        ReviewFiles(){
            char executable[MAX_PATH]{};GetModuleFileNameA(nullptr,executable,MAX_PATH);
            directory=std::filesystem::path(executable).parent_path();
            for(const char* name:{"savegame.ini","settings.ini"}){
                auto path=directory/name;
                std::ifstream input(path,std::ios::binary);
                files.push_back({path,std::string(std::istreambuf_iterator<char>(input),{}),bool(input)});
            }
        }
        ~ReviewFiles(){
            savegame::flush();
            for(const auto& file:files){
                if(file.exists){std::ofstream output(file.path,std::ios::binary|std::ios::trunc);output<<file.bytes;}
                else{std::error_code error;std::filesystem::remove(file.path,error);}
            }
        }
    } preserveFiles;
    auto folder=std::filesystem::current_path()/("imgui-review-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    menus::EnableReview();
    auto capture=[&](const std::string& name){
        std::set<std::filesystem::path> before;
        auto screenshots=preserveFiles.directory/"screenshots";
        if(std::filesystem::exists(screenshots))for(const auto& file:std::filesystem::directory_iterator(screenshots))before.insert(file.path());
        for(int frame=0;frame<4;++frame)render();requestScreenshot();render();
        if(std::filesystem::exists(screenshots))for(const auto& file:std::filesystem::directory_iterator(screenshots))
            if(file.path().extension()==".png"&&!before.count(file.path())){
                std::filesystem::rename(file.path(),folder/(name+".png"));return true;
            }
        return false;
    };
    bool ok=true;
    auto check=[&](bool passed,const char* label){
        logging::write((std::string("ImGui widget ")+label+(passed?": PASS":": FAIL")).c_str());ok&=passed;
    };
    auto click=[&](const std::string& label,bool right=false,bool shift=false){
        for(int frame=0;frame<3;++frame)render();
        bool found=menus::ReviewPointer(label,true,right,shift);render();
        menus::ReviewPointer(label,false,right,shift);for(int frame=0;frame<3;++frame)render();
        return found;
    };
    for(auto page:{ui::Page::Main,ui::Page::Graphics,ui::Page::Controls,ui::Page::Audio}){
        ui::page=page;ok&=capture("pause-"+std::to_string(int(page)));
    }
    ui::page=ui::Page::Closed;ui::showHelp=true;ok&=capture("help");ui::showHelp=false;
    ui::page=ui::Page::Main;
    check(click("Graphics")&&ui::page==ui::Page::Graphics,"open graphics");
    check(click("Scene quality")&&click("Scene quality/Low")&&ui::graphicsQuality==0,"graphics combo");
    check(click("Scene quality")&&click("Scene quality/High")&&ui::graphicsQuality==2,"graphics combo restore");
    ui::windowChoice=0;ui::windowMode=0;ui::applyWindow();
    check(screenW==1280&&screenH==720,"720p resize");ok&=capture("graphics-720p");
    check(click("Back")&&ui::page==ui::Page::Main,"graphics back");
    ui::windowChoice=2;ui::applyWindow();
    check(click("Controls")&&ui::page==ui::Page::Controls,"open controls");
    bool inverted=ui::invertY;
    check(click("Invert vertical mouse")&&ui::invertY!=inverted,"controls checkbox");
    auto key=ui::bindings[0];check(click(ui::keyName(key))&&ui::waitingForBinding==0,"start rebinding");
    input::windowProc(win,WM_KEYDOWN,key,0);input::windowProc(win,WM_KEYUP,key,0);
    check(ui::waitingForBinding==-1&&ui::bindings[0]==key,"key binding through Win32 input");
    check(click("Back")&&click("Audio")&&ui::page==ui::Page::Audio,"open audio");
    int volume=ui::masterVolume;
    check(click("Master volume")&&ui::masterVolume!=volume,"audio slider");
    check(click("Back")&&click("Resume")&&ui::page==ui::Page::Closed,"resume");
    ui::page=ui::Page::Closed;
    occupied=-1;money=100000;player=commerce::shops[0].p;
    commerce::openShop(0);check(commerce::menu()==commerce::Menu::Shop,"open shop");ok&=capture("shop");
    auto entryLabel=[](const commerce::MenuEntry& entry){return entry.label+(entry.price>0?"  $"+std::to_string(entry.price):"");};
    for(int i=0;i<std::min(8,int(commerce::items.size()));++i){
        auto entry=commerce::menuEntries()[i];if(!entry.available)continue;
        int cash=money;check(click(entryLabel(entry))&&money==cash-entry.price,"shop purchase");break;
    }
    check(click("Close")&&commerce::menu()==commerce::Menu::None,"shop close");
    player=commerce::houses[0].p;commerce::openHouse(0);ok&=capture("house");commerce::close();
    occupied=-1;money=100000;player=commerce::houses[0].p;
    commerce::openHouse(0);check(click(entryLabel(commerce::menuEntries()[0]))&&commerce::houses[0].owned,"buy house");
    commerce::houses[1].owned=true;
    ok&=capture("house-owned-garage-travel");
    vehicles[0].owned=true;vehicles[0].p=commerce::houses[0].p;vehicles[0].exploded=false;
    check(click(entryLabel(commerce::menuEntries()[0]))&&vehicles[0].garageHouseId==commerce::houses[0].id,"store vehicle");
    police::setWantedLevel(0);activeMission=-1;
    check(click(entryLabel(commerce::menuEntries()[1]))&&commerce::menu()==commerce::Menu::None,"fast travel");
    debug_menu::open=true;ok&=capture("debug");debug_menu::open=false;
    debug_menu::open=true;auto cash=money;
    check(click("Add $100,000")&&money==cash+100000,"debug cash");
    check(click("Close")&&!debug_menu::open,"debug close");
    occupied=-1;player={300,235};playerY=groundHeight(player);grounded=true;swimming=false;
    weapon=weapons::indexOf("timed-bomb");unlocked[weapon]=true;magazine[weapon]=1;fireCooldown=0;
    shoot();ok&=ordnance::timerOpen()&&capture("bomb-timer");
    check(click("Cancel")&&!ordnance::timerOpen()&&magazine[weapon]==1,"timer cancel");
    shoot();ordnance::setTimerSeconds(5);
    check(click("Place and arm")&&!ordnance::timerOpen()&&magazine[weapon]==0,"timer arm");
    ok&=builder::requestToggle()&&FinishBuilderTransition();
    builder::addItem(builder::itemIndex("granite"),32);builder::addItem(builder::itemIndex("iron-axe"),1);
    builder::addItem(builder::itemIndex("log"),20);builder::handleKey('E');
    ok&=capture("inventory-crafting");
    auto stack=builder::inventory()[0];
    check(click("slot-0")&&builder::cursor().count==stack.count&&builder::inventory()[0].item==-1,"inventory pick up");
    check(click("slot-0")&&builder::cursor().item==-1&&builder::inventory()[0].count==stack.count,"inventory place");
    check(click("slot-0",true)&&builder::cursor().count==(stack.count+1)/2,"inventory split");
    check(click("slot-0")&&builder::cursor().item==-1&&builder::inventory()[0].count==stack.count,"inventory merge");
    const auto& recipe=builder::recipes()[0];
    auto countItem=[](int item){int count=0;for(const auto& stack:builder::inventory())if(stack.item==item)count+=stack.count;return count;};
    int priorResult=countItem(recipe.result);
    check(click(builder::items()[recipe.result].name+" x"+std::to_string(recipe.count))&&countItem(recipe.result)==priorResult+recipe.count,"craft recipe");
    check(click("Close inventory (E / Escape)")&&!builder::inventoryOpen(),"inventory close");
    builder::Cell cell{};bool placed=false;
    for(int dx=-3;dx<=3&&!placed;++dx)for(int dz=-3;dz<=3&&!placed;++dz){
        cell=builder::cellAt({player.x+dx*40,0,player.z+dz*40});placed=builder::place(cell,builder::itemIndex("chest"),false);
    }
    check(placed,"place chest fixture");
    auto low=builder::cellLow(cell);player=previousPlayer={low.x-35,low.z+10};playerY=low.y;cameraYaw=0;cameraPitch=-std::atan2(30.0f,45.0f);
    jolt_world::teleportCharacter(player,playerY);builder::use();
    check(builder::inventoryOpen()&&builder::chest(),"open chest");
    if(builder::chest()){
        ok&=capture("inventory-chest");auto stored=builder::inventory()[0].count;
        ui::windowChoice=0;ui::applyWindow();ok&=capture("inventory-chest-720p");
        check(click("slot-0",false,true)&&builder::inventory()[0].item==-1&&(*builder::chest())[0].count==stored,"shift transfer to chest");
        check(click("slot-36",false,true)&&(*builder::chest())[0].item==-1&&builder::inventory()[0].count==stored,"shift transfer from chest");
        check(click("Close inventory (E / Escape)")&&!builder::inventoryOpen(),"chest close");
        ui::windowChoice=2;ui::applyWindow();
    }
    ok&=builder::requestToggle()&&FinishBuilderTransition();
    ui::page=ui::Page::Main;check(click("Save game")&&ui::page==ui::Page::Closed&&message=="Game saved.","save game");
    int savedCash=money;money=1;ui::page=ui::Page::Main;
    check(click("Load game")&&ui::page==ui::Page::Closed&&money==savedCash,"load game");
    logging::write((std::string("ImGui native menu review: ")+(ok?"PASS ":"FAIL ")+folder.string()).c_str());
    return ok;
}
} // namespace app
#endif
