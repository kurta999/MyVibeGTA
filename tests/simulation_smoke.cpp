#include "../src/game.h"
#include "../src/game_internal.h"
#include "../src/physics.h"
#include "../src/props.h"
#include "../src/savegame.h"
#include "../src/weapons.h"
#include "../src/camera.h"
#include "../src/input.h"
#include "../src/ui.h"
#include "../src/ai.h"
#include "../src/content.h"
#include "../src/data_file.h"
#include "../src/fire.h"
#include "../src/police.h"
#include "../src/commerce.h"
#include "../src/traversal.h"
#include "../src/weather.h"
#include "../src/regions.h"
#ifdef MINI_CITY_JOLT
#include "../src/debug_menu.h"
#endif
#ifdef MINI_CITY_JOLT
#include "../src/jolt_world.h"
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

double connectedPlayableArea(){
    constexpr int step=100;
    constexpr int columns=int(regions::WIDTH)/step;
    constexpr int rows=int(regions::DEPTH)/step;
    std::vector<unsigned char> passable(columns*rows),visited(columns*rows);
    auto index=[=](int x,int z){return z*columns+x;};
    auto center=[=](int x,int z){return game::Vec2{(x+0.5f)*step,(z+0.5f)*step};};
    int start=-1;
    for(int z=0;z<rows;++z)for(int x=0;x<columns;++x){
        game::Vec2 point=center(x,z);
        int cell=index(x,z);
        passable[cell]=!game::solid(point,1);
        if(passable[cell]&&game::len(point-game::player)<100&&start<0)start=cell;
    }
    assert(start>=0);
    std::vector<int> queue{start};visited[start]=1;
    int conservativeCells=0;
    for(std::size_t head=0;head<queue.size();++head){
        int cell=queue[head],x=cell%columns,z=cell/columns;
        game::Vec2 point=center(x,z);
        bool wholeCell=true;
        for(game::Vec2 offset: {game::Vec2{25,25},{25,-25},{-25,25},{-25,-25}})
            if(game::solid(point+offset,1)){wholeCell=false;break;}
        if(wholeCell)++conservativeCells;
        for(game::Vec2 direction: {game::Vec2{1,0},{-1,0},{0,1},{0,-1}}){
            int nx=x+int(direction.x),nz=z+int(direction.z);
            if(nx<0||nz<0||nx>=columns||nz>=rows)continue;
            int neighbor=index(nx,nz);
            if(!passable[neighbor]||visited[neighbor]||
               game::solid(point+direction*(step*0.5f),1))continue;
            visited[neighbor]=1;queue.push_back(neighbor);
        }
    }
    return double(conservativeCells)*step*step;
}

