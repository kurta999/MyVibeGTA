#include "app_previews.h"
#include "game.h"
#include "game_internal.h"
#include "ai.h"
#include "birds.h"
#include "builder.h"
#include "builder_feedback.h"
#include "camera.h"
#include "commerce.h"
#include "debug_menu.h"
#include "destruction.h"
#include "dx11_assets.h"
#include "excavation.h"
#include "fire.h"
#include "grapple.h"
#include "input.h"
#include "jolt_world.h"
#include "logging.h"
#include "ordnance.h"
#include "ped_navigation.h"
#include "ped_navigation_surface.h"
#include "physics.h"
#include "police.h"
#include "regions.h"
#include "savegame.h"
#include "scenery_edits.h"
#include "surface_work.h"
#include "terrain.h"
#include "traversal.h"
#include "ui.h"
#include "vehicle_systems.h"
#include "weapons.h"
#include "wildlife.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace app {
using namespace game;

bool FinishBuilderTransition(){
    const bool expected=!builder::active();
    for(int tick=0;tick<5000&&builder::transitioning();++tick){
        builder::advance(1.0f/60);
        if(builder::transitioning())Sleep(1);
    }
    return !builder::transitioning()&&builder::active()==expected;
}

namespace {
bool prepareDenseBuilder(){
    std::ostringstream records;
    records<<"Version=2\n";
    const auto origin=builder::cellAt({4310,0,3590});
    int count=0;
    for(int x=0;x<32;++x)for(int z=0;z<24;++z){
        if(x%8>=5||z%8>=5)continue;
        const auto cell=builder::cellLow({origin.x+x,0,origin.z+z});
        const int floor=int(std::ceil(terrain::baseHeight({cell.x+10,cell.z+10})/builder::BLOCK_SIZE));
        const int height=4+(x/8+z/8)%4;
        for(int y=0;y<height;++y){
            records<<"Record"<<count++<<"=P "<<origin.x+x<<' '<<floor+y<<' '<<origin.z+z<<" granite 0";
            for(int slot=0;slot<27;++slot)records<<" none 0 0";
            records<<'\n';
        }
    }
    if(!builder::restore(records.str()))return false;
    player=previousPlayer={4210,3820};
    playerY=terrain::baseHeight(player);
    cameraYaw=0;cameraPitch=-.08f;
    cameraMode=CameraMode::ThirdNear;
    jolt_world::teleportCharacter(player,playerY);
    if(!builder::requestToggle()||!FinishBuilderTransition())return false;
    logging::write(("Benchmark dense builder: "+std::to_string(count)+" placed blocks").c_str());
    return true;
}
}

