#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "logging.h"
#include <fstream>
#include <string>

namespace logging {
namespace {
std::ofstream output;
}

void initialize(){
    char executable[MAX_PATH]{};
    if(!GetModuleFileNameA(nullptr,executable,MAX_PATH))return;
    std::string path(executable);
    size_t slash=path.find_last_of("\\/");
    output.open(path.substr(0,slash+1)+"MiniCity3D.log",std::ios::app);
    write("Application started");
}

void write(const char* message){
    if(!output)return;
    SYSTEMTIME now{};GetLocalTime(&now);
    output<<now.wYear<<'-'<<now.wMonth<<'-'<<now.wDay<<' '
        <<now.wHour<<':'<<now.wMinute<<':'<<now.wSecond<<"  "<<message<<'\n';
    output.flush();
}

void shutdown(){
    write("Application stopped");
    output.close();
}
}
