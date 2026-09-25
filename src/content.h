#pragma once
#include <string>

namespace content {
bool populate(const char* worldPath=nullptr,const char* missionsPath=nullptr);
const std::string& lastError();
int rollPedCash(bool armed);
int pickpocketNoticePercent();
}
