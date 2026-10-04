#pragma once
#include <string>
#include <vector>
namespace radio {
struct Station{std::string id,name,url;};
bool load(const char* path=nullptr);
const std::vector<Station>& stations();
const std::string& lastError();
void select(const std::string& id);
std::string selectedId();
void cycle(int direction);
void update(bool driving,bool paused,int volume);
std::string status();
void shutdown();
}
