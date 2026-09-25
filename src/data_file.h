#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>

namespace data_file {
inline std::string trim(std::string value){
    auto space=[](unsigned char c){return std::isspace(c)!=0;};
    value.erase(value.begin(),std::find_if_not(value.begin(),value.end(),space));
    value.erase(std::find_if_not(value.rbegin(),value.rend(),space).base(),value.end());
    return value;
}
inline std::string resourcePath(const char* name){
    char executable[MAX_PATH]{};
    if(!GetModuleFileNameA(nullptr,executable,MAX_PATH))return {};
    std::string path(executable);
    auto slash=path.find_last_of("\\/");
    return path.substr(0,slash+1)+"data\\"+name;
}
inline bool validId(const std::string& id){
    if(id.empty()||id.size()>64)return false;
    for(unsigned char c:id)if(!std::islower(c)&&!std::isdigit(c)&&c!='-'&&c!='_')return false;
    return true;
}
class Ini {
    std::map<std::string,std::map<std::string,std::string>> sections;
    std::string error;
    bool fail(const std::string& detail){error=detail;return false;}
public:
    bool load(const std::string& path){
        sections.clear();error.clear();
        std::ifstream file(path);
        if(!file)return fail("Cannot open "+path);
        std::string line,section;
        int number=0;
        while(std::getline(file,line)){
            ++number;line=trim(line);
            if(line.empty()||line[0]=='#'||line[0]==';')continue;
            if(line.front()=='['&&line.back()==']'){
                section=trim(line.substr(1,line.size()-2));
                if(section.empty()||sections.count(section))return fail(path+":"+std::to_string(number)+": duplicate or empty section");
                sections.emplace(section,std::map<std::string,std::string>{});
                continue;
            }
            auto equals=line.find('=');
            if(section.empty()||equals==std::string::npos)return fail(path+":"+std::to_string(number)+": expected key=value inside a section");
            std::string key=trim(line.substr(0,equals)),value=trim(line.substr(equals+1));
            if(key.empty()||value.empty()||sections[section].count(key))
                return fail(path+":"+std::to_string(number)+": duplicate or empty key/value");
            sections[section][key]=value;
        }
        if(!file.eof())return fail("Read error in "+path);
        return true;
    }
    const std::string& lastError() const{return error;}
    bool has(const std::string& section,const std::string& key) const{
        auto s=sections.find(section);return s!=sections.end()&&s->second.count(key)!=0;
    }
    bool string(const std::string& section,const std::string& key,std::string& out){
        auto s=sections.find(section);
        if(s==sections.end()||!s->second.count(key))return fail("Missing ["+section+"] "+key);
        out=s->second.at(key);return true;
    }
    bool integer(const std::string& section,const std::string& key,int& out,int low,int high){
        std::string value;if(!string(section,key,value))return false;
        char* end=nullptr;long parsed=std::strtol(value.c_str(),&end,10);
        if(end==value.c_str()||*end||parsed<low||parsed>high)return fail("Invalid ["+section+"] "+key+" ("+value+")");
        out=int(parsed);return true;
    }
    bool real(const std::string& section,const std::string& key,float& out,float low,float high){
        std::string value;if(!string(section,key,value))return false;
        char* end=nullptr;float parsed=std::strtof(value.c_str(),&end);
        if(end==value.c_str()||*end||!std::isfinite(parsed)||parsed<low||parsed>high)
            return fail("Invalid ["+section+"] "+key+" ("+value+")");
        out=parsed;return true;
    }
    bool version(int expected){
        int value=0;return integer("Schema","Version",value,expected,expected);
    }
};
}