int main(){
    std::srand(1);
    char currentDirectory[MAX_PATH]{};GetCurrentDirectoryA(MAX_PATH,currentDirectory);
    std::string weaponConfig=std::string(currentDirectory)+"\\weapons-test.ini";
    std::ifstream original(data_file::resourcePath("weapons.ini"));
    assert(original);
    std::string config((std::istreambuf_iterator<char>(original)),std::istreambuf_iterator<char>());
    auto damage=config.find("Damage=52");
    assert(damage!=std::string::npos);
    config.replace(damage,9,"Damage=77");
    {std::ofstream data(weaponConfig);data<<config;}
    assert(weapons::load(weaponConfig.c_str()));
    assert(weapons::stats(0).damage==77);
    assert(weapons::indexOf("sniper")==4&&weapons::indexOf("rpg")==5&&
        weapons::indexOf("water-cannon")==8&&
        weapons::indexOf("silenced-pistol")==9&&
        weapons::indexOf("katana")==10&&weapons::indexOf("bat")==15&&
        weapons::indexOf("bow")==16&&weapons::count()==17);
    auto duplicate=config.find("Id=smg");
    assert(duplicate!=std::string::npos);
    config.replace(duplicate,6,"Id=pistol");
    {std::ofstream data(weaponConfig,std::ios::trunc);data<<config;}
    assert(!weapons::load(weaponConfig.c_str()));
    assert(weapons::lastError().find("Duplicate")!=std::string::npos);
    std::remove(weaponConfig.c_str());
    assert(!weapons::load(weaponConfig.c_str()));
    assert(weapons::stats(0).damage==52);
    assert(weapons::load());
    assert(weapons::stats(weapons::indexOf("pistol")).projectileSpeed>=2800);
    assert(weapons::stats(weapons::indexOf("sniper")).projectileSpeed>=6000);
    assert(weapons::stats(weapons::indexOf("rpg")).projectileSpeed==500);
    assert(physics::load());
    assert(fire::load());
    assert(police::load());
    assert(commerce::load());
    assert(traversal::load());
    assert(weather::load()&&weather::states().size()==5);
    assert(regions::load()&&regions::all().size()==7&&
        regions::species().size()==30&&regions::roads().size()==16&&
        regions::hubs().size()==4);
    assert(regions::biomeAt({8750,9300})==regions::Biome::City);
    assert(regions::roadAt({8500,9300}));
    assert(regions::marinaBayAt({8400,10360})&&regions::waterAt({8400,10360}));
    assert(regions::marinaPierAt({8500,10390})&&!regions::waterAt({8500,10390}));
    std::ifstream originalRegions(data_file::resourcePath("regions.ini"));
    assert(originalRegions);
    std::string regionData((std::istreambuf_iterator<char>(originalRegions)),
        std::istreambuf_iterator<char>());
    auto cityVehicleCount=regionData.find("CityVehicles=12");
    assert(cityVehicleCount!=std::string::npos);
    regionData.replace(cityVehicleCount,std::string("CityVehicles=12").size(),
        "CityVehicles=13");
    std::string regionConfig=std::string(currentDirectory)+"\\regions-test.ini";
    {std::ofstream data(regionConfig);data<<regionData;}
    assert(regions::load(regionConfig.c_str()));
    game::reset();
    assert(std::count_if(game::buildings.begin(),game::buildings.end(),
        [](const game::Building& building){return building.id.rfind("marina-",0)==0;})==15);
    assert(std::count_if(game::vehicles.begin(),game::vehicles.end(),
        [](const game::Vehicle& vehicle){return vehicle.id.rfind("marina-boat-",0)==0;})==3);
    auto countEastVehicles=[](){return std::count_if(game::vehicles.begin(),
        game::vehicles.end(),[](const game::Vehicle& vehicle){
            return vehicle.id.rfind("east-vehicle-",0)==0;
        });};
    assert(countEastVehicles()==13);
    auto cityVehicleCycle=regionData.find("VehicleCycle=sport,car,car");
    assert(cityVehicleCycle!=std::string::npos);
    regionData.replace(cityVehicleCycle+std::string("VehicleCycle=").size(),5,
        "hover");
    {std::ofstream data(regionConfig,std::ios::trunc);data<<regionData;}
    assert(!regions::load(regionConfig.c_str()));
    assert(regions::lastError().find("VehicleCycle")!=std::string::npos);
    game::reset();
    assert(countEastVehicles()==13);
    std::remove(regionConfig.c_str());
    assert(regions::load());
    game::reset();
    const double playableArea=connectedPlayableArea();
    std::printf("Connected playable area (conservative 100-unit grid): %.2f million square units\n",
        playableArea/1000000.0);
    assert(playableArea>264000000.0);
    assert(fire::groundAt({1050,920})==fire::Material::Grass);
    assert(fire::groundAt({300,250})==fire::Material::Asphalt);
    assert(fire::groundAt({300,game::SHORE+10})==fire::Material::Water);
    fire::reset();
    assert(fire::ignite({1050,920}));
    for(int tick=0;tick<60;++tick)fire::update(0.05f);
    assert(fire::active().size()>1);
    fire::extinguish({1050,920},100,30);
    assert(fire::active().empty());
    assert(!fire::ignite({1050,920}));
    fire::reset();
    assert(fire::ignite({1050,920},fire::Material::Metal));
    for(int tick=0;tick<60;++tick)fire::update(0.05f);
    assert(fire::active().size()<=1);
    fire::reset();
    assert(weather::set("rain"));
    assert(fire::ignite({1050,920}));
    for(int tick=0;tick<100;++tick)fire::update(0.05f);
    assert(fire::active().empty());
    assert(!fire::ignite({1050,920}));
    weather::reset();fire::reset();
    game::reset();
    game::peds[0].p={1250,1450};
    fire::ignitePed(game::peds[0]);
    assert(game::peds[0].burnTime>0);
    for(int tick=0;tick<20;++tick)fire::update(0.05f);
    assert(game::peds[0].alive&&game::peds[0].health<100);
    for(int tick=0;tick<200&&game::peds[0].alive;++tick)fire::update(0.05f);
    assert(!game::peds[0].alive&&game::peds[0].burnTime==0);
    game::reset();
    int burningCar=int(game::vehicles.size())-6;
    game::damageVehicle(burningCar,200);
    assert(game::vehicles[burningCar].burnTime>0&&!game::vehicles[burningCar].exploded);
    float burningCarHealth=game::vehicleHealth(burningCar);
    for(int tick=0;tick<10;++tick)fire::update(0.05f);
    assert(!game::vehicles[burningCar].exploded&&
        game::vehicleHealth(burningCar)<burningCarHealth);
    fire::extinguish(game::vehicles[burningCar].p,30,3);
    assert(game::vehicles[burningCar].burnTime==0);
    fire::igniteVehicle(game::vehicles[burningCar]);
    for(int tick=0;tick<100&&!game::vehicles[burningCar].exploded;++tick)
        fire::update(0.05f);
    assert(game::vehicles[burningCar].exploded);
    game::reset();
    assert(weather::set("clear",0.1f));
    weather::update(0.2f);
    assert(weather::current().id=="windy");
    weather::reset();
    assert(fire::groundAt({4000,9000})==fire::Material::Grass);
    assert(fire::groundAt({4000,14500})==fire::Material::Snow);
    assert(fire::groundAt({12000,3500})==fire::Material::Sand);
    assert(fire::groundAt({7800,8000})==fire::Material::Water);
    assert(fire::groundAt({7800,8500})==fire::Material::Asphalt);
    assert(fire::groundAt({1200,2000})==fire::Material::Asphalt);
    fire::reset();
    assert(fire::ignite({4000,9000}));
    for(int tick=0;tick<60;++tick)fire::update(0.05f);
    assert(fire::active().size()>1);
    fire::reset();
    assert(!fire::ignite({4000,14500}));
    assert(!fire::ignite({7800,8000}));
    assert(std::abs(physics::tractionAt({7800,8500})-1.0f)<0.001f);
    assert(std::abs(physics::tractionAt({4000,9000})-0.78f)<0.001f);
    assert(std::abs(physics::tractionAt({12000,3500})-0.62f)<0.001f);
    assert(std::abs(physics::tractionAt({4000,14500})-0.45f)<0.001f);
    assert(weather::set("rain"));
    assert(physics::tractionAt({7800,8500})<1.0f);
    weather::reset();
    assert(physics::tuning(game::Kind::Car).maxHealth==250);
    std::ifstream originalVehicles(data_file::resourcePath("vehicles.ini"));
    assert(originalVehicles);
    std::string vehicleData((std::istreambuf_iterator<char>(originalVehicles)),std::istreambuf_iterator<char>());
    auto acceleration=vehicleData.find("Acceleration=245");
    assert(acceleration!=std::string::npos);
    vehicleData.replace(acceleration,16,"Acceleration=246");
    std::string vehicleConfig=std::string(currentDirectory)+"\\vehicles-test.ini";
    {std::ofstream data(vehicleConfig);data<<vehicleData;}
    assert(physics::load(vehicleConfig.c_str()));
    assert(physics::tuning(game::Kind::Car).acceleration==246);
    std::remove(vehicleConfig.c_str());
    assert(physics::load());
    game::reset();
#ifdef MINI_CITY_JOLT
    assert(!debug_menu::open&&!debug_menu::godMode&&!debug_menu::flyMode);
    debug_menu::toggle();
    assert(debug_menu::open&&debug_menu::entryCount()==debug_menu::WEAPONS_START+weapons::count());
    input::windowProc(nullptr,WM_KEYDOWN,VK_F11,0);
    assert(game::screenshotRequested);
    game::screenshotRequested=false;
    debug_menu::handleKey(VK_RETURN);
    assert(debug_menu::godMode);
    game::applyDamage(40);
    assert(game::health==game::PLAYER_MAX_HEALTH);
    debug_menu::handleKey(VK_DOWN);
    debug_menu::handleKey(VK_RETURN);
    assert(debug_menu::flyMode);
    game::keys[VK_SPACE]=true;
    game::update(1.0f/60.0f);
    game::keys[VK_SPACE]=false;
    assert(game::playerY>0);
    debug_menu::handleKey(VK_RETURN);
    assert(!debug_menu::flyMode);
    debug_menu::handleKey(VK_DOWN);
    game::health=23;
    debug_menu::handleKey(VK_RETURN);
    assert(game::health==game::PLAYER_MAX_HEALTH);
    debug_menu::selection=3;
    game::gameHour=23.5f;
    debug_menu::handleKey(VK_RIGHT);
    assert(std::abs(game::gameHour-0.5f)<0.001f);
    debug_menu::handleKey(VK_LEFT);
    assert(std::abs(game::gameHour-23.5f)<0.001f);
    debug_menu::selection=4;
    weather::set(weather::states().front().id);
    debug_menu::handleKey(VK_LEFT);
    assert(weather::current().id==weather::states().back().id);
    debug_menu::handleKey(VK_RIGHT);
    assert(weather::current().id==weather::states().front().id);
    debug_menu::selection=debug_menu::WEAPONS_START+weapons::count()-1;
    debug_menu::handleKey(VK_RETURN);
    assert(!debug_menu::open&&game::weapon==weapons::count()-1&&
        game::unlocked[game::weapon]&&game::magazine[game::weapon]>0);
    game::reset();
    assert(!debug_menu::open&&!debug_menu::godMode&&!debug_menu::flyMode);
    game::Ped& struck=game::peds[0];
    struck.p={300,250};struck.health=100;struck.armor=0;
    game::Vehicle impactCar{};
    impactCar.kind=game::Kind::Car;impactCar.p={290,250};
    impactCar.velocity={30,0};impactCar.speed=30;
    assert(ai::vehicleImpact(struck,impactCar));
    assert(struck.alive&&struck.health<100&&struck.knockedDown==0);
    assert(impactCar.speed==30);
    struck.vehicleImpactCooldown=0;
    impactCar.velocity={70,0};impactCar.speed=70;
    assert(ai::vehicleImpact(struck,impactCar));
    assert(struck.alive&&struck.knockedDown>0&&struck.impactAnimationTotal>0);
    struck.vehicleImpactCooldown=0;
    impactCar.velocity={140,0};impactCar.speed=140;
    assert(ai::vehicleImpact(struck,impactCar)&&!struck.alive);
    assert(impactCar.speed==140);
    game::reset();
    auto starter=std::find_if(game::vehicles.begin(),game::vehicles.end(),
        [](const game::Vehicle& vehicle){return vehicle.id=="starter-car";});
    assert(starter!=game::vehicles.end());
    game::occupied=int(starter-game::vehicles.begin());
    starter->speed=70;starter->velocity=game::forward(starter->angle)*70;
    game::peds[0].p=starter->p+game::Vec2{10,0};
    game::peds[0].health=100;game::peds[0].armor=0;
    ai::update(1.0f/60.0f);
    assert(game::peds[0].knockedDown>0&&starter->speed==70);
    game::reset();
    assert(jolt_world::activeBuildingColliderCount()>0);
    assert(jolt_world::activeBuildingColliderCount()<game::buildings.size());
    assert(jolt_world::activePedCharacterCount()>0);
    assert(jolt_world::activePedCharacterCount()<game::peds.size());
    auto eastStreamPed=std::find_if(game::peds.begin(),game::peds.end(),
        [](const game::Ped& ped){return ped.id=="east-ped-0";});
    assert(eastStreamPed!=game::peds.end());
    game::Vec2 eastStreet=eastStreamPed->p;
    jolt_world::teleportCharacter(eastStreet,0);
    game::player=eastStreet;
    assert(jolt_world::activeBuildingColliderCount()>0);
    assert(jolt_world::activeBuildingColliderCount()<game::buildings.size());
    for(int tick=0;tick<2;++tick)game::update(1.0f/60.0f);
    assert(jolt_world::activePedCharacterCount()>0);
    game::Vec2 emptyCountry{4000,9000};
    jolt_world::teleportCharacter(emptyCountry,0);
    game::player=emptyCountry;
    assert(jolt_world::activeBuildingColliderCount()==0);
    assert(jolt_world::activePedCharacterCount()==0);
    game::reset();
#endif
    assert(regions::WIDTH*regions::DEPTH>=
        50.0f*game::WORLD_W*game::WORLD_D);
    int dryTiles=0;
    for(float z=50;z<regions::DEPTH;z+=100)
        for(float x=50;x<regions::WIDTH;x+=100)
            if(!regions::waterAt({x,z}))++dryTiles;
    assert(dryTiles*10000.0f>50.0f*game::WORLD_W*game::WORLD_D);
    assert(regions::biomeAt({12000,8500})==regions::Biome::City);
    assert(regions::biomeAt({4000,14000})==regions::Biome::Snow);
    assert(regions::biomeAt({12000,3000})==regions::Biome::Desert);
    assert(regions::biomeAt({12000,14000})==regions::Biome::Savanna);
    assert(regions::roadAt({1200,14000})&&regions::roadAt({12000,3000})&&
        regions::roadAt({12000,14000})&&regions::roadAt({7800,8500}));
    assert(regions::waterAt({7800,8000})&&!regions::waterAt({7800,8500}));
    assert(!regions::waterAt({1200,2000})&&regions::waterAt({1300,2000})&&
        regions::roadAt({1200,2000})&&!game::solid({1200,2000},10));
    for(const auto& prop:game::props)assert(!regions::causewayAt(prop.p));
    assert(game::buildings.size()>60&&game::peds.size()>120&&
        game::vehicles.size()>30&&game::trees.size()>6000);
    int regionalTraffic=0;
    for(const auto& vehicle:game::vehicles)if(vehicle.trafficRoute>=0){
        ++regionalTraffic;
        assert(vehicle.id.rfind("traffic-",0)==0&&regions::roadAt(vehicle.p));
    }
    assert(regionalTraffic>=8);
    for(const auto& hub:regions::hubs()){
        auto ped=std::find_if(game::peds.begin(),game::peds.end(),
            [&](const game::Ped& item){return item.id==hub.id+"-ped-0";});
        auto vehicle=std::find_if(game::vehicles.begin(),game::vehicles.end(),
            [&](const game::Vehicle& item){return item.id==hub.id+"-vehicle-0";});
        assert(ped!=game::peds.end()&&vehicle!=game::vehicles.end());
        assert(game::len(ped->p-hub.p)<140&&regions::roadAt(vehicle->p));
    }
    auto regionalPed=std::find_if(game::peds.begin(),game::peds.end(),
        [](const game::Ped& item){return item.id=="snow-outpost-ped-0";});
    assert(regionalPed!=game::peds.end());
    game::Vec2 snowHome=regionalPed->p;
    regionalPed->alive=false;regionalPed->respawn=0.01f;
    ai::update(0.02f);
    assert(regionalPed->alive&&
        game::len(regionalPed->p-snowHome)<0.01f);
    auto sleepingPed=std::find_if(game::peds.begin(),game::peds.end(),
        [](const game::Ped& item){return item.id=="snow-outpost-ped-1";});
    assert(sleepingPed!=game::peds.end());
    game::Vec2 sleepingPosition=sleepingPed->p;
    ai::update(0.05f);
    assert(game::len(sleepingPed->p-sleepingPosition)<0.01f);
    game::player=regions::hubs()[1].p;
    ai::update(0.05f);
    assert(game::len(sleepingPed->p-sleepingPosition)>0.1f);
    game::player={300,250};
    auto eastPed=std::find_if(game::peds.begin(),game::peds.end(),
        [](const game::Ped& item){return item.id=="east-ped-0";});
    assert(eastPed!=game::peds.end());
    game::Vec2 eastHome=eastPed->p;
    eastPed->alive=false;eastPed->respawn=0.01f;
    ai::update(0.02f);
    assert(eastPed->alive&&game::len(eastPed->p-eastHome)<0.01f);
    std::set<std::string> regionalModels;
    for(const auto& tree:game::trees)if(!tree.modelId.empty())
        regionalModels.insert(tree.modelId);
    assert(regionalModels.size()==regions::species().size());
    auto nearbyTrees=regions::nearbyTreeIndices({4000,14000},600);
    std::set<int> nearbySet(nearbyTrees.begin(),nearbyTrees.end());
    assert(nearbySet.size()==nearbyTrees.size());
    for(int index=0;index<int(game::trees.size());++index)
        if(game::len(game::trees[index].p-game::Vec2{4000,14000})<=600)
            assert(nearbySet.count(index));
    int targetTree=-1;
    for(int index=0;index<int(game::trees.size());++index){
        const auto& tree=game::trees[index];
        if(regions::biomeAt(tree.p)!=regions::Biome::Countryside||
           tree.height<30||tree.p.x<100||tree.p.x>regions::WIDTH-100)continue;
        bool isolated=true;
        for(int neighbor:regions::nearbyTreeIndices(tree.p,35))
            if(neighbor!=index&&game::len(game::trees[neighbor].p-tree.p)<35)
                isolated=false;
        for(const auto& vehicle:game::vehicles)
            if(game::len(vehicle.p-tree.p)<60)isolated=false;
        for(const auto& ped:game::peds)
            if(game::len(ped.p-tree.p)<60)isolated=false;
        for(const auto& prop:game::props)
            if(game::len(prop.p-tree.p)<60)isolated=false;
        if(isolated){targetTree=index;break;}
    }
    assert(targetTree>=0);
    const auto treePoint=game::trees[targetTree].p;
    game::Bullet treeShot{};
    treeShot.p={treePoint.x-18,12,treePoint.z};
    treeShot.v={1200,0,0};treeShot.life=1;
    treeShot.damage=12;treeShot.range=600;
    game::bullets.push_back(treeShot);
    game::update(1.0f/60.0f);
    assert(game::bullets.empty()&&game::trees[targetTree].health==88);
    treeShot.rocket=true;treeShot.damage=10;
    treeShot.explosionRadius=60;treeShot.explosionDamage=20;
    game::bullets.push_back(treeShot);
    game::update(1.0f/60.0f);
    assert(game::trees[targetTree].burning&&
        game::trees[targetTree].health<88);
    int forestTrees=0,giantTrees=0,landmarkTrees=0;
    for(const auto& tree:game::trees)
        if(tree.id.rfind("forest-tree-",0)==0){
            ++forestTrees;
            if(tree.scale>=5)++giantTrees;
            if(tree.scale>=9.5f)++landmarkTrees;
            assert(regions::biomeAt(tree.p)==regions::Biome::Countryside);
            assert(!regions::roadAt(tree.p)&&!game::solid(tree.p,1));
        }
    assert(forestTrees>1800);
    assert(giantTrees>=20&&landmarkTrees>=5);
    int groveTrees=0;
    for(int index:regions::nearbyTreeIndices({4000,9000},340))
        if(game::trees[index].id.rfind("forest-tree-",0)==0)++groveTrees;
    assert(groveTrees>75);
    int bushes=0;
    std::set<std::string> bushModels;
    for(const auto& prop:regions::decorations()){
        if(prop.modelId.rfind("bush_",0)==0){
            ++bushes;bushModels.insert(prop.modelId);
            assert(regions::biomeAt(prop.p)==regions::Biome::Countryside);
            assert(!regions::roadAt(prop.p)&&!game::solid(prop.p,1));
        }else assert(regions::biomeAt(prop.p)==regions::Biome::Desert);
    }
    assert(bushes>4500&&bushModels.size()==36);
    assert(regions::decorations().size()==size_t(900+bushes));
    const auto& desertProp=regions::decorations().front();
    auto nearbyProps=regions::nearbyDecorationIndices(desertProp.p,50);
    assert(std::find(nearbyProps.begin(),nearbyProps.end(),0)!=nearbyProps.end());
    assert(nearbyProps.size()<regions::decorations().size());
    assert(fire::surfaceAt({desertProp.p.x,desertProp.height*0.5f,desertProp.p.z})==
        (desertProp.modelId.rfind("cactus_",0)==0?
            fire::Material::Wood:fire::Material::Concrete));
    game::player={1200,1820};game::playerY=0;
#ifdef MINI_CITY_JOLT
    for(int tick=0;tick<100;++tick)
        jolt_world::moveCharacter({0,400},false,1.0f/60.0f);
    assert(game::player.z>2200&&!game::swimming);
#endif
    game::player={7560,8500};game::playerY=0;
#ifdef MINI_CITY_JOLT
    for(int tick=0;tick<110;++tick)
        jolt_world::moveCharacter({400,0},false,1.0f/60.0f);
    assert(game::player.x>8050&&!game::swimming);
    game::player={300,250};
    int crossingCar=int(game::vehicles.size())-6;
    jolt_world::teleportVehicle(crossingCar,{7540,8500},0);
    for(int tick=0;tick<360;++tick){
        jolt_world::driveVehicle(crossingCar,1,0,1.0f/60.0f);
        jolt_world::step(1.0f/60.0f);
    }
    assert(game::vehicles[crossingCar].p.x>8050&&
        std::abs(game::vehicles[crossingCar].p.z-8500)<100);
    jolt_world::teleportVehicle(crossingCar,{1200,1800},game::PI/2);
    for(int tick=0;tick<360;++tick){
        jolt_world::driveVehicle(crossingCar,1,0,1.0f/60.0f);
        jolt_world::step(1.0f/60.0f);
    }
    assert(game::vehicles[crossingCar].p.z>2230&&
        std::abs(game::vehicles[crossingCar].p.x-1200)<60);
    auto traffic=std::find_if(game::vehicles.begin(),game::vehicles.end(),
        [](const game::Vehicle& vehicle){return vehicle.trafficRoute>=0&&
            vehicle.id.find("snow-route")!=std::string::npos;});
    assert(traffic!=game::vehicles.end());
    game::Vec2 trafficStart=traffic->p;
    game::player=trafficStart+game::Vec2{0,75};
    jolt_world::teleportCharacter(game::player,0);
    for(int tick=0;tick<180;++tick)game::update(1.0f/60.0f);
    assert(game::len(traffic->p-trafficStart)>25&&
        regions::roadAt(traffic->p)&&!traffic->exploded);
#endif
    game::reset();
    assert(traversal::lastError().empty());
    assert(traversal::ladders.size()>12&&traversal::trees.size()>500);
    const auto ladder=traversal::ladders.front();
    game::player=ladder.bottom;
    assert(traversal::startLadder(0));
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    for(int tick=0;tick<260&&traversal::active();++tick)traversal::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    assert(!traversal::active());
    assert(game::len(game::player-ladder.roof)<1&&game::playerY>ladder.height);
    for(int tick=0;tick<60;++tick)game::update(1.0f/60.0f);
    assert(game::playerY>=ladder.height-2);
    assert(weather::set("rain",30));
    assert(savegame::save()&&savegame::load());
    assert(game::playerY>=ladder.height-2);
    assert(weather::current().id=="rain");
    auto eastLadder=std::find_if(traversal::ladders.begin(),traversal::ladders.end(),
        [](const traversal::Ladder& item){
            return item.buildingId.rfind("east-city-",0)==0;
        });
    assert(eastLadder!=traversal::ladders.end());
    int eastLadderIndex=int(std::distance(traversal::ladders.begin(),eastLadder));
    game::player=eastLadder->bottom;game::playerY=0;
    assert(traversal::startLadder(eastLadderIndex));
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    for(int tick=0;tick<320&&traversal::active();++tick)
        traversal::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    assert(!traversal::active()&&game::playerY>eastLadder->height);
    const auto climbTree=traversal::trees.front();
    game::player=climbTree.bottom;game::playerY=0;
    assert(traversal::startTree(0));
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    for(int tick=0;tick<30;++tick)traversal::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    assert(traversal::active()&&game::playerY>15);
    game::keys[VK_SPACE]=true;traversal::update(1.0f/60.0f);
    game::keys[VK_SPACE]=false;
    assert(!traversal::active());
    auto regionalClimb=std::find_if(traversal::trees.begin(),traversal::trees.end(),
        [](const traversal::ClimbTree& tree){return tree.generated;});
    assert(regionalClimb!=traversal::trees.end());
    int regionalClimbIndex=int(std::distance(traversal::trees.begin(),regionalClimb));
    game::player=regionalClimb->bottom;game::playerY=0;
    assert(traversal::startTree(regionalClimbIndex));
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    for(int tick=0;tick<20;++tick)traversal::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    assert(game::playerY>10);
    traversal::detach();
    game::reset();
    assert(commerce::shops.size()==8&&commerce::houses.size()==14&&commerce::items.size()==26);
    for(const auto& shop:commerce::shops)assert(!game::solid(shop.p,12));
    for(const auto& house:commerce::houses){
        assert(!game::solid(house.p,12));
        assert(!game::solid(house.p+game::Vec2{0,20},12));
        for(int slot=0;slot<house.slots;++slot)
            assert(!game::solid(house.p+game::Vec2{35+slot*30.0f,0},18));
    }
    game::money=10000;game::armor=20;
    game::player=commerce::shops[0].p;
    commerce::openShop(0);
    assert(commerce::menu()==commerce::Menu::Shop);
    int cash=game::money;
    assert(commerce::buyItem(1));
    assert(game::unlocked[weapons::indexOf("smg")]&&game::money==cash-350);
    assert(commerce::buyItem(12)&&game::armor==70);
    police::setWantedLevel(2);
    assert(commerce::buyItem(13)&&police::wantedLevel()==1);
    assert(commerce::buyItem(15));
    assert(commerce::buyItem(19));
    assert(game::weapon==weapons::indexOf("katana")&&game::ammo[game::weapon]<0);
    int ownedVehicle=-1;
    for(int index=0;index<int(game::vehicles.size());++index)
        if(game::vehicles[index].owned){assert(ownedVehicle<0);ownedVehicle=index;}
    assert(ownedVehicle>=0&&game::vehicles[ownedVehicle].kind==game::Kind::Car);
    commerce::close();
    game::player=commerce::houses[0].p;
    assert(commerce::buyHouse(0)&&commerce::houses[0].owned);
    game::player=commerce::houses[1].p;
    assert(commerce::buyHouse(1)&&commerce::ownedHouseCount()==2);
    game::player=commerce::houses[0].p;
#ifdef MINI_CITY_JOLT
    jolt_world::teleportVehicle(ownedVehicle,commerce::houses[0].p+game::Vec2{20,0},0);
#else
    game::vehicles[ownedVehicle].p=commerce::houses[0].p+game::Vec2{20,0};
#endif
    assert(commerce::storeVehicle(0,ownedVehicle));
    assert(game::vehicles[ownedVehicle].garageHouseId==commerce::houses[0].id);
    game::Vec2 parked=game::vehicles[ownedVehicle].p;
    assert(savegame::save());
    assert(savegame::load());
    assert(commerce::ownedHouseCount()==2&&game::vehicles[ownedVehicle].owned);
    assert(game::vehicles[ownedVehicle].garageHouseId==commerce::houses[0].id);
    assert(game::len(game::vehicles[ownedVehicle].p-parked)<2);
    assert(!commerce::fastTravel(1));
    police::setWantedLevel(0);
    assert(commerce::fastTravel(1));
    assert(game::len(game::player-commerce::houses[1].p)<25);
    game::player=commerce::houses[8].p;
    assert(commerce::buyHouse(8)&&commerce::ownedHouseCount()==3);
    game::player=commerce::houses[0].p;
    assert(commerce::fastTravel(8));
    assert(game::len(game::player-commerce::houses[8].p)<25);
    assert(savegame::save()&&savegame::load());
    assert(game::len(game::player-commerce::houses[8].p)<25&&
        commerce::ownedHouseCount()==3);
    game::money=10000;game::player=commerce::houses[11].p;
    assert(commerce::buyHouse(11));
    game::player=commerce::houses[8].p;
    assert(commerce::fastTravel(11));
    assert(regions::biomeAt(game::player)==regions::Biome::Snow);
    assert(savegame::save()&&savegame::load()&&commerce::houses[11].owned);
    assert(game::unlocked[weapons::indexOf("katana")]&&
        game::ammo[weapons::indexOf("katana")]<0);
    game::money=100000;
    for(int index=0;index<int(commerce::houses.size());++index){
        if(commerce::houses[index].owned)continue;
        game::player=commerce::houses[index].p;
        bool room=commerce::ownedHouseCount()<10;
        assert(commerce::buyHouse(index)==room);
    }
    assert(commerce::ownedHouseCount()==10);
    game::peds.clear();game::buildings.clear();
    game::player={300,250};game::cameraYaw=0;
    game::Ped meleeTarget{};meleeTarget.p={350,250};meleeTarget.health=100;
    game::peds.push_back(meleeTarget);
    game::weapon=weapons::indexOf("katana");
    game::shoot();
    assert(game::peds[0].health==25&&game::bullets.empty());
    game::fireCooldown=0;game::weapon=0;game::keys[VK_SPACE]=true;
    game::shoot();
    game::keys[VK_SPACE]=false;
    assert(game::peds[0].health==3&&game::bullets.empty());
    game::reset();game::buildings.clear();
    game::player={300,250};
    for(auto& ped:game::peds){ped.alive=false;ped.respawn=999999;}
    game::Building wall{};wall.x=365;wall.z=220;wall.w=60;wall.d=60;wall.h=60;
    game::buildings.push_back(wall);
    game::peds[0].alive=true;game::peds[0].p={350,250};
    game::peds[0].health=60;game::peds[0].armor=0;game::peds[0].knockedDown=1;
    game::Bullet arrow{};arrow.p={310,20,250};arrow.v={900,0,0};
    arrow.life=1;arrow.damage=100;arrow.range=600;arrow.arrow=true;arrow.silent=true;
    game::bullets.push_back(arrow);
    for(int tick=0;tick<8;++tick)game::update(1.0f/60.0f);
    assert(!game::peds[0].alive&&game::peds[0].pinned);
    assert(game::ragdollParts.size()==6);
    for(int tick=0;tick<30;++tick)game::update(1.0f/60.0f);
    assert(game::len(game::peds[0].p-game::peds[0].pinAnchor)<30);
    assert(savegame::save()&&savegame::load());
    assert(game::peds[0].pinned&&!game::peds[0].alive);
    for(int tick=0;tick<2;++tick)game::update(1.0f/60.0f);
    assert(game::ragdollParts.size()==6);
    for(int tick=0;tick<420;++tick)game::update(1.0f/60.0f);
    assert(game::peds[0].pinned&&game::ragdollParts.size()==6);
    game::reset();game::player={5100,5100};
    game::peds[0].p={5140,5100};game::peds[0].health=100;
    game::peds[0].armor=0;game::peds[0].knockedDown=1;
    game::Bullet fastBullet{};fastBullet.p={5110,20,5100};
    fastBullet.v={6000,0,0};fastBullet.life=1;
    fastBullet.damage=10;fastBullet.range=600;
    game::bullets.push_back(fastBullet);
    game::update(1.0f/60.0f);
    assert(game::peds[0].health==90);
    game::reset();game::buildings.clear();game::vehicles.clear();
    game::props.clear();game::trees.clear();game::player={5000,5000};
    for(auto& ped:game::peds){ped.alive=false;ped.respawn=999999;}
    game::peds[0].alive=true;game::peds[0].p={5147,5100};
    game::peds[0].health=100;game::peds[0].armor=0;
    game::peds[0].knockedDown=1;
    game::Bullet crossingShot{};crossingShot.p={5110,20,5100};
    crossingShot.v={96000,0,0};crossingShot.life=1;
    crossingShot.damage=20;crossingShot.range=3000;
    game::bullets.push_back(crossingShot);
    game::update(1.0f/60.0f);
    assert(game::bullets.empty()&&game::peds[0].health==80);
    game::reset();game::buildings.clear();
    for(auto& ped:game::peds){ped.alive=false;ped.respawn=999999;}
    game::peds[0].alive=true;game::peds[0].p={350,250};
    game::peds[0].health=100;game::peds[0].armor=60;
    game::peds[0].knockedDown=1;
    game::Bullet headshot{};headshot.p={310,32,250};headshot.v={900,0,0};
    headshot.life=1;headshot.damage=1;headshot.range=600;
    game::bullets.push_back(headshot);
    for(int tick=0;tick<8;++tick)game::update(1.0f/60.0f);
    assert(!game::peds[0].alive&&game::peds[0].armor==60);
    game::reset();game::buildings.clear();game::vehicles.clear();
    game::props.clear();game::trees.clear();game::player={5100,5100};
    for(auto& ped:game::peds){ped.alive=false;ped.respawn=999999;}
    game::peds[0].alive=true;game::peds[0].p={5140,5100};
    game::peds[0].health=100;game::peds[0].armor=0;game::peds[0].knockedDown=1;
    game::Bullet legShot{};legShot.p={5110,5,5100};legShot.v={900,0,0};
    legShot.life=1;legShot.damage=20;legShot.range=600;
    game::bullets.push_back(legShot);
    for(int tick=0;tick<8;++tick)game::update(1.0f/60.0f);
    assert(game::peds[0].alive&&game::peds[0].health==90);
    game::reset();game::buildings.clear();game::peds.clear();
    game::Building thinWall{};
    thinWall.x=5135;thinWall.z=5090;thinWall.w=0.1f;
    thinWall.d=20;thinWall.h=60;
    game::buildings.push_back(thinWall);
    game::Ped sheltered{};sheltered.p={5160,5100};
    sheltered.health=100;sheltered.knockedDown=1;
    game::peds.push_back(sheltered);
    game::Bullet thinWallShot{};
    thinWallShot.p={5110,20,5100};thinWallShot.v={6000,0,0};
    thinWallShot.life=1;thinWallShot.damage=25;thinWallShot.range=600;
    game::bullets.push_back(thinWallShot);
    game::update(1.0f/60.0f);
    assert(game::bullets.empty()&&game::peds[0].health==100);
    assert(!game::hitFlashes.empty()&&
        std::abs(game::hitFlashes.back().p.x-5135.0f)<0.1f);
    game::peds.clear();
    thinWallShot.rocket=true;thinWallShot.explosionRadius=12;
    thinWallShot.explosionDamage=10;
    game::bullets.push_back(thinWallShot);
    game::update(1.0f/60.0f);
    assert(!game::blasts.empty());
    assert(std::abs(game::blasts.back().p.x-5135.0f)<0.1f);
    game::reset();game::buildings.clear();game::player={300,250};
    for(auto& ped:game::peds){ped.alive=false;ped.respawn=999999;}
    int smg=weapons::indexOf("smg");
    game::armedKills[smg]=99;
    game::peds[0].alive=true;game::peds[0].p={350,250};
    game::peds[0].health=40;game::peds[0].armor=0;
    game::peds[0].armed=true;game::peds[0].knockedDown=1;
    game::Bullet qualifying{};qualifying.p={310,20,250};qualifying.v={900,0,0};
    qualifying.life=1;qualifying.damage=100;qualifying.range=600;
    qualifying.silent=true;qualifying.sourceWeapon=smg;
    game::bullets.push_back(qualifying);
    for(int tick=0;tick<8;++tick)game::update(1.0f/60.0f);
    assert(game::armedKills[smg]==100&&game::dualWieldActive(smg));
    game::bullets.clear();game::weapon=smg;game::unlocked[smg]=true;
    game::magazine[smg]=10;game::ammo[smg]=100;game::fireCooldown=0;
    game::shoot();
    assert(game::magazine[smg]==8&&game::bullets.size()==2);
    assert(savegame::save()&&savegame::load());
    assert(game::armedKills[smg]==100&&game::dualWieldActive(smg));
    game::reset();
    assert(content::lastError().empty());
    for(auto& ped:game::peds)ped.alive=false;
    assert(!police::report(police::Crime::Murder,game::player,true));
    for(int tick=0;tick<15;++tick)police::update(0.1f);
    assert(police::wantedLevel()==0);
    game::peds[0].alive=true;game::peds[0].p=game::player+game::Vec2{40,0};
    assert(police::report(police::Crime::Murder,game::player,true));
    for(int tick=0;tick<35;++tick)police::update(0.1f);
    assert(police::wantedLevel()==2);
    int officers=0;
    for(const auto& ped:game::peds)if(ped.police&&ped.alive){
        ++officers;assert(ped.weaponIndex==weapons::indexOf("smg"));
    }
    assert(officers>=1);
    assert(police::report(police::Crime::Murder,game::player,true));
    for(int tick=0;tick<15;++tick)police::update(0.1f);
    assert(police::wantedLevel()==4);
    for(const auto& ped:game::peds)if(ped.police&&ped.alive)
        assert(ped.weaponIndex==weapons::indexOf("sniper"));
    for(auto& ped:game::peds)if(ped.police&&ped.alive){
        ped.p=game::player+game::Vec2{120,0};
        ped.state=game::PedState::Attack;ped.hostile=true;ped.fireCooldown=0;
        break;
    }
    game::buildings.clear();game::bullets.clear();
    ai::update(1.0f/60.0f);
    bool strongShot=false;
    for(const auto& bullet:game::bullets)if(bullet.hostile&&
        bullet.damage==weapons::stats(weapons::indexOf("sniper")).damage)
        strongShot=true;
    assert(strongShot);
    assert(savegame::save());
    assert(savegame::load()&&police::wantedLevel()==4);
    game::reset();
    game::buildings.clear();game::peds[0].p=game::player+game::Vec2{50,0};
    game::cameraYaw=0;
    assert(ai::notifyThreat(game::player,game::cameraYaw)>0);
    assert(game::peds[0].state==game::PedState::Flee);
    game::reset();
    game::buildings.clear();game::props.clear();
    for(auto& ped:game::peds){ped.alive=false;ped.respawn=999999;}
    game::Ped& loneVictim=game::peds[0];
    loneVictim.alive=true;loneVictim.health=20;loneVictim.armor=0;
    loneVictim.p=game::player+game::Vec2{50,0};
    game::Bullet quietShot{};
    quietShot.p={game::player.x+20,18,game::player.z};
    quietShot.v={1200,0,0};quietShot.life=1;quietShot.damage=50;
    quietShot.range=600;quietShot.silent=true;
    game::bullets.push_back(quietShot);
    for(int tick=0;tick<5;++tick)game::update(1.0f/60.0f);
    assert(!game::peds[0].alive);
    for(int tick=0;tick<90;++tick)police::update(1.0f/60.0f);
    assert(police::wantedLevel()==0);
    game::reset();
    assert(game::missions.size()==10);
    assert(game::missions[6].kind==game::MissionKind::Drive);
    assert(game::missions[7].kind==game::MissionKind::Targets);
    assert(game::missions[8].kind==game::MissionKind::Collect);
    assert(game::missions[9].kind==game::MissionKind::Drive);
    for(int index=6;index<10;++index){
        assert(regions::roadAt(game::missions[index].start));
        for(const auto& goal:game::missions[index].goals)
            assert(regions::roadAt(goal));
    }
    game::player=game::missions[8].start;
    game::startMission();
    assert(game::activeMission==8);
    game::reset();
    assert(game::trees.size()>6042);
    assert(std::count_if(game::trees.begin(),game::trees.end(),
        [](const game::Tree& tree){return tree.id.rfind("marina-tree-",0)==0;})>=20);
    assert(fire::ignite(game::trees.front().p,fire::Material::Wood));
    for(int tick=0;tick<230;++tick)fire::update(0.05f);
    assert(game::trees.front().destroyed&&game::trees.front().health==0);
    auto& regionalTree=game::trees[42];
    assert(fire::surfaceAt({regionalTree.p.x,20,regionalTree.p.z})==
        fire::Material::Wood);
    assert(fire::ignite(regionalTree.p,fire::Material::Wood));
    for(int tick=0;tick<20;++tick)fire::update(0.05f);
    assert(regionalTree.burning);
    fire::extinguish(regionalTree.p,30,30);
    assert(!regionalTree.burning);
    assert(savegame::save());
    assert(savegame::load()&&game::trees.front().destroyed);
    game::reset();
    for(const auto& ped:game::peds)
        assert(ped.armed?ped.cash>=45:ped.cash>=5&&ped.cash<=35);
    std::ifstream originalWorld(data_file::resourcePath("world.ini"));
    assert(originalWorld);
    std::string world((std::istreambuf_iterator<char>(originalWorld)),std::istreambuf_iterator<char>());
    auto vehicleX=world.find("X=345");
    assert(vehicleX!=std::string::npos);
    world.replace(vehicleX,5,"X=355");
    std::string worldConfig=std::string(currentDirectory)+"\\world-test.ini";
    {std::ofstream data(worldConfig);data<<world;}
    assert(content::populate(worldConfig.c_str()));
    assert(game::vehicles[game::vehicles.size()-6].id=="starter-car");
    assert(game::vehicles[game::vehicles.size()-6].p.x==355);
    auto duplicateVehicle=world.find("Id=sport-car");
    assert(duplicateVehicle!=std::string::npos);
    world.replace(duplicateVehicle,12,"Id=starter-car");
    {std::ofstream data(worldConfig,std::ios::trunc);data<<world;}
    assert(!content::populate(worldConfig.c_str()));
    assert(content::lastError().find("Duplicate")!=std::string::npos);
    std::remove(worldConfig.c_str());
    std::srand(1);
    game::reset();
    game::player=game::missions[1].start;
    game::startMission();
    assert(game::activeMission==-1&&game::nextMission()==0);
    game::reset();
    assert(game::magazine[0]==weapons::stats(0).magazine);
    assert(physics::tuning(game::Kind::SportCar).maxSpeed>physics::tuning(game::Kind::Boat).maxSpeed);
    game::Vehicle driftCar{};driftCar.kind=game::Kind::Car;
    for(int tick=0;tick<60;++tick){
        driftCar.angle+=0.008f;
        game::Vec2 forward=game::forward(driftCar.angle);
        game::Vec2 side{-forward.z,forward.x};
        driftCar.velocity=forward*150+side*50;
        assert(physics::updateDrift(driftCar,1,true,1.0f/60.0f)==0);
    }
    driftCar.velocity=game::forward(driftCar.angle)*150;
    assert(physics::updateDrift(driftCar,0,false,1.0f/60.0f)==45);
    assert(physics::updateDrift(driftCar,0,false,1.0f/60.0f)==0);
    game::Vehicle spinCar{};spinCar.kind=game::Kind::Car;
    for(int tick=0;tick<120;++tick){
        spinCar.angle+=0.02f;
        spinCar.velocity=game::forward(spinCar.angle)*8+
            game::Vec2{-std::sin(spinCar.angle),std::cos(spinCar.angle)}*8;
        assert(physics::updateDrift(spinCar,1,true,1.0f/60.0f)==0);
    }
    assert(physics::updateDrift(spinCar,0,false,1.0f/60.0f)==0);
    assert(!game::props.empty());

    game::player=game::missions[0].start;
    game::startMission();
    assert(game::activeMission==0&&game::missionStep==0);
    int car=int(game::vehicles.size())-6;
    game::occupied=car;
    for(size_t step=0;step<game::missions[0].goals.size();++step){
        game::vehicles[car].p=game::missions[0].goals[step];
        game::player=game::vehicles[car].p;
        game::update(1.0f/60.0f);
        if(step+1<game::missions[0].goals.size())assert(game::missionStep==int(step+1));
    }
    assert(game::activeMission==-1&&game::missionDone[0]);
    assert(game::money==game::missions[0].reward);
    game::occupied=-1;
    game::player=game::missions[1].start;
    game::startMission();
    assert(game::activeMission==1&&game::nextMission()==1);
    game::reset();
    game::missionDone.fill(true);game::missionDone[5]=false;
    game::player=game::missions[5].start;game::startMission();
    assert(game::activeMission==5&&game::missionStep==0);
    car=int(game::vehicles.size())-6;game::occupied=car;
    game::vehicles[car].p=game::missions[5].goals[0];game::player=game::vehicles[car].p;
    game::update(1.0f/60.0f);
    assert(game::missionStep==1);
    game::occupied=-1;
    auto beachTarget=game::missions[5].goals[1];
    game::bullets.push_back({{beachTarget.x,20,beachTarget.z-20},{0,0,1200},1,12,0});
    game::update(1.0f/60.0f);
    assert(game::missionStep==2);
    int boat=int(game::vehicles.size())-1;game::occupied=boat;
    game::vehicles[boat].p=game::missions[5].goals[2];game::player=game::vehicles[boat].p;
    game::update(1.0f/60.0f);
    assert(game::activeMission==-1&&game::missionDone[5]);
    game::occupied=-1;game::reset();
    for(int mission=0;mission<int(game::missions.size());++mission){
        game::occupied=-1;
        game::player=game::missions[mission].start;
        game::startMission();
        assert(game::activeMission==mission);
        const auto definition=game::missions[mission];
        for(size_t goal=0;goal<definition.goals.size();++goal){
            bool target=definition.kind==game::MissionKind::Targets||
                (definition.kind==game::MissionKind::Finale&&goal==1);
            if(target){
                game::occupied=-1;
                const auto point=definition.goals[goal];
                game::bullets.push_back({{point.x,20,point.z-20},{0,0,1200},1,12,0});
            }else{
                int vehicle=definition.kind==game::MissionKind::Collect?-1:
                    definition.kind==game::MissionKind::Boat||
                    (definition.kind==game::MissionKind::Finale&&goal==2)?
                    int(game::vehicles.size())-1:
                    definition.kind==game::MissionKind::Bike?
                    int(game::vehicles.size())-4:int(game::vehicles.size())-6;
                game::occupied=vehicle;
                if(vehicle>=0)game::vehicles[vehicle].p=definition.goals[goal];
                game::player=definition.goals[goal];
            }
            game::update(1.0f/60.0f);
            if(goal+1<definition.goals.size())assert(game::missionStep==int(goal+1));
        }
        assert(game::activeMission==-1&&game::missionDone[mission]);
    }
    game::occupied=-1;game::reset();

    game::buildings.clear();game::peds.clear();game::props.clear();game::vehicles.clear();
    game::cameraYaw=0;game::cameraPitch=0;
    auto pose=camera::compute(game::player,0,true,-1);
    game::Vec3 ray=game::norm(pose.target-pose.eye);
    game::Vec3 pedPoint=pose.eye+ray*200;
    game::Ped target{};target.p={pedPoint.x,pedPoint.z};target.alive=true;
    game::peds.push_back(target);
    game::Vec3 hit=camera::traceReticle(pose,650);
    assert(game::len(hit-pose.eye)<200&&hit.y>2&&hit.y<37);
    game::Vec3 wallPoint=pose.eye+ray*100;
    game::buildings.push_back({wallPoint.x-5,wallPoint.z-5,10,10,80,{1,1,1}});
    hit=camera::traceReticle(pose,650);
    assert(game::len(hit-pose.eye)<150);
    game::buildings={{-5,-5,10,10,80,{1,1,1}}};game::peds.clear();
    hit=camera::traceReticle({{0,10,0},{100,10,0}},100);
    assert(hit.x>4.9f&&hit.x<5.1f);
    game::buildings.clear();
    game::Vec3 highMuzzle=camera::weaponMuzzle({0,0},0,0,{250,120,0});
    game::Vec3 lowMuzzle=camera::weaponMuzzle({0,0},0,0,{250,-80,0});
    assert(highMuzzle.y>20&&lowMuzzle.y<20);
    assert(game::len(highMuzzle-lowMuzzle)>7);
    game::cameraMode=game::CameraMode::ThirdNear;
    game::rightMouse=false;
    auto freeView=camera::compute(game::player,0,false,-1);
    float freeYaw=game::cameraYaw,freePitch=game::cameraPitch;
    input::applyMouseDelta(40,-20);
    assert(game::cameraYaw!=freeYaw&&game::cameraPitch!=freePitch);
    assert(!game::rightMouse);
    auto nearView=camera::compute(game::player,0,false,-1);
    assert(game::len(nearView.target-freeView.target)>0.1f);
    game::cameraMode=game::CameraMode::ThirdFar;
    auto farView=camera::compute(game::player,0,false,-1);
    assert(game::len(farView.eye-game::Vec3{game::player.x,0,game::player.z})>
           game::len(nearView.eye-game::Vec3{game::player.x,0,game::player.z}));
    game::cameraMode=game::CameraMode::FirstClose;
    auto firstView=camera::compute(game::player,0,false,-1);
    assert(firstView.eye.x==game::player.x&&firstView.eye.y==31);
    game::weapon=4;game::rightMouse=true;game::scopeBlend=1;game::scopeLevel=2;
    assert(camera::isScoped()&&camera::scopeMagnification()==8);
    assert(camera::fieldOfView()<10.0f);
    game::rightMouse=false;game::scopeBlend=0;game::weapon=0;
    game::telescopeActive=true;game::scopeLevel=1;
    for(int tick=0;tick<4;++tick)game::update(0.05f);
    assert(camera::zoomActive()&&camera::scopeMagnification()==4);
    assert(camera::firstPersonActive()&&camera::fieldOfView()<20.0f);
    auto telescopeView=camera::compute(game::player,game::playerY,false,-1);
    assert(telescopeView.eye.y>=30);
    int telescopeAmmo=game::magazine[0];
    game::leftMouse=true;game::rightMouse=true;game::update(0.05f);
    assert(game::magazine[0]==telescopeAmmo);
    game::leftMouse=false;game::rightMouse=false;game::telescopeActive=false;
    game::scopeBlend=0;
    game::reset();game::buildings.clear();game::peds.clear();game::props.clear();
    int damagedCar=int(game::vehicles.size())-6;
    const auto carStart=game::vehicles[damagedCar].p;
    game::bullets.push_back({{carStart.x-35,20,carStart.z},{1200,0,0},1,52,0});
    game::update(1.0f/60.0f);
    assert(game::vehicleHealth(damagedCar)<physics::tuning(game::Kind::Car).maxHealth);
    game::damageVehicle(damagedCar,130);
    assert(game::vehicles[damagedCar].damage>=physics::tuning(game::Kind::Car).smokeThreshold);
    game::player=game::vehicles[damagedCar].p+game::Vec2{40,0};
    assert(game::interactionPrompt().find("REPAIR")!=std::string::npos);
    game::interact();
    assert(game::repairKits==0&&game::vehicles[damagedCar].damage==0);
    game::damageVehicle(damagedCar,300);
    assert(game::vehicles[damagedCar].exploded&&game::vehicleHealth(damagedCar)==0);
    assert(!game::blasts.empty()&&game::blasts.back().radius==95.0f);
    assert(!game::repairVehicle(damagedCar));
    assert(savegame::save());
    assert(savegame::load());
    int savedCar=int(game::vehicles.size())-6;
    assert(game::vehicles[savedCar].exploded&&game::repairKits==0);
    game::reset();
    game::buildings.clear();game::peds.clear();game::props.clear();
    int rocketCar=int(game::vehicles.size())-6;
    game::weapon=weapons::indexOf("rpg");game::unlocked[game::weapon]=true;
    game::magazine[game::weapon]=1;
    game::cameraYaw=0;game::cameraPitch=0;
    game::rightMouse=true;game::leftMouse=true;
    for(int tick=0;tick<12&&!game::vehicles[rocketCar].exploded;++tick)
        game::update(1.0f/60.0f);
    game::leftMouse=false;game::rightMouse=false;
    assert(game::magazine[weapons::indexOf("rpg")]==0);
    assert(game::vehicles[rocketCar].exploded);
    assert(!game::blasts.empty());
    game::reset();
    game::weapon=weapons::indexOf("flamethrower");
    game::unlocked[game::weapon]=true;game::magazine[game::weapon]=10;
    game::rightMouse=true;game::leftMouse=true;
    game::update(1.0f/60.0f);
    game::rightMouse=false;game::leftMouse=false;
    assert(game::magazine[weapons::indexOf("flamethrower")]<10);
    assert(!fire::active().empty());
    game::reset();game::peds.clear();game::buildings.clear();game::props.clear();
    game::Ped flameTarget{};flameTarget.p={605,500};
    game::peds.push_back(flameTarget);
    game::Bullet flameShot{};flameShot.p={580,18,500};flameShot.v={1200,0,0};
    flameShot.life=1;flameShot.damage=3;flameShot.range=300;flameShot.streamType=1;
    game::bullets.push_back(flameShot);
    game::update(1.0f/60.0f);
    assert(game::peds[0].burnTime>0);
    game::Bullet foamShot=flameShot;foamShot.streamType=2;
    game::bullets.push_back(foamShot);
    game::update(1.0f/60.0f);
    assert(game::peds[0].burnTime==0);
    game::reset();game::peds.clear();game::buildings.clear();game::props.clear();
    game::Ped wetTarget{};wetTarget.p={605,500};
    game::peds.push_back(wetTarget);
    game::Bullet water{};water.p={580,18,500};water.v={1200,0,0};
    water.life=1;water.damage=3;water.range=300;water.streamType=3;
    game::bullets.push_back(water);
    game::update(1.0f/60.0f);
    assert(game::peds[0].knockedDown>2.0f);
    game::reset();
    game::peds.clear();game::buildings.clear();game::props.clear();
    game::Vec2 firePoint{550,900};
    assert(fire::ignite(firePoint,fire::Material::Grass));
    for(int pulse=0;pulse<3;++pulse){
        game::Bullet spray{};spray.p={520,18,900};spray.v={1200,0,0};
        spray.life=1;spray.damage=1;spray.range=200;spray.streamType=3;
        game::bullets.push_back(spray);
        game::update(1.0f/60.0f);
    }
    assert(fire::active().empty());
    assert(!fire::ignite(firePoint));
    game::reset();
    game::cameraMode=game::CameraMode::ThirdNear;
    game::peds.clear();game::buildings.clear();
    game::Ped armed{};armed.p={game::player.x+50,game::player.z};armed.armed=true;
    game::Ped fleeing{};fleeing.p={game::player.x+60,game::player.z};fleeing.style=1;
    game::Ped investigating{};investigating.p={game::player.x+70,game::player.z};
    game::peds={armed,fleeing,investigating};
    ai::notifyGunshot(game::player);
    assert(game::peds[0].state==game::PedState::Attack);
    assert(game::peds[1].state==game::PedState::Flee);
    assert(game::peds[2].state==game::PedState::Investigate);
    ai::reactToHit(game::peds[2],game::player);
    assert(game::peds[2].state==game::PedState::Flee);
    game::reset();
    game::buildings.clear();game::peds.clear();game::props.clear();
    game::bullets.push_back({{game::player.x-20,18,game::player.z},{1200,0,0},1,12,0,
        0,650,0,true});
    float healthBefore=game::health;
    game::update(1.0f/60.0f);
    assert(game::health<healthBefore);
    game::reset();

    game::weapon=1;game::unlocked[1]=true;game::ammo[1]=50;game::magazine[1]=0;
    game::startReload();
    assert(game::reloadRemaining>0);
    for(int i=0;i<120;++i)game::update(1.0f/60.0f);
    assert(game::magazine[1]==weapons::stats(1).magazine);
    assert(game::ammo[1]==20);

    auto& crate=game::props.front();
    int oldHealth=crate.health;
    assert(props::hit({crate.p.x,10,crate.p.z},{200,0,0},25));
    assert(crate.health==oldHealth-25);
#ifdef MINI_CITY_JOLT
    game::reset();
    int driveByCar=int(game::vehicles.size())-6;
    game::occupied=driveByCar;game::player=game::vehicles[driveByCar].p;
    game::leftMouse=true;
    game::update(1.0f/60.0f);
    game::leftMouse=false;
    assert(game::magazine[0]==weapons::stats(0).magazine-1);
    assert(game::lastMuzzle.y>15);
    game::reset();
    driveByCar=int(game::vehicles.size())-6;
    game::occupied=driveByCar;game::player=game::vehicles[driveByCar].p;
    game::weapon=4;game::unlocked[4]=true;
    game::magazine[4]=weapons::stats(4).magazine;
    game::leftMouse=true;
    game::update(1.0f/60.0f);
    game::leftMouse=false;
    assert(game::magazine[4]==weapons::stats(4).magazine);
    game::reset();
    int drivenCar=int(game::vehicles.size())-6;
    game::occupied=drivenCar;
    game::Vec2 parkedStart=game::vehicles[drivenCar].p;
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    for(int tick=0;tick<90;++tick)game::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    assert(game::len(game::vehicles[drivenCar].p-parkedStart)>80);
    assert(jolt_world::wheelContactCount(drivenCar)>=2);
    game::reset();
    drivenCar=int(game::vehicles.size())-6;
    game::vehicles[drivenCar].p={300,400};
    game::vehicles[drivenCar].angle=game::PI/2;
    game::occupied=drivenCar;
    game::Vec2 driveStart=game::vehicles[drivenCar].p;
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    for(int tick=0;tick<120;++tick)game::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    assert(game::len(game::vehicles[drivenCar].p-driveStart)>80);
    assert(jolt_world::wheelContactCount(drivenCar)>=2);
    float straightAngle=game::vehicles[drivenCar].angle;
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    game::keys[ui::bindings[int(ui::Action::Left)]]=true;
    for(int tick=0;tick<45;++tick)game::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    game::keys[ui::bindings[int(ui::Action::Left)]]=false;
    assert(std::abs(game::vehicles[drivenCar].angle-straightAngle)>0.10f);
    float speedBeforeBrake=std::abs(game::vehicles[drivenCar].speed);
    game::keys[ui::bindings[int(ui::Action::Backward)]]=true;
    for(int tick=0;tick<35;++tick)game::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Backward)]]=false;
    assert(std::abs(game::vehicles[drivenCar].speed)<speedBeforeBrake*0.8f);
    game::reset();
    int drivenBike=int(game::vehicles.size())-4;
    game::occupied=drivenBike;
    game::Vec2 bikeStart=game::vehicles[drivenBike].p;
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    for(int tick=0;tick<90;++tick)game::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    assert(game::len(game::vehicles[drivenBike].p-bikeStart)>80);
    assert(jolt_world::wheelContactCount(drivenBike)>=2);
    game::reset();
    for(int tick=0;tick<5;++tick)game::update(1.0f/60.0f);
    game::keys[VK_SPACE]=true;
    game::update(1.0f/60.0f);
    game::keys[VK_SPACE]=false;
    float jumpPeak=game::playerY;
    for(int tick=0;tick<120;++tick){
        game::update(1.0f/60.0f);
        jumpPeak=std::max(jumpPeak,game::playerY);
    }
    assert(jumpPeak>10.0f);
    assert(game::playerY<1.0f&&game::grounded);
    game::reset();game::player={850,game::SHORE+25};
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    game::cameraYaw=game::PI/2;
    float swimStart=game::player.z;
    for(int tick=0;tick<90;++tick)game::update(1.0f/60.0f);
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    assert(game::swimming&&!game::grounded);
    assert(game::player.z>swimStart+60&&game::playerY>-5);
    game::reset();
    game::keys[VK_SPACE]=true;
    float heldPeak=0;
    for(int tick=0;tick<180;++tick){
        game::update(1.0f/60.0f);
        heldPeak=std::max(heldPeak,game::playerY);
    }
    game::keys[VK_SPACE]=false;
    assert(heldPeak<45.0f);
    game::reset();
    game::armor=25;game::playerY=600;game::grounded=false;
    for(int tick=0;tick<180;++tick){
        jolt_world::moveCharacter({0,0},false,1.0f/60.0f);
        props::update(1.0f/60.0f);
    }
    assert(game::grounded&&game::playerY<1.0f);
    assert(game::armor<25);
    game::reset();
    game::player={14,80};
    jolt_world::teleportCharacter(game::player,0);
    for(int tick=0;tick<60;++tick)
        jolt_world::moveCharacter({-160,0},false,1.0f/60.0f);
    assert(game::player.x>=12.0f&&game::playerY<1.0f&&game::grounded);
    float edgePosition=game::player.x;
    for(int tick=0;tick<60;++tick)
        jolt_world::moveCharacter({160,0},false,1.0f/60.0f);
    assert(game::player.x>edgePosition+100.0f&&game::grounded);
    game::player={-35,80};
    jolt_world::teleportCharacter(game::player,-20);
    for(int tick=0;tick<20;++tick)
        jolt_world::moveCharacter({0,0},false,1.0f/60.0f);
    assert(game::player.x>=12.0f&&game::playerY<1.0f&&game::grounded);
    game::reset();
    game::keys[ui::bindings[int(ui::Action::Forward)]]=true;
    for(int tick=0;tick<60;++tick)game::update(1.0f/60.0f);
    float normalWalk=game::player.x-300.0f;
    game::keys[ui::bindings[int(ui::Action::Forward)]]=false;
    game::reset();
    input::windowProc(nullptr,WM_SYSKEYDOWN,VK_MENU,0);
    input::windowProc(nullptr,WM_SYSKEYDOWN,'W',0);
    for(int tick=0;tick<60;++tick)game::update(1.0f/60.0f);
    float slowWalk=game::player.x-300.0f;
    input::windowProc(nullptr,WM_SYSKEYUP,'W',0);
    input::windowProc(nullptr,WM_SYSKEYUP,VK_MENU,0);
    assert(slowWalk>25.0f&&slowWalk<normalWalk*0.7f);
    game::reset();
    float standingEye=camera::compute(game::player,0,false,-1).eye.y;
    input::windowProc(nullptr,WM_KEYDOWN,VK_CONTROL,0);
    assert(game::crouched);
    input::windowProc(nullptr,WM_KEYUP,VK_CONTROL,0);
    for(int tick=0;tick<10;++tick)game::update(1.0f/60.0f);
    assert(game::crouched&&game::grounded&&
        camera::compute(game::player,0,false,-1).eye.y<standingEye-8.0f);
    input::windowProc(nullptr,WM_KEYDOWN,VK_CONTROL,1LL<<30);
    assert(game::crouched);
    input::windowProc(nullptr,WM_KEYUP,VK_CONTROL,0);
    input::windowProc(nullptr,WM_KEYDOWN,VK_CONTROL,0);
    assert(!game::crouched);
    input::windowProc(nullptr,WM_KEYUP,VK_CONTROL,0);
    game::reset();
    int entryCar=int(game::vehicles.size())-6;
    game::player=game::vehicles[entryCar].p+game::Vec2{20,0};
    game::enterExit();
    assert(game::enteringVehicle==entryCar&&game::occupied==-1);
    for(int tick=0;tick<20;++tick)game::update(1.0f/60.0f);
    assert(game::enteringVehicle==entryCar&&game::occupied==-1);
    for(int tick=0;tick<30;++tick)game::update(1.0f/60.0f);
    assert(game::enteringVehicle==-1&&game::occupied==entryCar);
    assert(game::vehicleLightsOn(game::vehicles[entryCar]));
    input::windowProc(nullptr,WM_KEYDOWN,'H',0);
    assert(!game::vehicleLightsOn(game::vehicles[entryCar]));
    game::gameHour=22;
    assert(!game::vehicleLightsOn(game::vehicles[entryCar]));
    input::windowProc(nullptr,WM_KEYDOWN,'H',1LL<<30);
    assert(!game::vehicleLightsOn(game::vehicles[entryCar]));
    input::windowProc(nullptr,WM_KEYUP,'H',0);
    input::windowProc(nullptr,WM_KEYDOWN,'H',0);
    assert(game::vehicleLightsOn(game::vehicles[entryCar]));
    input::windowProc(nullptr,WM_KEYUP,'H',0);
    game::reset();
    game::peds[0].p={300,450};
    game::Vec2 pedStart=game::peds[0].p;
    for(int tick=0;tick<60;++tick)
        jolt_world::movePed(0,{80,0},1.0f/60.0f);
    assert(game::len(game::peds[0].p-pedStart)>30);
    game::reset();
    game::Vec2 start=game::player;
    for(int tick=0;tick<12;++tick){
        jolt_world::moveCharacter({100,0},false,1.0f/60.0f);
        props::update(1.0f/60.0f);
    }
    assert(game::player.x>start.x+4.0f);
    int boatIndex=int(game::vehicles.size())-1;
    game::Vec2 boatStart=game::vehicles[boatIndex].p;
    game::vehicles[boatIndex].velocity={90,0};
    jolt_world::driveVehicle(boatIndex,0,0,1.0f/60.0f);
    props::update(1.0f/60.0f);
    assert(game::vehicles[boatIndex].p.x>boatStart.x+0.1f);
    game::reset();
    game::Vec2 crateStart=game::props.front().p;
    jolt_world::impulse(0,{120,0,0});
    for(int tick=0;tick<30;++tick)props::update(1.0f/60.0f);
    assert(game::len(game::props.front().p-crateStart)>1.0f);
    game::Ped fallen{};fallen.p={240,game::BEACH_START+120};fallen.style=2;
    jolt_world::spawnRagdoll(fallen,{400,0,100});
    props::update(1.0f/60.0f);
    assert(game::ragdollParts.size()==6);
    for(int tick=0;tick<380;++tick)props::update(1.0f/60.0f);
    assert(game::ragdollParts.empty());
    game::reset();
    auto& pinnedPed=game::peds[0];
    pinnedPed.p=game::player+game::Vec2{45,0};
    pinnedPed.alive=false;pinnedPed.health=0;pinnedPed.respawn=40;
    pinnedPed.pinned=true;pinnedPed.pinAnchor=pinnedPed.p+game::Vec2{4,0};
    const std::string pinnedId=pinnedPed.id;
    jolt_world::spawnRagdoll(pinnedPed,{},&pinnedPed.pinAnchor);
    for(int tick=0;tick<910;++tick)props::update(1.0f/60.0f);
    assert(game::corpseSnapshots.size()==1&&game::ragdollParts.size()==6);
    assert(!game::peds[0].pinned&&game::peds[0].corpseVisualDelay>0);
    const auto frozen=game::corpseSnapshots[0].parts[0].p;
    assert(savegame::save());
    assert(savegame::load());
    auto restored=std::find_if(game::corpseSnapshots.begin(),
        game::corpseSnapshots.end(),[&](const game::CorpseSnapshot& pose){
            return pose.pedId==pinnedId;
        });
    assert(restored!=game::corpseSnapshots.end());
    assert(std::abs(restored->parts[0].p.x-frozen.x)<0.01f);
    assert(std::abs(restored->parts[0].p.y-frozen.y)<0.01f);
    props::update(1.0f/60.0f);
    assert(game::ragdollParts.size()==6);
