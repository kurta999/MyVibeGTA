#pragma once
#include "game.h"
#include <string>
#include <vector>

namespace commerce {
enum class ItemKind { Weapon, Ammo, Health, Armor, Wanted, RepairKit, Vehicle };
struct Shop {std::string id,name;game::Vec2 p;};
struct House {std::string id,name;game::Vec2 p;int price=0,slots=0;bool owned=false;};
struct Item {std::string id,name,reference;ItemKind kind;int amount=0,price=0,weapon=-1;
    game::Kind vehicleKind=game::Kind::Car;};
struct MenuEntry {std::string label;int price=0;bool available=true;};
enum class Menu { None, Shop, House };
extern std::vector<Shop> shops;
extern std::vector<House> houses;
extern std::vector<Item> items;
bool load(const char* path=nullptr);
const std::string& lastError();
void reset();
Menu menu();
int selected();
std::string menuTitle();
std::vector<MenuEntry> menuEntries();
void openShop(int index);
void openHouse(int index);
void handleKey(int key);
void close();
int ownedHouseCount();
bool buyItem(int itemIndex);
bool buyHouse(int houseIndex);
bool storeVehicle(int houseIndex,int vehicleIndex);
bool fastTravel(int houseIndex);
}
