#pragma once
#include "game.h"
#include <array>
#include <map>
#include <cstdint>

namespace builder {
constexpr float BLOCK_SIZE=40.0f;
constexpr float REACH=BLOCK_SIZE*5;
constexpr int INVENTORY_SLOTS=36;
enum class Tool {None,Pickaxe,Axe,Shovel,Hoe,Shears,Brush};
struct Item {
    std::string id,name;bool block=false;
    Tool tool=Tool::None;int tier=0,durability=0;
    float hp=100,speed=1,blastResistance=100;
    Tool harvestTool=Tool::None;int harvestTier=0;
    float handSpeed=5;
    std::string harvestDrop,craftGroup;
    std::string repairMaterial;int repairCount=0,repairAmount=0;
};
struct Stack {int item=-1,count=0,durability=0;};
struct Cell {
    int x=0,y=0,z=0;
    bool operator<(const Cell& b)const{return x!=b.x?x<b.x:y!=b.y?y<b.y:z<b.z;}
    bool operator==(const Cell& b)const{return x==b.x&&y==b.y&&z==b.z;}
};
struct Block {int item=0;float damage=0;std::array<Stack,27> contents{};};
struct Drop {game::Vec3 p{};Stack stack{};};
struct Ingredient {std::string id;int count=1;};
struct Recipe {std::string id,station;int result=-1,count=1;std::vector<Ingredient> inputs;};
enum class Source {None,Block,Building,Tree,Ground,Scenery,Deposit};
struct Target {
    Source source=Source::None;Cell cell{},adjacent{};game::Vec3 point{},normal{};
    int index=-1,item=-1;float distance=REACH;
    std::string objectId;
};
struct Rect {int x,y,w,h;};
bool loadCatalog(const char* path=nullptr);
const std::string& lastError();
const std::vector<Item>& items();
int itemIndex(const std::string& id);
void reset();
bool active();
bool transitioning();
bool modal();
bool inventoryOpen();
const std::vector<Recipe>& recipes();
bool canCraft(int recipe);
bool craft(int recipe);
int recipeScroll();
Rect recipeRect(int index,int width,int height);
Rect repairRect(int width,int height);
bool canRepair();
bool repair();
std::string repairHint();
float transitionProgress();
const char* transitionLabel();
bool requestToggle();
// Called before game simulation. Returns true when simulation must remain paused.
bool advance(float dt);
void update(float dt);
bool handleKey(int key);
void wheel(int direction);
void mouse(int x,int y,bool right,bool shift);
void use();
// Independent of normal-game aiming; opening menus and mode changes cancel it.
void setUseHeld(bool down);
void closeInventory();
const std::map<Cell,Block>& blocks();
const std::vector<Drop>& drops();
const std::array<Stack,INVENTORY_SLOTS>& inventory();
const std::array<Stack,27>* chest();
const Stack& cursor();
int selected();
const Target& target();
float miningProgress();
float miningPhase();
std::string harvestHint();
Rect slotRect(int index,int width,int height);
game::Vec3 cellLow(Cell cell);
Cell cellAt(game::Vec3 point);
bool addItem(int item,int count,int durability=-1);
bool place(Cell cell,int item,bool consume=true);
bool canPlace(Cell cell,int item);
bool mineBlock(Cell cell);
bool mineTerrain(Cell cell);
bool mineScenery(const Target& target);
void blast(game::Vec3 point,float radius,int damage);
bool contains(game::Vec3 point,float pad=0);
bool segment(game::Vec3 start,game::Vec3 end,float& entry);
Target trace(game::Vec3 origin,game::Vec3 direction);
std::uint64_t revision();
// Stored in the same atomic save generation as inventory and ordinary progress.
std::string capture();
bool restore(const std::string& records);
const std::vector<game::Tree>& normalTrees();
}
