#pragma once
#include "builder.h"
#include "dx11_assets.h"
#include <set>

namespace surface_work {
struct Deposit {builder::Cell cell;game::Vec3 position;float yaw=0;int resource=-1;};
bool load(const char* path=nullptr);
const std::string& lastError();
void clear();
const std::set<builder::Cell>& tilledCells();
const std::set<builder::Cell>& brushedCells();
void restore(std::set<builder::Cell> tilled,std::set<builder::Cell> brushed);
bool validSoil(builder::Cell cell);
bool validDeposit(builder::Cell cell);
bool deposit(builder::Cell cell,Deposit& result);
std::vector<Deposit> nearby(game::Vec2 focus,float radius);
bool tilled(builder::Cell cell);
bool till(const builder::Target& target);
bool brush(builder::Cell cell,int& resource);
void removed(builder::Cell cell);
bool trace(game::Vec3 origin,game::Vec3 direction,float range,builder::Target& target);
void append(std::vector<dx11::ModelInstance>& instances,float radius);
}