bool PreparePreviews(const char* commandLine,bool benchmark,bool savedBuilderPreview,bool& finished){
    if(std::strstr(commandLine,"--weapon-icons-preview")){
        player=previousPlayer={4500,4500};playerY=50;cameraYaw=0;cameraPitch=-.4f;
        cameraMode=CameraMode::FirstWide;pickups.clear();ui::grassDistance=0;
        for(int i=0;i<weapons::count();++i)
            pickups.push_back({{4600+float(i/5)*40,4500+float(i%5-2)*36},i,true,0,"icon-preview-"+std::to_string(i)});
        weapon=weapons::indexOf("minigun");debug_menu::flyMode=true;
        jolt_world::teleportCharacter(player,playerY);
    }
    if(std::strstr(commandLine,"--expansion-preview")){
        player=previousPlayer={4300,4500};playerY=160;cameraYaw=0;cameraPitch=-.45f;
        cameraMode=CameraMode::FirstWide;occupied=-1;rightMouse=false;ui::grassDistance=0;
        vehicles.clear();peds.clear();pickups.clear();trees.clear();props.clear();buildings.clear();
        const Kind kinds[]={Kind::Skateboard,Kind::Bicycle,Kind::Tractor,Kind::Combine,Kind::Tank,Kind::Truck,Kind::Trailer,Kind::Airplane};
        for(int n=0;n<8;++n){Vehicle v{};v.kind=kinds[n];v.id="preview-"+std::to_string(n);v.angle=PI/2;
            v.p={4630+float(n/4)*190,4350+float(n%4)*100};v.c=n%2?rgb(206,168,56):rgb(64,144,172);vehicles.push_back(v);}
        jolt_world::reset();for(int tick=0;tick<45;++tick)jolt_world::step(1.0f/60);
        debug_menu::flyMode=true;jolt_world::teleportCharacter(player,playerY);
    }
    if(std::strstr(commandLine,"--combine-preview")){
        player=previousPlayer={4400,4680};playerY=35;cameraYaw=-1.06f;cameraPitch=-.17f;
        cameraMode=CameraMode::FirstWide;occupied=-1;rightMouse=false;ui::grassDistance=0;
        vehicles.clear();peds.clear();pickups.clear();trees.clear();props.clear();buildings.clear();
        Vehicle v{};v.kind=Kind::Combine;v.id="combine-preview";v.p={4500,4500};v.angle=PI/2;vehicles.push_back(v);
        jolt_world::reset();for(int tick=0;tick<45;++tick)jolt_world::step(1.0f/60);
        debug_menu::flyMode=true;jolt_world::teleportCharacter(player,playerY);
    }
    if(std::strstr(commandLine,"--destruction-preview")){
        player=previousPlayer={4500,4500};playerY=0;cameraYaw=.12f;cameraPitch=.03f;
        cameraMode=CameraMode::FirstWide;occupied=-1;rightMouse=false;ui::grassDistance=0;
        vehicles.clear();peds.clear();pickups.clear();trees.clear();props.clear();buildings.clear();
        buildings.push_back({4620,4430,45,155,130,rgb(170,159,140),"damage-preview"});
        Vehicle v{};v.kind=Kind::Car;v.id="blast-preview";v.p={4580,4540};v.c=rgb(194,70,50);vehicles.push_back(v);
        jolt_world::reset();destruction::blast({4620,20,4500},100);damageVehicle(0,10000);
        for(int tick=0;tick<12;++tick)jolt_world::step(1.0f/60);
        jolt_world::teleportCharacter(player,playerY);
    }
    if(std::strstr(commandLine,"--masonry-preview")){
        player=previousPlayer={4500,4500};playerY=0;cameraYaw=0;cameraPitch=-.08f;
        cameraMode=CameraMode::FirstWide;occupied=-1;rightMouse=false;ui::grassDistance=0;
        vehicles.clear();peds.clear();pickups.clear();trees.clear();props.clear();buildings.clear();
        buildings.push_back({4650,4420,55,160,140,rgb(175,151,129),"masonry-preview"});
        jolt_world::reset();destruction::blast({4650,40,4500},120);
        for(int tick=0;tick<180;++tick)jolt_world::step(1.0f/60);
        jolt_world::teleportCharacter(player,playerY);
    }
    if((std::strstr(commandLine,"--tank-surface-preview")||std::strstr(commandLine,"--truck-preview")||std::strstr(commandLine,"--trailer-preview"))){
        bool trailer=std::strstr(commandLine,"--trailer-preview")!=nullptr;
        player=previousPlayer=trailer?Vec2{4300,4800}:Vec2{4400,4680};playerY=trailer?75.0f:45.0f;
        cameraYaw=std::atan2(4500-player.z,4500-player.x);cameraPitch=trailer?-.15f:-.18f;
        cameraMode=CameraMode::FirstWide;occupied=-1;rightMouse=false;ui::grassDistance=0;
        vehicles.clear();peds.clear();pickups.clear();trees.clear();props.clear();buildings.clear();
        Vehicle v{};v.kind=trailer?Kind::Trailer:std::strstr(commandLine,"--truck-preview")?Kind::Truck:Kind::Tank;
        v.id="vehicle-surface-preview";v.p={4500,4500};v.angle=PI/2;vehicles.push_back(v);
        jolt_world::reset();for(int n=0;n<90;++n)jolt_world::step(1.0f/60);
        debug_menu::flyMode=true;jolt_world::teleportCharacter(player,playerY);
    }
    if(std::strstr(commandLine,"--tank-aim-preview")){
        player=previousPlayer={4500,4500};playerY=0;cameraYaw=.9f;cameraPitch=.25f;
        cameraMode=CameraMode::ThirdNear;rightMouse=false;ui::grassDistance=0;
        vehicles.clear();peds.clear();pickups.clear();trees.clear();props.clear();buildings.clear();
        Vehicle tank{};tank.kind=Kind::Tank;tank.p=player;tank.angle=0;tank.id="tank-aim-preview";vehicles.push_back(tank);
        jolt_world::reset();occupied=0;
        for(int tick=0;tick<90;++tick)jolt_world::step(1.0f/60);
        vehicle_systems::update(1.0f/60);playerY=vehicles[0].rideHeight;
        if(std::strstr(commandLine,"--tank-side-view")){
            occupied=-1;cameraYaw=-.5f;cameraPitch=.1f;playerY=0;
        }
    }
    if(std::strstr(commandLine,"--airplane-preview")){
        player=previousPlayer={4500,4900};playerY=0;cameraYaw=0;cameraPitch=0;
        cameraMode=CameraMode::ThirdFar;rightMouse=false;ui::grassDistance=0;
        vehicles.clear();peds.clear();pickups.clear();trees.clear();props.clear();buildings.clear();
        Vehicle v{};v.kind=Kind::Airplane;v.id="plane-preview";v.p=player;v.c=rgb(221,225,234);vehicles.push_back(v);
        occupied=0;jolt_world::reset();keys['W']=true;
        for(int tick=0;tick<120;++tick)update(1.0f/60);keys[VK_SPACE]=true;
        for(int tick=0;tick<60;++tick)update(1.0f/60);keys[VK_SPACE]=false;
        for(int tick=0;tick<90;++tick)update(1.0f/60);keys['W']=false;
        cameraYaw=2.5f;cameraPitch=-.14f;vehicleLookTime=30;previousPlayer=player;
    }
    if(std::strstr(commandLine,"--vehicle-scale-preview")){
        player=previousPlayer={4500,4500};playerY=0;cameraYaw=0;cameraPitch=0;
        cameraMode=CameraMode::ThirdNear;occupied=-1;rightMouse=false;ui::grassDistance=0;
        vehicles.clear();peds.clear();pickups.clear();
        for(Kind kind:{Kind::Car,Kind::SportCar,Kind::Helicopter}){
            Vehicle v{};v.kind=kind;v.id=kind==Kind::Car?"starter-car":"scale-preview";
            v.p=kind==Kind::Car?Vec2{4570,4430}:kind==Kind::SportCar?Vec2{4570,4520}:Vec2{4690,4610};
            v.angle=0;v.c=rgb(190,80,50);vehicles.push_back(v);
        }
        jolt_world::reset();
    }
    if(std::strstr(commandLine,"--helicopter-preview")){
        for(int i=0;i<int(vehicles.size());++i)if(vehicles[i].kind==Kind::Helicopter){
            jolt_world::teleportVehicle(i,{4500,4500},0);occupied=i;
            player=previousPlayer=vehicles[i].p;cameraYaw=0;
            keys[VK_SPACE]=true;
            for(int tick=0;tick<180;++tick)update(1.0f/60);
            keys[VK_SPACE]=false;
            for(int tick=0;tick<90;++tick)update(1.0f/60);
            cameraYaw=2.2f;cameraPitch=0;vehicleLookTime=30;
            cameraMode=CameraMode::ThirdNear;previousPlayer=player;break;
        }
    }
    if((std::strstr(commandLine,"--minigun-preview")||
        std::strstr(commandLine,"--shovel-preview"))){
        player=previousPlayer={300,250};playerY=0;cameraYaw=0;cameraPitch=0;
        cameraMode=CameraMode::ThirdNear;rightMouse=true;
        weapon=weapons::indexOf(std::strstr(commandLine,"--shovel-preview")?"shovel":"minigun");
        unlocked[weapon]=true;magazine[weapon]=weapons::stats(weapon).magazine;
        if(weapons::stats(weapon).melee){shoot();meleeVisualTime=.2f;}
        else {leftMouse=true;shoot();}
    }
    if(std::strstr(commandLine,"--grapple-preview")){
        player=previousPlayer={300,250};playerY=0;cameraYaw=.5f;cameraPitch=.45f;
        jolt_world::teleportCharacter(player,0);rightMouse=leftMouse=true;
        weapon=weapons::indexOf("grapple-hook");unlocked[weapon]=true;
        shoot();for(int tick=0;tick<24;++tick)update(1.0f/60);
        previousPlayer=player;
        logging::write(grapple::active()?"Grapple smoke: attached":"Grapple smoke: missed");
    }
    if(std::strstr(commandLine,"--ordnance-preview")){
        player=previousPlayer={4500,4500};playerY=0;cameraYaw=.45f;cameraPitch=0;
        cameraMode=CameraMode::ThirdNear;cameraYaw=0;ui::grassDistance=0;
        trees.clear();buildings.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();
        buildings.push_back({4590,4450,110,150,140,rgb(181,164,137),"ordnance-preview-wall"});
        Vehicle car{};car.kind=Kind::Car;car.p={4520,4430};car.id="ordnance-preview-car";car.c=rgb(190,62,40);
        vehicles.push_back(car);jolt_world::reset();jolt_world::teleportCharacter(player,0);
        weapon=weapons::indexOf("c4");unlocked[weapon]=true;magazine[weapon]=40;ammo[weapon]=40;
        ordnance::devices.push_back({weapons::Payload::C4,{4589.5f,30,4500},{},{-1,0,0},{},{0,1,0},3,130,210,true});
        ordnance::devices.push_back({weapons::Payload::TimedBomb,{4540,1,4535},{},{0,1,0},{},{0,1,0},30,500,1800,true});
        for(int i=0;i<3;++i)ordnance::devices.push_back({weapons::Payload::Grenade,
            {4510+float(i)*15,2,4510},{},{0,1,0},{},{0,1,0},3,115,140,true});
        destruction::blast({4590,25,4550},115);
        ordnance::smoke.push_back({{4590,0,4600},20,75,false});
        message="C4 / TIMED BOMB / FRAG / SMOKE / VEHICLE WEAR";messageTime=5;
    }
    if(std::strstr(commandLine,"--bomb-timer-preview")){
        weapon=weapons::indexOf("timed-bomb");unlocked[weapon]=true;magazine[weapon]=1;
        fireCooldown=0;shoot();
    }
    if(std::strstr(commandLine,"--big-bomb-preview")){
        player=previousPlayer={4500,4500};playerY=0;cameraYaw=.5f;cameraPitch=0;
        cameraMode=CameraMode::ThirdFar;ui::grassDistance=0;trees.clear();buildings.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();
        buildings.push_back({4650,4420,150,170,210,rgb(177,158,135),"big-bomb-wall"});
        jolt_world::reset();debug_menu::godMode=true;
        ordnance::devices.push_back({weapons::Payload::TimedBomb,{4610,1,4490},{},{0,1,0},{},{0,1,0},.01f,500,1800,true});
        ordnance::update(.02f);for(int i=0;i<60;++i)ordnance::update(1.0f/60);
    }
    if(std::strstr(commandLine,"--underwater-preview")){
        player=previousPlayer={600,2000};playerY=-75;swimming=true;grounded=false;
        jolt_world::teleportCharacter(player,playerY);jolt_world::moveCharacter({},false,1.0f/60);
    }
    if(savedBuilderPreview){
        if(builder::blocks().empty()||!builder::requestToggle()||!FinishBuilderTransition())return false;
        auto low=builder::cellLow(builder::blocks().begin()->first);
        occupied=-1;player=previousPlayer={low.x-90,low.z+builder::BLOCK_SIZE*.5f};playerY=terrain::baseHeight(player);
        jolt_world::teleportCharacter(player,playerY);cameraMode=CameraMode::FirstWide;cameraYaw=0;
        cameraPitch=std::atan2(low.y+builder::BLOCK_SIZE*.5f-(playerY+40),100.0f);builder::update(.01f);
        logging::write(("Saved builder preview: "+std::to_string(builder::blocks().size())+" migrated blocks on 20-unit grid").c_str());
    }
    if(benchmark&&(std::strstr(commandLine,"--benchmark-builder")||std::strstr(commandLine,"--benchmark-normal"))){
        bool desired=std::strstr(commandLine,"--benchmark-builder")!=nullptr;
        if(builder::active()!=desired&&(!builder::requestToggle()||!FinishBuilderTransition()))return false;
        logging::write(desired?"Benchmark layer: builder, generated world retained":"Benchmark layer: normal, generated world retained");
    }
    if(benchmark&&std::strstr(commandLine,"--benchmark-dense-builder")&&!prepareDenseBuilder())return false;
    if(std::strstr(commandLine,"--builder-stack-preview")){
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();pickups.clear();occupied=-1;
        player=previousPlayer={400,410};playerY=0;cameraYaw=0;cameraPitch=-std::atan2(20.0f,90.0f);jolt_world::reset();
        if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        int soil=builder::itemIndex("soil");builder::addItem(soil,3);
        if(!builder::place({24,0,20},soil))return false;
        builder::update(.01f);builder::setUseHeld(true);builder::setUseHeld(false);
        if(builder::blocks().size()!=2)return false;
        if(std::strstr(commandLine,"--stack-respawn")){
            health=0;input::windowProc(win,WM_KEYDOWN,'R',0);
            if(!builder::active()||health!=PLAYER_MAX_HEALTH||builder::blocks().size()!=2)return false;
            player=previousPlayer={400,410};playerY=0;jolt_world::teleportCharacter(player,playerY);
        }
        if(std::strstr(commandLine,"--stack-third-person"))cameraMode=CameraMode::ThirdNear;
        builder::update(.01f);
    }
    if((std::strstr(commandLine,"--bike-seat-preview")||
                           std::strstr(commandLine,"--bicycle-seat-preview"))){
        Kind kind=std::strstr(commandLine,"--bicycle-seat-preview")?Kind::Bicycle:Kind::Bike;
        peds.clear();props.clear();pickups.clear();ui::grassDistance=0;
        for(int n=0;n<int(vehicles.size());++n)if(vehicles[n].kind!=kind)
            jolt_world::teleportVehicle(n,{6000+float(n)*150,6000},0);
        for(int i=0;i<int(vehicles.size());++i)if(vehicles[i].kind==kind){
            jolt_world::teleportVehicle(i,{4500,4500},0);occupied=i;
            player=previousPlayer=vehicles[i].p;cameraYaw=2.2f;cameraPitch=0;
            vehicleLookTime=30;cameraMode=CameraMode::ThirdNear;break;
        }
        if(std::strstr(commandLine,"--seat-close-up")){
            // Review the shared driver pose from a closer on-foot camera.
            auto& bike=vehicles[occupied];Ped driver{};driver.id="seat-review";
            driver.alive=true;driver.style=1;driver.drivingVehicle=occupied;
            peds.push_back(driver);bike.driver=0;occupied=-1;
            player=previousPlayer=bike.p+Vec2{60,-60};playerY=terrain::height(player)+3;
            Vec3 aim=norm(Vec3{bike.p.x,bike.rideHeight+16,bike.p.z}-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});
            cameraMode=CameraMode::FirstWide;cameraYaw=std::atan2(aim.z,aim.x);cameraPitch=std::asin(aim.y);
            jolt_world::teleportCharacter(player,playerY);
        }
    }
    if(std::strstr(commandLine,"--builder-rock-review")){
        // Controlled native material review, with real placement, held-input
        // mining and F5/save reconstruction. Acquisition has separate tests.
        constexpr const char* rocks[]={"chalk","mudstone","shale","tuff","pumice","sandstone","limestone","travertine","dolostone","conglomerate",
            "slate","marble","schist","gneiss","andesite","granite","diorite","gabbro","basalt","quartzite"};
        buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();wildlife::animals.clear();birds::flock.clear();
        occupied=enteringVehicle=-1;ui::grassDistance=0;gameHour=12;debug_menu::flyMode=true;
        char reviewPath[MAX_PATH]{};DWORD length=GetEnvironmentVariableA("MINICITY_REVIEW_DIR",reviewPath,MAX_PATH);
        if(length==0||length>=MAX_PATH)return false;
        auto reviewFolder=std::filesystem::absolute(reviewPath).lexically_normal();std::filesystem::create_directories(reviewFolder);
        char module[MAX_PATH]{};GetModuleFileNameA(nullptr,module,MAX_PATH);auto folder=std::filesystem::path(module).parent_path()/"screenshots";
        std::set<std::filesystem::path> known;if(std::filesystem::exists(folder))for(const auto& file:std::filesystem::directory_iterator(folder))known.insert(file.path());
        std::ofstream captures("builder-rock-captures.tsv"),report("builder-rock-report.tsv");if(!captures||!report)return false;
        captures<<"view\tfile\n";report<<"item\thp\telapsed\texpected\tframes\tdurability\timage\n";
        auto captureRock=[&](const std::string& label){auto destination=reviewFolder/(label+".png");
            if(std::filesystem::exists(destination))std::filesystem::remove(destination);
            for(int frame=0;frame<4;++frame)render();requestScreenshot();render();std::filesystem::path generated;
            for(const auto& file:std::filesystem::directory_iterator(folder))if(!known.count(file.path())){known.insert(file.path());if(file.path().extension()==".png")generated=file.path();}
            if(generated.empty())return false;std::filesystem::rename(generated,destination);captures<<label<<'\t'<<destination.filename().string()<<'\n';captures.flush();return true;};
        auto aimRock=[&](){player=previousPlayer={260,280};playerY=0;cameraMode=CameraMode::FirstWide;
            auto direction=norm(Vec3{340,20,340}-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
            jolt_world::teleportCharacter(player,playerY);builder::update(.01f);};
        for(const char* name:rocks){
            builder::reset();player=previousPlayer={260,280};playerY=0;cameraMode=CameraMode::FirstWide;jolt_world::reset();
            if(!builder::requestToggle()||!FinishBuilderTransition())return false;
            int item=builder::itemIndex(name),pick=builder::itemIndex("iron-pickaxe");if(item<0||pick<0)return false;
            if(!builder::addItem(item,2)||!builder::addItem(pick,1)||!builder::place({8,0,8},item,true)||!builder::handleKey('2'))return false;
            aimRock();if(builder::target().source!=builder::Source::Block||builder::target().item!=item)return false;
            const auto& material=builder::items()[item];message=material.name+" | HP "+std::to_string(int(material.hp))+" | original textured block and matching icon";messageTime=30;
            if(!captureRock(std::string(name)+".world"))return false;
            int frames=0;leftMouse=true;while(builder::blocks().count({8,0,8})&&frames<600){builder::update(1.0f/120);++frames;}
            leftMouse=false;builder::update(.01f);float elapsed=float(frames)/120,expected=material.hp/(builder::items()[pick].speed*40);
            if(builder::blocks().count({8,0,8})||builder::inventory()[0].item!=item||builder::inventory()[0].count!=2||
               builder::inventory()[1].durability!=builder::items()[pick].durability-1||std::abs(elapsed-expected)>1.1f/120)return false;
            report<<name<<'\t'<<material.hp<<'\t'<<elapsed<<'\t'<<expected<<'\t'<<frames<<'\t'<<builder::inventory()[1].durability<<'\t'<<name<<".world.png\n";report.flush();
            logging::write((std::string("Rock review: ")+name+" placed/rendered/mined; unique yield and durability PASS").c_str());
        }
        // Reconstruct the complete same-ID catalog through the real save and
        // F5 paths, retaining a normal-mode view with no builder colliders.
        game::reset();player=previousPlayer={300,300};playerY=280;cameraMode=CameraMode::FirstWide;jolt_world::reset();
        if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        builder::Cell galleryCell{};bool clearGallery=false;
        for(int z=1;z<41&&!clearGallery;++z)for(int x=1;x<55&&!clearGallery;++x){bool valid=true;
            for(int n=0;n<20&&valid;++n)valid=builder::canPlace({x+n%5,0,z+n/5},builder::itemIndex(rocks[n]));
            if(valid){galleryCell={x,0,z};clearGallery=true;}}
        if(!clearGallery)return false;
        for(int n=0;n<20;++n)if(!builder::place({galleryCell.x+n%5,0,galleryCell.z+n/5},builder::itemIndex(rocks[n]),false))return false;
        auto galleryLow=builder::cellLow(galleryCell);Vec2 galleryCenter{galleryLow.x+100,galleryLow.z+80};
        auto gallery=[&](){player=previousPlayer={galleryCenter.x,galleryCenter.z+150};playerY=280;cameraMode=CameraMode::FirstWide;
            auto direction=norm(Vec3{galleryCenter.x,20,galleryCenter.z}-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
            jolt_world::teleportCharacter(player,playerY);builder::update(.01f);};
        gallery();message="20 ROCK MATERIALS | DISTINCT GRAIN, LAYERS, PORES AND CRYSTALS";messageTime=30;
        if(!captureRock("gallery")||!savegame::save()||!savegame::load()||builder::active()||builder::blocks().size()!=20)return false;
        gallery();if(jolt_world::activeBuilderColliderCount()!=0)return false;message="NORMAL WORLD | ALL 20 BUILDER ROCKS AND COLLIDERS HIDDEN";messageTime=30;
        if(!captureRock("normal")||!builder::requestToggle()||!FinishBuilderTransition())return false;
        gallery();if(jolt_world::activeBuilderColliderCount()==0)return false;
        for(int n=0;n<20;++n){auto block=builder::blocks().find({galleryCell.x+n%5,0,galleryCell.z+n/5});if(block==builder::blocks().end()||block->second.item!=builder::itemIndex(rocks[n]))return false;}
        message="BUILDER RESTORED | 20 SAVED MATERIAL IDENTITIES AND COLLIDERS";messageTime=30;
        if(!captureRock("restored")||!savegame::save())return false;
        logging::write("Rock review: all 20 native world views and timed mining checks PASS; save/F5 gallery reconstructed");
        finished=true;
        return true;
    }
    if(std::strstr(commandLine,"--builder-catalog-review")){
        // Load the progression scenario's earned save, supplied by the verifier.
        // Review the exact crafted tools through inventory/chest/drop inputs.
        if(!savegame::load())return false;
        peds.clear();vehicles.clear();props.clear();wildlife::animals.clear();birds::flock.clear();ui::grassDistance=0;gameHour=12;jolt_world::reset();
        if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        builder::Cell chestCell{};int chests=0;for(const auto& block:builder::blocks())if(builder::items()[block.second.item].id=="chest"){chestCell=block.first;++chests;}
        if(chests!=1)return false;auto low=builder::cellLow(chestCell);
        // Keep the earned camp untouched. Review away from its narrow street,
        // with enough original, flat ground for the third-person camera and
        // dropped-item view; do not remove scenery to make captures pass.
        Vec2 reviewStage{};bool clearStage=false;
        Vec2 stageForward=forward(.5f),stageSide{-stageForward.z,stageForward.x};
        for(float z=2400;z<=6000&&!clearStage;z+=80)for(float x=2400;x<=6000&&!clearStage;x+=80){
            Vec2 candidate{x,z};if(game::solid(candidate,20))continue;float floor=terrain::baseHeight(candidate);bool valid=true;
            // Validate the actual camera corridor rather than requiring a large
            // empty square in the generated forest and rolling terrain.
            for(float along=-140;along<=40&&valid;along+=30)for(float across:{-20.0f,20.0f}){Vec2 point=candidate+stageForward*along+stageSide*across;
                if(game::solid(point,15)||std::abs(terrain::baseHeight(point)-floor)>4||std::abs(terrain::height(point)-floor)>4){valid=false;break;}}
            if(!valid||game::solid({x,z-40},15)||std::abs(terrain::height({x,z-40})-floor)>2)continue;
            float hit=0;
            if(scenery_edits::segment({x,floor+25,z},{x-stageForward.x*125,floor+68,z-stageForward.z*125},hit)||
               scenery_edits::segment({x,floor+31,z-40},{x+5,floor+10,z+5},hit))continue;
            if(valid){reviewStage=candidate;clearStage=true;}
        }
        if(!clearStage){logging::write("Builder catalog review: no clear original-ground camera stage");return false;}
        char stageReport[160];std::snprintf(stageReport,sizeof(stageReport),"Builder catalog review stage: %.0f, %.0f; original scenery retained",reviewStage.x,reviewStage.z);logging::write(stageReport);
        auto moveReview=[&](Vec2 p){player=previousPlayer=p;playerY=terrain::height(p);jolt_world::teleportCharacter(p,playerY);};
        auto clickReview=[](int n,bool shift=false){auto r=builder::slotRect(n,screenW,screenH);builder::mouse(r.x+3,r.y+3,false,shift);};
        auto openReviewChest=[&](){moveReview({low.x-35,low.z+20});cameraMode=CameraMode::FirstWide;cameraYaw=0;cameraPitch=std::atan2(20-(playerY+31),55.0f);
            builder::update(.3f);if(builder::target().source!=builder::Source::Block||builder::target().item!=builder::itemIndex("chest"))return false;
            builder::use();return builder::chest()!=nullptr;};
        auto inventorySlot=[](int item){for(int n=0;n<36;++n)if(builder::inventory()[n].item==item)return n;return -1;};
        auto equipReview=[&](int item){int n=inventorySlot(item);if(n<0)return false;
            if(n>=9){builder::handleKey('E');clickReview(n);clickReview(8);if(builder::cursor().item>=0)clickReview(n);if(builder::cursor().item>=0)return false;builder::closeInventory();n=8;}
            return builder::handleKey('1'+n)&&builder::inventory()[builder::selected()].item==item;};
        char module[MAX_PATH]{};GetModuleFileNameA(nullptr,module,MAX_PATH);auto folder=std::filesystem::path(module).parent_path()/"screenshots";
        char reviewPath[MAX_PATH]{};DWORD reviewLength=GetEnvironmentVariableA("MINICITY_REVIEW_DIR",reviewPath,MAX_PATH);
        std::filesystem::path reviewFolder;if(reviewLength>0&&reviewLength<MAX_PATH){reviewFolder=std::filesystem::absolute(reviewPath).lexically_normal();std::filesystem::create_directories(reviewFolder);}
        std::set<std::filesystem::path> known;if(std::filesystem::exists(folder))for(const auto& file:std::filesystem::directory_iterator(folder))known.insert(file.path());
        std::ofstream captures("builder-catalog-captures.tsv");if(!captures)return false;captures<<"view\tfile\n";
        auto captureReview=[&](const std::string& label){message="Tool catalog | "+label;messageTime=5;
            // Replace only this named review artifact before capture, avoiding
            // a second full catalog's disk allocation on a nearly full drive.
            auto destination=reviewFolder/(label+".png");if(!reviewFolder.empty()&&std::filesystem::exists(destination))std::filesystem::remove(destination);
            for(int frame=0;frame<4;++frame)render();requestScreenshot();render();bool captured=false;
            std::filesystem::path generated;
            for(const auto& file:std::filesystem::directory_iterator(folder))if(!known.count(file.path())){known.insert(file.path());
                if(file.path().extension()==".png"){generated=file.path();captured=true;}}
            if(captured){if(!reviewFolder.empty()){std::filesystem::rename(generated,destination);generated=destination;}
                captures<<label<<'\t'<<generated.filename().string()<<'\n';}
            captures.flush();logging::write(("Builder catalog capture: "+label+(captured?" PASS":" FAIL")).c_str());return captured;};
        if(!openReviewChest())return false;int count=0;for(auto stack:*builder::chest())if(stack.item>=0&&builder::items()[stack.item].tool!=builder::Tool::None)++count;
        if(count!=22||!captureReview("storage"))return false;builder::closeInventory();
        for(int item=0;item<int(builder::items().size());++item){const auto& tool=builder::items()[item];if(tool.tool==builder::Tool::None)continue;
            if(!openReviewChest())return false;int stored=-1,durability=0;
            for(int n=0;n<27;++n)if((*builder::chest())[n].item==item){stored=n;durability=(*builder::chest())[n].durability;}
            if(stored<0)return false;clickReview(36+stored,true);builder::closeInventory();if(!equipReview(item))return false;
            moveReview(reviewStage);
            cameraMode=CameraMode::FirstWide;cameraYaw=.5f;cameraPitch=-.16f;
            builder::update(.01f);
            if(!captureReview(tool.id+".hand"))return false;builder::handleKey('C');if(!captureReview(tool.id+".third"))return false;
            auto drops=builder::drops().size();builder::handleKey('Q');if(builder::drops().size()!=drops+1)return false;auto dropped=builder::drops().back();
            if(dropped.stack.item!=item||dropped.stack.durability!=durability)return false;
            moveReview({dropped.p.x,dropped.p.z-40});cameraMode=CameraMode::FirstWide;
            auto direction=norm(dropped.p+Vec3{5,5,5}-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
            builder::update(.01f);
            if(!captureReview(tool.id+".drop"))return false;
            moveReview({dropped.p.x,dropped.p.z});builder::update(.01f);int retrieved=inventorySlot(item);
            if(retrieved<0||builder::inventory()[retrieved].durability!=durability||builder::drops().size()!=drops)return false;
            if(!openReviewChest())return false;clickReview(retrieved,true);if(inventorySlot(item)>=0)return false;builder::closeInventory();
        }
        if(!savegame::save())return false;
        logging::write("Builder catalog review: all 22 earned tools captured in hand/third/drop views and returned to chest");
        finished=true;
        return true;
    }
    if(std::strstr(commandLine,"--builder-preview")){
        player=previousPlayer={300,235};playerY=game::groundHeight(player);occupied=-1;
        jolt_world::teleportCharacter(player,playerY);
        if(!builder::requestToggle())return false;
        if(!FinishBuilderTransition())return false;
        int granite=builder::itemIndex("granite");builder::addItem(granite,32);builder::addItem(builder::itemIndex("iron-axe"),1);builder::addItem(builder::itemIndex("diamond-pickaxe"),1);
        builder::addItem(builder::itemIndex("chest"),3);builder::addItem(builder::itemIndex("log"),20);builder::addItem(builder::itemIndex("marble"),24);
        for(int x=8;x<=11;++x)for(int y=0;y<2;++y)builder::place({x,y,7},granite,false);
        cameraYaw=.5f;cameraPitch=-.16f;builder::update(1.0f/60);
        if(std::strstr(commandLine,"--builder-tool")||std::strstr(commandLine,"--builder-third-person"))builder::handleKey('2');
        if(std::strstr(commandLine,"--builder-third-person")){
            builder::handleKey('C');
            // Move only the preview's obstructing aircraft, so the animated hand is visible.
            for(std::size_t n=0;n<vehicles.size();++n)if(vehicles[n].kind==Kind::Helicopter&&len(vehicles[n].p-player)<300)
                jolt_world::teleportVehicle(n,{5000,5000},vehicles[n].angle,game::groundHeight({5000,5000}));
        }
        if(std::strstr(commandLine,"--builder-inventory"))builder::handleKey('E');
        if(std::strstr(commandLine,"--builder-loading"))builder::requestToggle();
    }
    if(std::strstr(commandLine,"--builder-save-preview")){
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();pickups.clear();ui::grassDistance=0;
        player=previousPlayer={300,235};playerY=0;occupied=enteringVehicle=-1;cameraMode=CameraMode::FirstWide;cameraYaw=.5f;cameraPitch=-.16f;jolt_world::reset();
        if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        if(!builder::addItem(builder::itemIndex("iron-axe"),1,77)||!builder::addItem(builder::itemIndex("granite"),32))return false;
        for(int x=8;x<=11;++x)for(int y=0;y<2;++y)if(!builder::place({x,y,7},builder::itemIndex("granite"),false))return false;
        jolt_world::refreshScenery();if(!savegame::save())return false;
        if(std::strstr(commandLine,"--save-focus")){
            float hour=gameHour;if(!builder::requestToggle())return false;input::windowProc(win,WM_KILLFOCUS,0,0);
            if(!ui::paused())return false;
            for(int tick=0;tick<5000&&builder::transitioning();++tick){if(ui::paused())builder::advance(1.0f/60);if(builder::transitioning())Sleep(1);}
            if(builder::transitioning()||builder::active()||!ui::paused()||gameHour!=hour)return false;
            input::windowProc(win,WM_KEYDOWN,VK_ESCAPE,0);if(ui::paused())return false;
            logging::write("Builder focus preview: F5 finished through focus-loss pause, simulation clock unchanged, Escape resumes");
        }else if(std::strstr(commandLine,"--save-loading")){
            if(!builder::requestToggle())return false;builder::advance(.12f);
        }else{
            std::string file=savegame::filePath();
            HANDLE locked=CreateFileA(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(locked==INVALID_HANDLE_VALUE)return false;
            bool requested=builder::requestToggle();bool switched=requested&&FinishBuilderTransition();CloseHandle(locked);
            if(!requested||switched||builder::transitioning()||!builder::active()||message.find("Mode unchanged")==std::string::npos)return false;
            if(std::strstr(commandLine,"--save-retry")){if(!builder::requestToggle()||!FinishBuilderTransition()||builder::active())return false;}
        }
        char report[192];std::snprintf(report,sizeof(report),"Builder save preview: mode %s, transitioning %d, stored blocks %zu, active colliders %zu, axe durability %d",
            builder::active()?"builder":"normal",int(builder::transitioning()),builder::blocks().size(),jolt_world::activeBuilderColliderCount(),builder::inventory()[0].durability);logging::write(report);
    }
    if(std::strstr(commandLine,"--builder-recovery-preview")){
        // Prepared cuts isolate actor recovery; all falling and F5 work uses
        // the ordinary Jolt bodies and mode transition, without advancing AI.
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();pickups.clear();ui::grassDistance=0;
        player=previousPlayer={140,60};playerY=0;occupied=enteringVehicle=-1;cameraMode=CameraMode::FirstWide;cameraYaw=PI/2;cameraPitch=0;
        props.push_back({{118,150},{},0});Ped ped{};ped.id="builder-recovery-preview-corpse";ped.p=ped.target={164,170};ped.style=1;ped.respawn=45;peds.push_back(ped);jolt_world::reset();
        auto switchLayer=[&](){if(!builder::requestToggle())return false;if(!FinishBuilderTransition())return false;
            return !builder::transitioning()&&jolt_world::lastSceneryRecovery().unresolved==0;};
        if(!switchLayer())return false;builder::handleKey('9');for(int x=2;x<=4;++x)for(int z=2;z<=4;++z)for(int y=-2;y<0;++y)if(!builder::mineTerrain({x,y,z}))return false;
        jolt_world::teleportPed(0,ped.p,-80);peds[0].alive=false;peds[0].health=0;peds[0].corpseVisualDelay=6;jolt_world::spawnRagdoll(peds[0],{});
        for(int n=0;n<420;++n)jolt_world::step(1.0f/60);
        if(std::strstr(commandLine,"--recovery-normal")||std::strstr(commandLine,"--recovery-restored")){if(!switchLayer())return false;}
        if(std::strstr(commandLine,"--recovery-restored")){if(!switchLayer())return false;for(int n=0;n<120;++n)jolt_world::step(1.0f/60);}
        cameraMode=CameraMode::FirstWide;cameraYaw=PI/2;cameraPitch=-std::atan2(builder::active()?100.0f:20.0f,100.0f);
        char report[192];std::snprintf(report,sizeof(report),"Builder recovery preview: mode %s, prop bottom %.3f, corpse torso %.3f, %zu pose parts",
            builder::active()?"builder":"normal",props[0].y,ragdollParts.empty()?0:ragdollParts[0].p.y,ragdollParts.size());logging::write(report);
    }
    if(std::strstr(commandLine,"--builder-feedback-preview")){
        // Deterministic rendering fixtures; acquisition is verified separately.
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();pickups.clear();occupied=-1;ui::grassDistance=0;
        player=previousPlayer={330,420};playerY=0;cameraYaw=cameraPitch=0;jolt_world::reset();
        if(!builder::requestToggle())return false;if(!FinishBuilderTransition())return false;
        const char* material="granite";const char* tool="iron-pickaxe";float miningTime=.4f;
        if(std::strstr(commandLine,"--feedback-wood")){material="log";tool="iron-axe";miningTime=.23f;}
        if(std::strstr(commandLine,"--feedback-soil")){material="soil";tool="iron-shovel";miningTime=.065f;}
        if(std::strstr(commandLine,"--feedback-shears")){material="leaves";tool="shears";miningTime=std::strstr(commandLine,"--feedback-shears-open")?0:std::strstr(commandLine,"--feedback-shears-cut")?.08f:.04f;}
        if(std::strstr(commandLine,"--feedback-brush")){material=nullptr;tool="brush";miningTime=.19f;}
        builder::addItem(builder::itemIndex(tool),1);
        if(material){if(!builder::place({10,0,10},builder::itemIndex(material),false))return false;builder::update(.01f);}
        else{bool found=false;for(const auto& deposit:surface_work::nearby({4500,4500},600)){
            for(int z=-5;z<=5&&!found;z+=2)for(int x=-5;x<=5&&!found;x+=2){auto point=deposit.position+Vec3{float(x),1.5f,float(z)};
                player=previousPlayer={point.x-65,point.z};playerY=terrain::baseHeight(player);auto direction=norm(point-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});
                cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);jolt_world::teleportCharacter(player,playerY);builder::update(.01f);
                found=builder::target().source==builder::Source::Deposit&&builder::target().cell==deposit.cell;}if(found)break;}
            if(!found)return false;builder::setUseHeld(true);
        }
        if(miningTime>0){if(material)leftMouse=true;builder::update(miningTime);
            if(!material){for(int tick=0;tick<4;++tick)builder::update(.05f);}
            else builder::update(std::strstr(commandLine,"--feedback-shears-cut")?.03f:std::strstr(commandLine,"--feedback-shears")?.005f:.02f);}
        if(std::strstr(commandLine,"--feedback-third")){builder::handleKey('C');}
        if(std::strstr(commandLine,"--feedback-normal")){
            if(!builder::requestToggle())return false;if(!FinishBuilderTransition())return false;cameraMode=CameraMode::FirstWide;
        }
    }
    if(std::strstr(commandLine,"--tool-work-preview")){
        // Visual fixtures supply tools/materials; input-driven action tests live in tool_work_scenarios.
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();pickups.clear();occupied=-1;ui::grassDistance=0;
        player=previousPlayer={100,100};playerY=terrain::baseHeight(player);jolt_world::reset();
        if(!builder::requestToggle())return false;if(!FinishBuilderTransition())return false;
        builder::addItem(builder::itemIndex("iron-hoe"),1,100);builder::addItem(builder::itemIndex("brush"),1,45);
        builder::addItem(builder::itemIndex("iron-ingot"),3);builder::addItem(builder::itemIndex("plank"),8);builder::addItem(builder::itemIndex("soil"),20);
        auto aimAt=[&](Vec3 point){player=previousPlayer={point.x-65,point.z};playerY=terrain::baseHeight(player);cameraMode=CameraMode::FirstWide;
            auto direction=norm(point-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
            jolt_world::teleportCharacter(player,playerY);builder::update(.01f);};
        builder::Cell soil{};bool found=false;
        for(int z=110;z<150&&!found;++z)for(int x=110;x<150&&!found;++x){Vec2 center{x*40.0f+20,z*40.0f+20};auto cell=builder::cellAt({center.x,terrain::baseHeight(center)-.01f,center.z});
            if(surface_work::validSoil(cell)&&!surface_work::validDeposit(cell)){soil=cell;found=true;}}
        if(!found)return false;auto low=builder::cellLow(soil);Vec3 center{low.x+20,terrain::baseHeight({low.x+20,low.z+20}),low.z+20};
        for(int z=soil.z-1;z<=soil.z+1;++z)for(int x=soil.x-1;x<=soil.x+1;++x){Vec2 p{x*40.0f+20,z*40.0f+20};builder::Target target;target.source=builder::Source::Ground;target.normal={0,1,0};
            target.cell=builder::cellAt({p.x,terrain::baseHeight(p)-.01f,p.z});if(!(target.cell==soil))surface_work::till(target);}
        aimAt(center);builder::setUseHeld(true);builder::update(.1f);builder::setUseHeld(false);
        if(std::strstr(commandLine,"--tool-work-brush")){
            builder::handleKey('2');found=false;
            for(const auto& deposit:surface_work::nearby({center.x,center.z},600)){
                for(int z=-5;z<=5&&!found;z+=2)for(int x=-5;x<=5&&!found;x+=2){aimAt(deposit.position+Vec3{float(x),1.5f,float(z)});
                    found=builder::target().source==builder::Source::Deposit&&builder::target().cell==deposit.cell;}if(found)break;}
            if(!found)return false;builder::setUseHeld(true);builder::update(.6f);
        }else if(std::strstr(commandLine,"--tool-work-repair")){
            aimAt(center);builder::place({soil.x,soil.y+1,soil.z+2},builder::itemIndex("crafting-bench"),false);builder::handleKey('E');
        }else if(std::strstr(commandLine,"--tool-work-normal")){
            if(!builder::requestToggle())return false;if(!FinishBuilderTransition())return false;aimAt(center);
        }
    }
    if(std::strstr(commandLine,"--tree-preview")){
        buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();pickups.clear();occupied=-1;
        ui::grassDistance=0;
        Tree tree{};tree.id="tree-quality-preview";tree.p={4500,4500};tree.scale=2;
        tree.modelId="tree_detailed";tree.crownWidth=54;tree.height=68;
        if(const char* option=std::strstr(commandLine,"--tree-model=")){
            char model[96]{};if(std::sscanf(option,"--tree-model=%95s",model)==1)tree.modelId=model;
        }
        trees.push_back(tree);
        bool distant=std::strstr(commandLine,"--tree-distant")!=nullptr;
        player=previousPlayer={distant?4080.0f:4320.0f,4500};playerY=terrain::baseHeight(player);
        cameraMode=CameraMode::FirstWide;
        auto direction=norm(Vec3{tree.p.x,terrain::baseHeight(tree.p)+65,tree.p.z}-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});
        cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
        jolt_world::reset();jolt_world::teleportCharacter(player,playerY);
        message=tree.modelId+(distant?" | DISTANT TREE":" | NEAR TREE");messageTime=30;
    }
    if(std::strstr(commandLine,"--scenery-preview")){
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();pickups.clear();occupied=-1;
        ui::grassDistance=0;
        Tree tree{};tree.id="scenery-preview-tree";tree.p={4500,4500};tree.scale=2;trees.push_back(tree);
        player=previousPlayer={4430,4500};playerY=terrain::baseHeight(player);jolt_world::reset();
        if(!builder::requestToggle())return false;if(!FinishBuilderTransition())return false;
        builder::addItem(builder::itemIndex("iron-axe"),1);builder::addItem(builder::itemIndex("diamond-pickaxe"),1);builder::addItem(builder::itemIndex("shears"),1);
        scenery_edits::Object object;scenery_edits::treeObject(0,object);
        if(std::strstr(commandLine,"--scenery-rock")){
            bool found=false;for(int z=0;z<42&&!found;++z)for(int x=0;x<42&&!found;++x)if(scenery_edits::outcropObject(x,z,object)&&object.size.x>120)found=true;
            if(!found)return false;builder::handleKey('2');
        }else if(std::strstr(commandLine,"--scenery-bush")){
            bool found=false;for(const auto& prop:regions::decorations())if(prop.modelId.rfind("bush_",0)==0){found=scenery_edits::find("decoration:"+prop.id,object);if(found)break;}
            if(!found)return false;builder::handleKey('3');
        }
        float eyeHeight=object.kind==scenery_edits::Kind::Tree?20:object.kind==scenery_edits::Kind::Rock?object.size.y*.6f:object.size.y*.7f;
        builder::Target section;section.distance=200;Vec3 origin=object.position+Vec3{-object.size.x,eyeHeight,0};
        if(scenery_edits::trace(origin,{1,0,0},200,section)&&section.objectId==object.id)scenery_edits::cut(object.id,section.cell);
        if(object.kind==scenery_edits::Kind::Tree)scenery_edits::cut(object.id,builder::cellAt(object.position+Vec3{0,7,-12}));
        player=previousPlayer={origin.x,origin.z};playerY=std::max(terrain::baseHeight(player),origin.y-31);
        if(object.kind==scenery_edits::Kind::Tree){player=previousPlayer={object.position.x-90,object.position.z};playerY=object.position.y;}
        if(object.kind==scenery_edits::Kind::Plant){player=previousPlayer={object.position.x-65,object.position.z};playerY=terrain::baseHeight(player);}
        auto direction=norm(object.position+Vec3{0,object.kind==scenery_edits::Kind::Tree?42:eyeHeight,0}-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});
        cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);jolt_world::teleportCharacter(player,playerY);builder::update(1.0f/60);
        if(std::strstr(commandLine,"--scenery-normal")){
            if(!builder::requestToggle())return false;if(!FinishBuilderTransition())return false;
            cameraMode=CameraMode::FirstWide;cameraYaw=0;cameraPitch=.12f;
        }
    }
    if(std::strstr(commandLine,"--excavation-preview")){
        // A deterministic capture fixture; these resources are not a gameplay acquisition test.
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();
        wildlife::animals.clear();pickups.clear();occupied=-1;ui::grassDistance=2;
        player=previousPlayer={140,60};playerY=0;jolt_world::reset();
        if(!builder::requestToggle())return false;
        if(!FinishBuilderTransition())return false;
        builder::addItem(builder::itemIndex("iron-shovel"),1);
        builder::addItem(builder::itemIndex("diamond-pickaxe"),1);
        for(int x=2;x<=4;++x)for(int z=2;z<=4;++z)for(int y=-2;y<0;++y)builder::mineTerrain({x,y,z});
        for(int x=5;x<=8;++x)for(int z=2;z<=4;++z)builder::mineTerrain({x,-2,z});
        cameraYaw=PI/2;cameraPitch=-.72f;
        if(std::strstr(commandLine,"--excavation-tunnel")){
            player=previousPlayer={220,140};playerY=-80;cameraYaw=0;cameraPitch=.06f;
            builder::handleKey('2');
        }
        if(std::strstr(commandLine,"--excavation-mountain")){
            excavation::clear();player=previousPlayer={13900,1780};playerY=terrain::baseHeight(player)+100;cameraPitch=-.9f;
            for(int x=346;x<=348;++x)for(int z=45;z<=47;++z){
                float h=terrain::baseHeight({x*40.0f+20,z*40.0f+20});int top=int(std::floor((h-.01f)/40));
                for(int y=top-1;y<=top;++y)builder::mineTerrain({x,y,z});
            }
        }
        jolt_world::teleportCharacter(player,playerY);builder::update(1.0f/60);
        if(std::strstr(commandLine,"--excavation-normal")){
            if(!builder::requestToggle())return false;
            if(!FinishBuilderTransition())return false;
            cameraYaw=PI/2;cameraPitch=-.72f;
        }
    }
    if(std::strstr(commandLine,"--builder-wildlife-preview")){
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();wildlife::animals.clear();birds::flock.clear();
        occupied=enteringVehicle=-1;ui::grassDistance=0;gameHour=12;player=previousPlayer={400,400};playerY=0;jolt_world::reset();
        if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        bool tunnel=std::strstr(commandLine,"--wildlife-tunnel")!=nullptr,mount=std::strstr(commandLine,"--wildlife-mounted")!=nullptr;
        bool block=std::strstr(commandLine,"--wildlife-block")!=nullptr,normal=std::strstr(commandLine,"--wildlife-normal")!=nullptr;
        if(tunnel||mount){for(int x=2;x<=12;++x)for(int z=2;z<=6;++z)
            for(int y=mount?-3:-2;y<=-2;++y)if(!builder::mineTerrain({x,y,z}))return false;
        }else if(!block){for(int x=5;x<=8;++x)for(int z=3;z<=6;++z)for(int y=-2;y<0;++y)if(!builder::mineTerrain({x,y,z}))return false;}
        if(block&&!builder::place({5,0,3},builder::itemIndex("granite"),false))return false;
        wildlife::Animal animal;animal.id="builder-wildlife-review";animal.species=mount?0:4;
        animal.p=animal.home=animal.target=tunnel?Vec2{140,140}:mount?Vec2{220,160}:block?Vec2{220,140}:Vec2{260,180};
        animal.health=wildlife::species()[animal.species].health;animal.timer=100;
        animal.elevation=mount?-120:tunnel?-80:block?40:0;animal.elevationAt=animal.p;animal.elevationKnown=animal.elevationMode=true;
        wildlife::animals.push_back(animal);
        if(tunnel){wildlife::animals[0].state=wildlife::State::Wander;wildlife::animals[0].target={440,140};}
        if(mount){player=previousPlayer={190,160};playerY=-120;cameraYaw=0;debug_menu::flyMode=false;jolt_world::teleportCharacter(player,playerY);
            if(!wildlife::mount(0))return false;keys['W']=true;}
        for(int tick=0;tick<(tunnel?160:mount?60:90);++tick){if(mount)wildlife::updateRider(1.0f/60);wildlife::update(1.0f/60);jolt_world::step(1.0f/60);}
        keys['W']=false;float floor=mount?-120:tunnel||!block?-80:40;
        if(std::abs(wildlife::originHeight(wildlife::animals[0])-floor)>2)return false;
        if(normal){if(!builder::requestToggle()||!FinishBuilderTransition()||jolt_world::activeBuilderColliderCount()!=0)return false;}
        if(block)pickups.push_back({{232,148},weapons::indexOf("pistol"),true,0,"wildlife-review-pickup"});
        auto& a=wildlife::animals[0];Vec3 focus{a.p.x,wildlife::originHeight(a)+12,a.p.z};
        if(mount){cameraMode=CameraMode::ThirdNear;cameraYaw=0;cameraPitch=-.1f;}
        else{
            player=previousPlayer=tunnel?Vec2{a.p.x-85,a.p.z}:block?Vec2{150,65}:Vec2{185,90};
            playerY=tunnel?-80:block?65:95;auto direction=norm(focus-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});
            cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);cameraMode=CameraMode::FirstWide;debug_menu::flyMode=true;
            jolt_world::teleportCharacter(player,playerY);
        }
        builder::update(.01f);message=normal?"NORMAL: ORIGINAL GROUND AND ANIMAL HEIGHT RESTORED":mount?"BUILDER: RIDING ON THE ACTUAL UNDERGROUND FLOOR":
            tunnel?"BUILDER: WILDLIFE WALKS BELOW THE RETAINED ROOF":block?"BUILDER: ANIMAL AND PICKUP SHARE PLACED BLOCK SUPPORT":"BUILDER: ANIMAL FALLS INTO THE EXCAVATED PIT";messageTime=30;
        char result[220]{};std::snprintf(result,sizeof(result),"Wildlife review: mode %s, animal %.2f %.2f %.2f, supported %d, mounted %d, player Y %.2f",
            builder::active()?"builder":"normal",a.p.x,wildlife::originHeight(a),a.p.z,int(a.supported),int(wildlife::riding()),playerY);logging::write(result);
    }
    if(std::strstr(commandLine,"--builder-destination-preview")){
        // Actual AI destination selection and Jolt movement in controlled
        // edited geometry; this is not an acquisition/progression fixture.
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();wildlife::animals.clear();birds::flock.clear();
        occupied=enteringVehicle=-1;ui::grassDistance=0;gameHour=12;player=previousPlayer={400,400};playerY=0;jolt_world::reset();
        if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        bool wander=std::strstr(commandLine,"--destination-wander")!=nullptr,cover=std::strstr(commandLine,"--destination-cover")!=nullptr;
        bool normal=std::strstr(commandLine,"--destination-normal")!=nullptr;float floor=wander||cover?-80.0f:0.0f;
        if(wander||cover){for(int x=2;x<=12;++x)for(int z=2;z<=(wander?8:5);++z)if(!builder::mineTerrain({x,-2,z}))return false;}
        else for(int x=5;x<=7;++x)for(int z=2;z<=4;++z)if(!builder::mineTerrain({x,-1,z}))return false;
        if(wander){buildings.push_back({80,80,440,280,120,rgb(115,132,146),"destination-review-building"});jolt_world::refreshScenery();}
        if(cover&&!builder::place({6,-2,3},builder::itemIndex("granite"),false))return false;
        Ped walker{};walker.id="destination-review";walker.p=walker.target=wander?Vec2{240,180}:cover?Vec2{360,140}:Vec2{150,140};
        walker.speed=65;walker.shirt=rgb(218,109,46);walker.socialCooldown=999;peds.push_back(walker);jolt_world::addPed();jolt_world::teleportPed(0,walker.p,floor);
        if(cover){player=previousPlayer={160,140};playerY=floor;jolt_world::teleportCharacter(player,playerY);
            peds[0].armed=true;peds[0].weaponIndex=weapons::indexOf("pistol");peds[0].tacticTimer=10;peds[0].fireCooldown=100;
            ped_navigation::beginFrame();ai::reactToHit(peds[0],player);if(peds[0].state!=PedState::TakeCover)return false;}
        else if(!wander){player=previousPlayer={100,140};playerY=0;jolt_world::teleportCharacter(player,playerY);ped_navigation::beginFrame();
            if(ai::notifyThreat(player,0)!=1)return false;}
        std::srand(41);for(int tick=0;tick<(wander?100:cover?360:80);++tick){ai::update(1.0f/60);jolt_world::step(1.0f/60);}
        if(std::abs(jolt_world::pedHeight(0)-floor)>2||cover&&peds[0].state!=PedState::Defend)return false;
        if(normal){if(!builder::requestToggle()||!FinishBuilderTransition()||jolt_world::activeBuilderColliderCount()!=0)return false;}
        Vec3 focus{};
        if(cover){player=previousPlayer={380,100};playerY=-80;focus={270,-62,140};message="BUILDER: DEFENDER HOLDS REAL UNDERGROUND BLOCK COVER";}
        else if(wander){player=previousPlayer={110,140};playerY=-80;focus={peds[0].p.x,-62,peds[0].p.z};message="BUILDER: WANDERING BELOW AN ORIGINAL BUILDING";}
        else{player=previousPlayer={185,25};playerY=100;focus={265,0,140};message=normal?"NORMAL: ORIGINAL GROUND AND ROUTING RESTORED":"BUILDER: FLEEING ACTOR CHOOSES A SUPPORTED PIT DETOUR";}
        auto direction=norm(focus-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
        cameraMode=CameraMode::FirstWide;debug_menu::flyMode=true;messageTime=30;jolt_world::teleportCharacter(player,playerY);builder::update(.01f);
        char result[256]{};std::snprintf(result,sizeof(result),"Destination review: mode %s, state %d, pedestrian %.2f %.2f %.2f, target %.2f %.2f %.2f, selected %d",
            builder::active()?"builder":"normal",int(peds[0].state),peds[0].p.x,jolt_world::pedHeight(0),peds[0].p.z,
            peds[0].target.x,peds[0].navigation.goalHeight,peds[0].target.z,int(ped_navigation_surface::hasDestination(peds[0])));logging::write(result);
    }
    if(std::strstr(commandLine,"--builder-interaction-preview")){
        // Review actual interactions and physics in controlled geometry.
        // These views do not claim an exploratory progression playthrough.
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();bullets.clear();wildlife::animals.clear();birds::flock.clear();
        commerce::shops.clear();commerce::houses.clear();traversal::ladders.clear();traversal::trees.clear();missions.clear();
        occupied=enteringVehicle=-1;ui::grassDistance=0;gameHour=12;health=PLAYER_MAX_HEALTH;player=previousPlayer={400,400};playerY=0;jolt_world::reset();
        if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        bool carried=std::strstr(commandLine,"--interaction-carried")!=nullptr,dropped=std::strstr(commandLine,"--interaction-dropped")!=nullptr;
        bool combat=std::strstr(commandLine,"--interaction-combat")!=nullptr,normal=std::strstr(commandLine,"--interaction-normal")!=nullptr;
        for(int x=2;x<=12;++x)for(int z=2;z<=6;++z){if(!builder::mineTerrain({x,-2,z}))return false;
            if(carried||dropped||normal)if(!builder::mineTerrain({x,-1,z}))return false;}
        Ped actor{};actor.id="builder-interaction-review";actor.p=actor.target=combat?Vec2{360,180}:Vec2{220,140};actor.speed=65;actor.socialCooldown=999;actor.cash=73;
        peds.push_back(actor);jolt_world::addPed();jolt_world::teleportPed(0,actor.p,-80);
        if(combat){
            buildings.push_back({80,80,440,280,120,rgb(115,132,146),"interaction-review-building"});jolt_world::refreshScenery();
            player=previousPlayer={160,180};playerY=-80;jolt_world::teleportCharacter(player,playerY);
            peds[0].armed=peds[0].hostile=true;peds[0].state=PedState::Attack;peds[0].alertTime=100;peds[0].weaponIndex=weapons::indexOf("pistol");
            for(int tick=0;tick<45;++tick){ai::update(1.0f/60);jolt_world::step(1.0f/60);}
            if(!ped_navigation_surface::hasDestination(peds[0])||std::abs(peds[0].navigation.goalHeight+80)>2||bullets.empty())return false;
            message="BUILDER: ARMED ACTOR MOVES AND FIRES ON THE UNDERGROUND FLOOR";
        }else{
            auto& body=peds[0];body.alive=false;body.health=0;body.respawn=45;body.corpseVisualDelay=6;
            if(!jolt_world::spawnRagdoll(body,{}))return false;
            for(int tick=0;tick<420;++tick)jolt_world::step(1.0f/60);
            player=previousPlayer=body.p+Vec2{-25,0};playerY=-80;cameraYaw=0;jolt_world::teleportCharacter(player,playerY);
            if(interactionPrompt().find("LOOT")==std::string::npos||carryPrompt().empty())return false;
            if(carried||dropped){interact();carryDrop();if(!body.carried||!corpseSnapshots.empty())return false;
                player=previousPlayer={200,160};playerY=-80;jolt_world::teleportCharacter(player,playerY);
                game::update(1.0f/60);
                if(dropped){carryDrop();if(body.carried||ragdollParts.size()!=6)return false;for(int tick=0;tick<180;++tick)jolt_world::step(1.0f/60);}}
            if(normal){if(!builder::requestToggle()||!FinishBuilderTransition()||jolt_world::activeBuilderColliderCount()!=0)return false;}
            message=normal?"NORMAL: ORIGINAL GROUND AND CORPSE SUPPORT RESTORED":carried?"BUILDER: CARRYING THE LOOTED BODY BELOW THE ORIGINAL GROUND":
                dropped?"BUILDER: RELEASED BODY SETTLES ON THE EXCAVATED FLOOR":"BUILDER: LOOT AND CARRY USE THE CAPTURED UNDERGROUND BODY POSE";
        }
        Vec3 low{},high{},contact{};
        if(peds[0].alive){low={peds[0].p.x,jolt_world::pedHeight(0),peds[0].p.z};high=low+Vec3{0,37,0};contact=low+Vec3{0,18,0};}
        else jolt_world::corpsePose(peds[0],low,high,contact);
        if(carried){cameraMode=CameraMode::ThirdNear;cameraYaw=0;cameraPitch=-.12f;debug_menu::flyMode=false;}
        else{
            if(combat){player=previousPlayer={220,160};playerY=-80;contact={peds[0].p.x,-62,peds[0].p.z};}
            else if(normal){player=previousPlayer={150,80};playerY=80;}
            else {player=previousPlayer=peds[0].p+Vec2{-32,-8};playerY=-80;}
            auto direction=norm(contact-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
            cameraMode=CameraMode::FirstWide;debug_menu::flyMode=true;jolt_world::teleportCharacter(player,playerY);
        }
        if(!carried)builder::handleKey('9'); // Keep actual body/weapon models visible in review views.
        messageTime=30;builder::update(.01f);
        char result[320]{};std::snprintf(result,sizeof(result),"Interaction review: mode %s, alive %d, carried %d, looted %d, body %.2f %.2f %.2f, bounds %.2f..%.2f, player Y %.2f, target floor %.2f, selected %d",
            builder::active()?"builder":"normal",int(peds[0].alive),int(peds[0].carried),int(peds[0].looted),contact.x,contact.y,contact.z,low.y,high.y,playerY,
            peds[0].navigation.goalHeight,int(ped_navigation_surface::hasDestination(peds[0])));logging::write(result);
    }
    if(std::strstr(commandLine,"--builder-police-preview")){
        // Native reviews use actual F5, dispatch, takedowns and Jolt poses.
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();bullets.clear();wildlife::animals.clear();birds::flock.clear();
        commerce::shops.clear();commerce::houses.clear();traversal::ladders.clear();traversal::trees.clear();missions.clear();police::reset();
        occupied=enteringVehicle=-1;ui::grassDistance=0;gameHour=12;health=PLAYER_MAX_HEALTH;player=previousPlayer={400,700};playerY=0;jolt_world::reset();
        if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        bool dispatch=std::strstr(commandLine,"--police-dispatch")!=nullptr,normal=std::strstr(commandLine,"--police-normal")!=nullptr;
        bool witness=std::strstr(commandLine,"--police-witness")!=nullptr,blocked=std::strstr(commandLine,"--police-blocked")!=nullptr;
        bool wide=dispatch||normal;
        for(int x=2;x<=(wide?32:12);++x)for(int z=wide?4:2;z<=(wide?12:6);++z)if(!builder::mineTerrain({x,-2,z}))return false;
        if(wide){buildings.push_back({80,160,1240,440,120,{1,1,1},"dispatch-surface-building"});jolt_world::refreshScenery();}
        auto at=[](Vec2 p,float y){player=previousPlayer=p;playerY=y;jolt_world::teleportCharacter(p,y);};
        auto add=[](Vec2 p,float y,float angle){Ped ped{};ped.id="police-review-"+std::to_string(peds.size());ped.p=ped.target=p;ped.angle=angle;ped.speed=57;ped.socialCooldown=999;
            peds.push_back(ped);jolt_world::addPed();jolt_world::teleportPed(peds.size()-1,p,y);};
        int target=-1;Vec3 focus{};
        if(wide||blocked){
            at(wide?Vec2{300,300}:Vec2{300,180},-80);cameraYaw=PI;police::setWantedLevel(1);ped_navigation::beginFrame();police::update(1.0f/60);
            if(blocked){if(!peds.empty())return false;focus={440,-62,180};message="BUILDER: POLICE WAIT FOR A REACHABLE FLOOR; NO SPAWN THROUGH THE ROOF";}
            else{
                if(peds.size()!=1||std::abs(jolt_world::pedHeight(0)+80)>2){logging::write("Police preview failed: no supported underground dispatch");return false;}
                for(int tick=0;tick<180;++tick){ped_navigation::beginFrame();police::update(1.0f/60);ai::update(1.0f/60,false);jolt_world::step(1.0f/60);}
                if(std::abs(jolt_world::pedHeight(0)+80)>2||bullets.empty()){char failure[160]{};std::snprintf(failure,sizeof(failure),"Police preview failed: underground pursuit/fire, Y %.2f, bullets %zu",jolt_world::pedHeight(0),bullets.size());logging::write(failure);return false;}
                if(normal){if(!builder::requestToggle()||!FinishBuilderTransition()||jolt_world::pedHeight(0)<-2||jolt_world::activeBuilderColliderCount()!=0)return false;}
                float y=jolt_world::pedHeight(0);at(peds[0].p+Vec2{-100,-35},y);focus={peds[0].p.x,y+18,peds[0].p.z};
                message=normal?"NORMAL: F5 RESTORES ORIGINAL GROUND AND POLICE SUPPORT":"BUILDER: POLICE PURSUE AND FIRE ON THE REACHABLE UNDERGROUND FLOOR";
            }
        }else{
            add({220,140},-80,0);at({195,140},-80);weapon=weapons::indexOf("knife");unlocked[weapon]=true;fireCooldown=0;
            if(witness)add({220,200},0,-PI/2);target=stealthTarget();if(target!=0)return false;
            if(witness){stealthKill();for(int tick=0;tick<100;++tick){ped_navigation::beginFrame();police::update(1.0f/60);jolt_world::step(1.0f/60);}
                if(peds[0].alive||police::wantedLevel()!=0)return false;Vec3 low{},high{};jolt_world::corpsePose(peds[0],low,high,focus);
                at(peds[0].p+Vec2{-35,-6},-80);message="BUILDER: RETAINED ROOF HIDES THE TAKEDOWN FROM A SURFACE WITNESS";
            }else{focus={220,-62,140};message="BUILDER: K TAKEDOWN REACHES A VICTIM ON THE SAME UNDERGROUND FLOOR";}
        }
        auto direction=norm(focus-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
        cameraMode=CameraMode::FirstWide;debug_menu::flyMode=true;jolt_world::teleportCharacter(player,playerY);builder::handleKey('9');builder::update(.01f);messageTime=30;
        auto stats=ped_navigation_surface::stats();char result[320]{};std::snprintf(result,sizeof(result),"Police review: mode %s, actors %zu, wanted %d, target %d, player Y %.2f, actor Y %.2f, plans %u, queries %u",
            builder::active()?"builder":"normal",peds.size(),police::wantedLevel(),target,playerY,peds.empty()?0:jolt_world::pedHeight(0),stats.plans,stats.physicsQueries);logging::write(result);
    }
    if(std::strstr(commandLine,"--builder-navigation-preview")){
        // Navigation-only geometry fixture; acquisition is verified separately.
        builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();pickups.clear();wildlife::animals.clear();birds::flock.clear();
        occupied=-1;ui::grassDistance=0;gameHour=12;player=previousPlayer={150,35};playerY=100;jolt_world::reset();
        if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        bool streaming=std::strstr(commandLine,"--navigation-streaming")!=nullptr;
        bool underground=streaming||std::strstr(commandLine,"--navigation-tunnel")!=nullptr;
        if(underground){for(int x=2;x<=(streaming?36:12);++x)for(int z=2;z<=4;++z)builder::mineTerrain({x,-2,z});}
        else{for(int x=5;x<=7;++x)for(int z=2;z<=4;++z)builder::mineTerrain({x,-1,z});}
        Ped walker{};walker.id="navigation-review";walker.p=walker.target={150,140};walker.speed=65;walker.shirt=rgb(218,109,46);
        peds.push_back(walker);jolt_world::addPed();float floor=underground?-80.0f:0.0f;jolt_world::teleportPed(0,walker.p,floor);
        if(std::strstr(commandLine,"--navigation-normal"))if(!builder::requestToggle()||!FinishBuilderTransition())return false;
        // Freeze after actual Jolt movement; capture the pit detour or roofed passage.
        Vec3 goal{370,floor,140};for(int tick=0;tick<(underground?80:100);++tick){ped_navigation::beginFrame();
            jolt_world::movePed(0,ped_navigation::velocityAtHeight(peds[0],goal,65,1.0f/60),1.0f/60);jolt_world::step(1.0f/60);}
        if(streaming){
            player=previousPlayer={900,300};playerY=0;jolt_world::teleportCharacter(player,0);
            peds[0].state=PedState::Investigate;peds[0].target={1100,140};peds[0].alertTime=30;
            for(int tick=0;tick<90;++tick){ai::update(1.0f/60);jolt_world::step(1.0f/60);}
            float cachedHeight=jolt_world::pedHeight(0);player=previousPlayer={2000,300};jolt_world::teleportCharacter(player,0);
            if(jolt_world::activePedCharacterCount()!=0||std::abs(jolt_world::pedHeight(0)-cachedHeight)>.01f)return false;
            player=previousPlayer={400,300};jolt_world::teleportCharacter(player,0);
            if(jolt_world::activePedCharacterCount()!=1||std::abs(jolt_world::pedHeight(0)-cachedHeight)>.01f)return false;
            for(int tick=0;tick<90;++tick){ai::update(1.0f/60);jolt_world::step(1.0f/60);}
            if(jolt_world::activePedCharacterCount()!=1||std::abs(jolt_world::pedHeight(0)+80)>2)return false;
            char checkpoint[200]{};std::snprintf(checkpoint,sizeof(checkpoint),"Streaming review: released capsule retained %.2f height; rebuilt capsule resumed at %.2f",cachedHeight,jolt_world::pedHeight(0));logging::write(checkpoint);
        }
        if(underground){player=previousPlayer={110,140};playerY=-80;cameraYaw=0;cameraPitch=.05f;}
        if(streaming)player=previousPlayer={peds[0].p.x-100,140};
        else if(!underground){player=previousPlayer={185,25};playerY=100;auto direction=norm(Vec3{265,0,140}-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});
            cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);}
        cameraMode=CameraMode::FirstWide;debug_menu::flyMode=true;jolt_world::teleportCharacter(player,playerY);builder::update(.01f);
        message=underground?"BUILDER: WALKING BELOW THE RETAINED ROOF":builder::active()?"BUILDER: PEDESTRIAN ROUTES AROUND THE PIT":"NORMAL: TERRAIN AND DIRECT ROUTE RESTORED";messageTime=30;
        if(streaming)message="BUILDER: STREAMED PEDESTRIAN RESUMES ON THE SAME UNDERGROUND FLOOR";
        char result[200]{};std::snprintf(result,sizeof(result),"Navigation review: mode %s, pedestrian %.2f %.2f %.2f",builder::active()?"builder":"normal",peds[0].p.x,jolt_world::pedHeight(0),peds[0].p.z);logging::write(result);
    }
    if(!regions::waterAt(player)&&
       !std::strstr(commandLine,"--excavation-preview")&&
       !std::strstr(commandLine,"--builder-navigation-preview")&&
       !std::strstr(commandLine,"--builder-destination-preview")&&
       !std::strstr(commandLine,"--builder-wildlife-preview")&&
       !std::strstr(commandLine,"--builder-interaction-preview")&&
       !std::strstr(commandLine,"--builder-police-preview")&&
       (std::strstr(commandLine,"--terrain-")||playerY==0||playerY<game::groundHeight(player))){
        playerY=game::groundHeight(player);jolt_world::teleportCharacter(player,playerY);
    }
    return true;
}
} // namespace app
