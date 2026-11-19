#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <string>

// Location of the player's writable files (savegame.ini, settings.ini).
// Defaults to the executable's folder; MYVIBEGTA_USER_DIR overrides it so
// test runs and tools use an isolated folder instead of a real save.
namespace user_paths {
inline std::string directory(){
    char value[MAX_PATH]{};
    DWORD length=GetEnvironmentVariableA("MYVIBEGTA_USER_DIR",value,MAX_PATH);
    std::string result;
    if(length>0&&length<MAX_PATH){
        result=value;
        std::error_code error;std::filesystem::create_directories(result,error);
    }else{
        char filename[MAX_PATH]{};GetModuleFileNameA(nullptr,filename,MAX_PATH);
        result=filename;result=result.substr(0,result.find_last_of("\\/"));
    }
    if(!result.empty()&&result.back()!='\\'&&result.back()!='/')result+='\\';
    return result;
}
inline std::string file(const char* name){return directory()+name;}
}