#endif

    game::money=1234;game::health=68;game::weapon=1;game::unlocked[1]=true;
    game::missionDone[5]=true;game::missionDone[8]=true;
    assert(savegame::save());
    game::money=0;game::health=100;game::weapon=0;
    game::missionDone[5]=false;game::missionDone[8]=false;
    assert(savegame::load());
    assert(game::money==1234&&game::health==68&&game::weapon==1);
    assert(game::missionDone[5]&&game::missionDone[8]);
    char executable[MAX_PATH]{};GetModuleFileNameA(nullptr,executable,MAX_PATH);
    std::string savePath(executable);
    savePath=savePath.substr(0,savePath.find_last_of("\\/")+1)+"savegame.ini";
    {std::ofstream legacy(savePath,std::ios::trunc);
        legacy<<"[Save]\nVersion=1\n[Player]\nX=300\nZ=250\nHealth=64\nMoney=712\n"
              "Hour=1650\nWeapon=1\n[Weapons]\nUnlocked1=1\nReserve1=37\nMagazine1=8\n"
              "[Missions]\nComplete0=1\n";}
    assert(savegame::load());
    assert(game::money==712&&game::health==256&&game::weapon==1);
    assert(game::ammo[1]==37&&game::magazine[1]==8&&game::missionDone[0]);
    assert(!game::missionDone[8]);
    assert(savegame::save());
    assert(GetPrivateProfileIntA("Save","Version",0,savePath.c_str())==3);
    game::money=0;game::missionDone[0]=false;
    assert(savegame::load()&&game::money==712&&game::missionDone[0]);
    game::reset();game::buildings.clear();game::peds.clear();
    game::player={300,250};game::money=0;game::health=100;game::armor=50;
    game::creditMoney(40);
    assert(game::money==40&&!game::spendMoney(41));
    assert(game::spendMoney(40)&&game::money==0);
    game::applyDamage(30);
    assert(game::armor==20&&game::health==100);
    game::applyDamage(50);
    assert(game::armor==0&&game::health==70);
    game::armor=12;
    game::Ped corpse{};
    corpse.id="ped-0";corpse.p={310,250};corpse.alive=false;
    corpse.cash=70;corpse.respawn=45;corpse.corpseVisualDelay=0;
    game::peds.push_back(corpse);
    assert(game::interactionPrompt().find("LOOT")!=std::string::npos);
    game::cycleInteraction();
    assert(game::interactionPrompt().find("START")!=std::string::npos);
    game::cycleInteraction();
    assert(game::interactionPrompt().find("LOOT")!=std::string::npos);
    game::interact();
    assert(game::money==70&&game::peds[0].looted&&game::peds[0].cash==0);
    game::interact();
    assert(game::money==70);
    assert(game::carryPrompt().find("CARRY")!=std::string::npos);
    game::carryDrop();
    assert(game::peds[0].carried);
    game::update(1.0f/60.0f);
    game::carryDrop();
    assert(!game::peds[0].carried);
    assert(savegame::save());
    game::money=0;game::armor=0;game::peds[0].looted=false;game::peds[0].cash=70;
    assert(savegame::load());
    assert(game::money==70&&game::armor==12);
    assert(game::peds[0].looted&&game::peds[0].cash==0&&!game::peds[0].alive);
    game::peds.clear();game::buildings.clear();
    game::Ped victim{};
    victim.id="test-victim";victim.p={310,250};victim.angle=0;victim.cash=90;
    game::peds.push_back(victim);
    game::player={300,250};game::money=0;
    assert(game::interactionPrompt().find("PICKPOCKET")!=std::string::npos);
    game::interact();
    assert((game::money==90&&game::peds[0].cash==0)||
           (game::money==0&&game::peds[0].hostile));
    ui::shadowQuality=2;ui::reflectionQuality=2;
    ui::antiAliasingQuality=2;ui::aoQuality=2;
    ui::drawDistance=100;ui::lodDistance=75;
    ui::grassDistance=83;ui::save();
    ui::shadowQuality=0;ui::reflectionQuality=0;
    ui::antiAliasingQuality=0;ui::aoQuality=0;
    ui::drawDistance=0;ui::lodDistance=0;
    ui::grassDistance=0;ui::load();
    assert(ui::shadowQuality==2&&ui::reflectionQuality==2&&
        ui::antiAliasingQuality==2&&ui::aoQuality==2&&
        ui::drawDistance==100&&ui::lodDistance==75&&
        ui::grassDistance==83);
    assert(std::abs(ui::drawDistanceScale()-7.5f)<0.001f);
    std::puts("simulation smoke passed");
}
