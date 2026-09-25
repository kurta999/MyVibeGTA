#include "commerce.h"
#include "data_file.h"
#include "game_internal.h"
#include "police.h"
#include "savegame.h"
#include "weapons.h"
#include "regions.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif
#include <algorithm>
#include <cmath>
#include <set>

namespace commerce {
std::vector<Shop> shops;
std::vector<House> houses;
std::vector<Item> items;
namespace {
std::string error;
Menu currentMenu=Menu::None;
int currentIndex=-1,currentSelection=0;
bool validId(data_file::Ini& file,const std::string& section,
             std::string& id,std::set<std::string>& ids){
    if(!file.string(section,"Id",id)){error=file.lastError();return false;}
    if(!data_file::validId(id)||!ids.insert(id).second){
        error="Invalid or duplicate ["+section+"] Id";return false;
    }
    return true;
}
bool kind(const std::string& name,ItemKind& result){
    if(name=="weapon")result=ItemKind::Weapon;
    else if(name=="ammo")result=ItemKind::Ammo;
    else if(name=="health")result=ItemKind::Health;
    else if(name=="armor")result=ItemKind::Armor;
    else if(name=="wanted")result=ItemKind::Wanted;
    else if(name=="repair-kit")result=ItemKind::RepairKit;
    else if(name=="vehicle")result=ItemKind::Vehicle;
    else return false;
    return true;
}
bool vehicleKind(const std::string& name,game::Kind& result){
    if(name=="car")result=game::Kind::Car;
    else if(name=="sport-car")result=game::Kind::SportCar;
    else if(name=="bike")result=game::Kind::Bike;
    else if(name=="boat")result=game::Kind::Boat;
    else return false;
    return true;
}
int availableVehicle(game::Kind kind,game::Vec2 shopPosition){
    int best=-1;float distance=100000;
    for(int index=0;index<int(game::vehicles.size());++index){
        const auto& vehicle=game::vehicles[index];
        float candidate=game::len(vehicle.p-shopPosition);
        if(vehicle.kind==kind&&!vehicle.owned&&!vehicle.exploded&&index!=game::occupied&&
           candidate<distance){best=index;distance=candidate;}
    }
    return best;
}
int nearestOwnedVehicle(int houseIndex){
    if(houseIndex<0||houseIndex>=int(houses.size()))return -1;
    int best=-1;float distance=95;
    for(int index=0;index<int(game::vehicles.size());++index){
        const auto& vehicle=game::vehicles[index];
        float candidate=game::len(vehicle.p-houses[houseIndex].p);
        if(vehicle.owned&&!vehicle.exploded&&vehicle.garageHouseId!=houses[houseIndex].id&&
           candidate<distance){best=index;distance=candidate;}
    }
    return best;
}
bool canTravel(){
    if(game::health<=0||game::occupied>=0||game::activeMission>=0||
       game::carryingBody()||police::wantedLevel()>0)return false;
    for(const auto& ped:game::peds)if(ped.alive&&ped.hostile&&
        game::len(ped.p-game::player)<180)return false;
    return true;
}
bool available(const Item& item){
    switch(item.kind){
    case ItemKind::Weapon:return item.weapon>=0&&
        !(game::unlocked[item.weapon]&&game::ammo[item.weapon]<0);
    case ItemKind::Ammo:return game::ammo[game::weapon]>=0&&game::ammo[game::weapon]<9950;
    case ItemKind::Health:return game::health<game::PLAYER_MAX_HEALTH;
    case ItemKind::Armor:return game::armor<100;
    case ItemKind::Wanted:return police::wantedLevel()>0;
    case ItemKind::RepairKit:return game::repairKits<10;
    case ItemKind::Vehicle:return currentIndex>=0&&currentIndex<int(shops.size())&&
        (item.vehicleKind!=game::Kind::Boat||
            (shops[currentIndex].p.x<game::WORLD_W&&
             shops[currentIndex].p.z>game::BEACH_START))&&
        availableVehicle(item.vehicleKind,shops[currentIndex].p)>=0;
    }
    return false;
}
void changed(const std::string& description){
    game::announce(description,3);
    savegame::save();
}
}
bool load(const char* path){
    error.clear();
    data_file::Ini file;
    if(!file.load(path?path:data_file::resourcePath("commerce.ini"))||!file.version(1)){
        error=file.lastError();return false;
    }
    int shopCount=0,itemCount=0,houseCount=0;
    if(!file.integer("Shops","Count",shopCount,1,20)||
       !file.integer("Items","Count",itemCount,1,64)||
       !file.integer("Houses","Count",houseCount,1,30)){
        error=file.lastError();return false;
    }
    std::vector<Shop> parsedShops;std::vector<Item> parsedItems;
    std::vector<House> parsedHouses;
    std::set<std::string> shopIds,itemIds,houseIds;
    for(int index=0;index<shopCount;++index){
        std::string section="Shop"+std::to_string(index);Shop shop{};
        if(!validId(file,section,shop.id,shopIds)||
           !file.string(section,"Name",shop.name)||
           !file.real(section,"X",shop.p.x,15,regions::WIDTH-15)||
           !file.real(section,"Z",shop.p.z,15,regions::DEPTH-15)){
            if(error.empty())error=file.lastError();return false;
        }
        if(regions::waterAt(shop.p)){error="Shop is in water: "+shop.id;return false;}
        parsedShops.push_back(shop);
    }
    for(int index=0;index<itemCount;++index){
        std::string section="Item"+std::to_string(index),type;
        Item item{};
        if(!validId(file,section,item.id,itemIds)||
           !file.string(section,"Name",item.name)||
           !file.string(section,"Kind",type)||
           !file.string(section,"Reference",item.reference)||
           !file.integer(section,"Amount",item.amount,1,10000)||
           !file.integer(section,"Price",item.price,1,100000)){
            if(error.empty())error=file.lastError();return false;
        }
        if(!kind(type,item.kind)){error="Invalid ["+section+"] Kind";return false;}
        if(item.kind==ItemKind::Weapon){
            item.weapon=weapons::indexOf(item.reference);
            if(item.weapon<0){error="Unknown ["+section+"] Reference";return false;}
        }else if(item.kind==ItemKind::Vehicle){
            if(!vehicleKind(item.reference,item.vehicleKind)){
                error="Invalid ["+section+"] Reference";return false;
            }
        }else if(item.reference!=(item.kind==ItemKind::Ammo?"current":"none")){
            error="Invalid ["+section+"] Reference";return false;
        }
        parsedItems.push_back(item);
    }
    for(int index=0;index<houseCount;++index){
        std::string section="House"+std::to_string(index);House house{};
        if(!validId(file,section,house.id,houseIds)||
           !file.string(section,"Name",house.name)||
           !file.real(section,"X",house.p.x,15,regions::WIDTH-15)||
           !file.real(section,"Z",house.p.z,15,regions::DEPTH-15)||
           !file.integer(section,"Price",house.price,1,1000000)||
           !file.integer(section,"Slots",house.slots,1,10)){
            if(error.empty())error=file.lastError();return false;
        }
        if(regions::waterAt(house.p)){error="House is in water: "+house.id;return false;}
        parsedHouses.push_back(house);
    }
    shops=std::move(parsedShops);items=std::move(parsedItems);
    houses=std::move(parsedHouses);reset();return true;
}
const std::string& lastError(){return error;}
void reset(){for(auto& house:houses)house.owned=false;close();}
Menu menu(){return currentMenu;}
int selected(){return currentSelection;}
std::string menuTitle(){
    if(currentMenu==Menu::Shop&&currentIndex>=0&&currentIndex<int(shops.size()))
        return shops[currentIndex].name;
    if(currentMenu==Menu::House&&currentIndex>=0&&currentIndex<int(houses.size()))
        return houses[currentIndex].name;
    return {};
}
std::vector<MenuEntry> menuEntries(){
    std::vector<MenuEntry> result;
    if(currentMenu==Menu::Shop){
        for(const auto& item:items)
            result.push_back({item.name,item.price,available(item)&&game::money>=item.price});
    }else if(currentMenu==Menu::House&&currentIndex>=0&&currentIndex<int(houses.size())){
        const House& house=houses[currentIndex];
        if(!house.owned)
            result.push_back({"Buy this house",house.price,
                ownedHouseCount()<10&&game::money>=house.price});
        else{
            result.push_back({"Store nearest owned vehicle",0,
                nearestOwnedVehicle(currentIndex)>=0});
            for(int index=0;index<int(houses.size());++index)
                if(index!=currentIndex&&houses[index].owned)
                    result.push_back({"Fast travel: "+houses[index].name,0,canTravel()});
        }
    }
    return result;
}
void openShop(int index){
    if(index<0||index>=int(shops.size())||game::occupied>=0||
       game::len(game::player-shops[index].p)>75)return;
    currentMenu=Menu::Shop;currentIndex=index;currentSelection=0;
    std::fill(std::begin(game::keys),std::end(game::keys),false);
    game::leftMouse=false;
}
void openHouse(int index){
    if(index<0||index>=int(houses.size())||game::occupied>=0||
       game::len(game::player-houses[index].p)>75)return;
    currentMenu=Menu::House;currentIndex=index;currentSelection=0;
    std::fill(std::begin(game::keys),std::end(game::keys),false);
    game::leftMouse=false;
}
void close(){currentMenu=Menu::None;currentIndex=-1;currentSelection=0;}
int ownedHouseCount(){
    return int(std::count_if(houses.begin(),houses.end(),
        [](const House& house){return house.owned;}));
}
bool buyItem(int itemIndex){
    if(currentMenu!=Menu::Shop||currentIndex<0||itemIndex<0||
       itemIndex>=int(items.size())||game::occupied>=0)return false;
    const Item& item=items[itemIndex];
    if(!available(item)){game::announce("Item unavailable or already full.",3);return false;}
    if(!game::spendMoney(item.price)){
        game::announce("Not enough money.",3);return false;
    }
    if(item.kind==ItemKind::Weapon){
        game::unlocked[item.weapon]=true;
        if(weapons::stats(item.weapon).melee){
            game::ammo[item.weapon]=-1;game::magazine[item.weapon]=1;
        }else if(game::ammo[item.weapon]>=0)
            game::ammo[item.weapon]=std::min(9999,game::ammo[item.weapon]+item.amount);
        if(game::magazine[item.weapon]==0&&game::ammo[item.weapon]>0){
            int load=std::min(weapons::stats(item.weapon).magazine,game::ammo[item.weapon]);
            game::magazine[item.weapon]=load;game::ammo[item.weapon]-=load;
        }
        game::weapon=item.weapon;
    }else if(item.kind==ItemKind::Ammo){
        game::ammo[game::weapon]=std::min(9999,game::ammo[game::weapon]+item.amount);
    }else if(item.kind==ItemKind::Health){
        game::health=std::min(game::PLAYER_MAX_HEALTH,
            game::health+item.amount*(game::PLAYER_MAX_HEALTH/100.0f));
    }else if(item.kind==ItemKind::Armor){
        game::armor=std::min(100.0f,game::armor+item.amount);
    }else if(item.kind==ItemKind::Wanted){
        police::setWantedLevel(police::wantedLevel()-item.amount);
    }else if(item.kind==ItemKind::RepairKit){
        game::repairKits=std::min(10,game::repairKits+item.amount);
    }else if(item.kind==ItemKind::Vehicle){
        int vehicleIndex=availableVehicle(item.vehicleKind,shops[currentIndex].p);
        if(vehicleIndex<0){game::creditMoney(item.price);return false;}
        auto& vehicle=game::vehicles[vehicleIndex];
        vehicle.owned=true;vehicle.garageHouseId.clear();
        game::Vec2 position=item.vehicleKind==game::Kind::Boat?
            game::Vec2{shops[currentIndex].p.x,game::SHORE+45}:
            shops[currentIndex].p+game::Vec2{40,0};
#ifdef MINI_CITY_JOLT
        jolt_world::teleportVehicle(vehicleIndex,position,0);
#else
        vehicle.p=position;vehicle.angle=0;vehicle.velocity={};vehicle.speed=0;
#endif
    }
    changed("PURCHASED "+item.name+"  -$"+std::to_string(item.price));
    return true;
}
bool buyHouse(int houseIndex){
    if(houseIndex<0||houseIndex>=int(houses.size())||houses[houseIndex].owned||
       ownedHouseCount()>=10||game::occupied>=0)return false;
    House& house=houses[houseIndex];
    if(game::len(game::player-house.p)>75||!game::spendMoney(house.price)){
        game::announce("Not enough money or too far from the house.",3);return false;
    }
    house.owned=true;currentSelection=0;
    changed("HOUSE PURCHASED: "+house.name);return true;
}
bool storeVehicle(int houseIndex,int vehicleIndex){
    if(houseIndex<0||houseIndex>=int(houses.size())||vehicleIndex<0||
       vehicleIndex>=int(game::vehicles.size())||!houses[houseIndex].owned||
       game::occupied>=0)return false;
    const House& house=houses[houseIndex];
    auto& vehicle=game::vehicles[vehicleIndex];
    if(!vehicle.owned||vehicle.exploded||vehicle.garageHouseId==house.id||
       game::len(vehicle.p-house.p)>95||game::len(game::player-house.p)>75)return false;
    int occupiedSlots=0;
    for(const auto& other:game::vehicles)if(other.garageHouseId==house.id)++occupiedSlots;
    if(occupiedSlots>=house.slots){game::announce("Garage is full.",3);return false;}
    vehicle.garageHouseId=house.id;
    game::Vec2 parked=house.p+game::Vec2{35+occupiedSlots*30.0f,0};
#ifdef MINI_CITY_JOLT
    jolt_world::teleportVehicle(vehicleIndex,parked,0);
#else
    vehicle.p=parked;vehicle.angle=0;vehicle.velocity={};vehicle.speed=0;
#endif
    changed("VEHICLE STORED AT "+house.name);return true;
}
bool fastTravel(int houseIndex){
    if(houseIndex<0||houseIndex>=int(houses.size())||!houses[houseIndex].owned||
       !canTravel())return false;
    game::Vec2 destination=houses[houseIndex].p+game::Vec2{0,20};
    if(game::solid(destination,12))destination=houses[houseIndex].p;
    if(game::solid(destination,12))return false;
    game::player=game::previousPlayer=destination;
    game::playerY=0;game::playerVerticalSpeed=0;game::playerVelocity={};
    game::grounded=true;game::swimming=false;
    close();changed("FAST TRAVEL: "+houses[houseIndex].name);return true;
}
void handleKey(int key){
    if(currentMenu==Menu::None)return;
    if(key==VK_ESCAPE){close();return;}
    auto entries=menuEntries();
    if(entries.empty()){close();return;}
    if(key==VK_UP)currentSelection=(currentSelection+int(entries.size())-1)%int(entries.size());
    if(key==VK_DOWN)currentSelection=(currentSelection+1)%int(entries.size());
    if(key!=VK_RETURN)return;
    currentSelection=std::clamp(currentSelection,0,int(entries.size())-1);
    if(currentMenu==Menu::Shop){buyItem(currentSelection);return;}
    if(!houses[currentIndex].owned){buyHouse(currentIndex);return;}
    if(currentSelection==0){
        int vehicle=nearestOwnedVehicle(currentIndex);
        if(vehicle>=0)storeVehicle(currentIndex,vehicle);
        else game::announce("Park an owned vehicle near this house.",3);
        return;
    }
    int row=1;
    for(int index=0;index<int(houses.size());++index)
        if(index!=currentIndex&&houses[index].owned){
            if(row==currentSelection){
                if(!fastTravel(index))game::announce("Travel unavailable during combat or missions.",3);
                return;
            }
            ++row;
        }
}
}
