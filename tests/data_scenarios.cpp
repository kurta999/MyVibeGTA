#include "../src/game.h"
#include "../src/content.h"
#include "../src/regions.h"
#include "../src/props.h"
#include "../src/mission_script.h"
#include "../src/world_placements.h"
#include "../src/terrain.h"
#include "../src/data_file.h"
#include "../src/dx11_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <cstdio>

void MissionScriptScenarios(){
    game::reset();assert(mission_script::LastError().empty());
    std::ifstream source(data_file::resourcePath("missions.lua"));assert(source);
    std::string original((std::istreambuf_iterator<char>(source)),{});
    auto temporary=std::filesystem::temp_directory_path()/"minicity-mission-scenario.lua";
    auto write=[&](const std::string& text){std::ofstream file(temporary);file<<text;assert(file.good());};
    // A script edit changes the actual progression predicate without a rebuild.
    std::string edited=original;auto start=edited.find("requires=\"car\"");assert(start!=std::string::npos);
    edited.replace(start,14,"requires=\"foot\"");write(edited);
    assert(content::populate(nullptr,temporary.string().c_str()));
    game::activeMission=-1;game::missionDone.fill(false);game::occupied=-1;game::player=game::missions[0].start;
    mission_script::Start();assert(game::activeMission==0);
    game::player=game::missions[0].goals[0];mission_script::Update(.01f);assert(game::missionStep==1);
    game::missionTime=.005f;mission_script::Update(.01f);assert(game::activeMission==-1&&!game::missionDone[0]);
    // The interaction prompt and F action use the script's start predicate too.
    edited=original;auto radius=edited.find("distance(c,m.start) < 75");assert(radius!=std::string::npos);
    edited.replace(radius,24,"distance(c,m.start) < 150");write(edited);
    assert(content::populate(nullptr,temporary.string().c_str()));
    game::missionDone.fill(false);game::player=game::missions[0].start+game::Vec2{100,0};
    assert(mission_script::StartCandidate()==0);
    bool prompt=false;
    for(int i=0;i<64&&!prompt;++i){prompt=game::interactionPrompt().find("START Sunset Circuit")!=std::string::npos;if(!prompt)game::cycleInteraction();}
    assert(prompt);game::interact();assert(game::activeMission==0);
    game::activeMission=-1;
    auto stableName=std::string(game::missions[0].name);auto stableCount=game::missions.size();
    write("return {version=1}");assert(!content::populate(nullptr,temporary.string().c_str()));
    assert(game::missions.size()==stableCount&&std::string(game::missions[0].name)==stableName);
    write("while true do end");assert(!content::populate(nullptr,temporary.string().c_str()));
    assert(mission_script::LastError().find("budget")!=std::string::npos);
    write("return this is invalid Lua");assert(!content::populate(nullptr,temporary.string().c_str()));
    assert(game::missions.size()==stableCount&&std::string(game::missions[0].name)==stableName);
    // A valid module can still return invalid transitions at runtime. Validate
    // the whole result before changing game state, rewards or notifications.
    auto returned=original.rfind("return {version=1");assert(returned!=std::string::npos);
    for(const std::string& result:{
        "{active=1,step=1,time=25,event='start'}",
        "{active=1.5,step=1,time=25,event='none'}",
        "{active=1,step=1.5,time=25,event='none'}",
        "{active=1,step=1,time=25,event='unknown'}",
        "{active=1,step=1,time=25,event='complete'}"}){
        edited=original;edited.insert(returned,"start=function(c) return "+result+" end\n");write(edited);
        assert(content::populate(nullptr,temporary.string().c_str()));
        game::activeMission=-1;game::missionStep=0;game::missionTime=71;game::missionDone.fill(false);
        int cash=game::money;mission_script::Start();
        assert(!mission_script::LastError().empty());
        assert(game::activeMission==-1&&game::missionStep==0&&game::missionTime==71&&game::money==cash&&!game::missionDone[0]);
    }
    // Replace the exported callbacks, including the inline functions. Assigning
    // a global named 'next' would leave the module's own next callback intact.
    const std::vector<std::pair<std::string,std::string>> invalidQueries={
        {"start_candidate=start_candidate","start_candidate=function(c) return 1.5 end"},
        {"next=function(c) for i,_ in ipairs(missions) do if not c.done[i] then return i end end return 0 end","next=function(c) return '1' end"},
        {"is_target=function(c) local g=goal(c);return g and g.requires==\"target\" or false end","is_target=function(c) return 'yes' end"},
        {"objective=objective","objective=function(c) return false end"}};
    for(int i=0;i<int(invalidQueries.size());++i){
        edited=original;auto callback=edited.find(invalidQueries[i].first,returned);assert(callback!=std::string::npos);
        edited.replace(callback,invalidQueries[i].first.size(),invalidQueries[i].second);write(edited);
        assert(content::populate(nullptr,temporary.string().c_str()));
        if(i==0)assert(mission_script::StartCandidate()==-1);
        else if(i==1)assert(mission_script::Next()==-1);
        else if(i==2)assert(!mission_script::IsTarget());
        else assert(std::string(mission_script::Objective())=="Invalid mission objective");
        assert(!mission_script::LastError().empty());
    }
    std::filesystem::remove(temporary);assert(content::populate());
    std::puts("Lua edited behavior, timeout, malformed input, instruction limit and rollback: PASS");
}
void PlacementDataScenarios(){
    game::reset();
    auto temporary=std::filesystem::temp_directory_path()/"minicity-placement-scenario.txt";
    auto write=[&](const std::string& text){std::ofstream file(temporary);file<<text;assert(file.good());};
    // Actual world population consumes edited records; invalid replacements keep the catalog.
    write("MINICITY_REGIONAL_PLACEMENTS 1\n"
          "building scenario-building 10000 10000 100 100 80 0.5 0.6 0.7\n"
          "tree scenario-tree 5000 5000 0 1 1.5 tree_oak 47 65 20\n"
          "decoration scenario-bush 5020 5000 bush_00 10 8 10\n"
          "pedestrian scenario-ped 5050 5000 35 1 0 0.5 0.6 0.7\n"
          "vehicle scenario-car 5100 5000 0 0 -1 0.5 0.6 0.7\n"
          "base-ground scenario-floor 123 456 223 556 7 0.5 0.6 0.7 -1 9 1850\n");
    assert(world_placements::Load(temporary.string().c_str()));assert(content::populate());regions::populate();
    assert(game::trees.back().id=="scenario-tree"&&game::trees.back().p.x==5000);
    assert(game::buildings.back().id=="scenario-building");
    assert(game::peds.back().id=="scenario-ped"&&game::peds.back().cash>=5);
    assert(game::vehicles.front().id=="scenario-car");
    assert(regions::decorations().size()==1&&regions::decorations()[0].id=="scenario-bush");
    std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];dx11::buildStaticScene(groups);
    assert(groups[9].size()==6);
    assert(groups[9][0].x==123&&groups[9][0].y==7&&groups[9][0].z==456);
    auto nearby=regions::nearbyTreeIndices({5000,5000},10);
    assert(std::find(nearby.begin(),nearby.end(),int(game::trees.size())-1)!=nearby.end());
    write("MINICITY_REGIONAL_PLACEMENTS 1\ntree invalid nan 5000 0 1 1 tree_oak 47 65 0\n");
    assert(!world_placements::Load(temporary.string().c_str()));assert(world_placements::Data().trees[0].id=="scenario-tree");
    write("MINICITY_REGIONAL_PLACEMENTS 1\nvehicle invalid 1 2 0 500 -1 1 1 1\n");
    assert(!world_placements::Load(temporary.string().c_str()));
    write("MINICITY_REGIONAL_PLACEMENTS 1\nunknown invalid\n");assert(!world_placements::Load(temporary.string().c_str()));
    assert(world_placements::Load());
    std::ifstream geographyFile(data_file::resourcePath("geography.ini"));
    std::string geography((std::istreambuf_iterator<char>(geographyFile)),{});
    auto bridge=geography.find("Z0=8400");assert(bridge!=std::string::npos);
    geography.replace(bridge,7,"Z0=8200");write(geography);
    assert(regions::load(nullptr,nullptr,temporary.string().c_str()));
    assert(regions::bridgeAt({7800,8300})&&!regions::waterAt({7800,8300}));
    const auto priorBridge=regions::geography().bridge;
    geography.replace(bridge,7,"Z0=9000");write(geography);
    assert(!regions::load(nullptr,nullptr,temporary.string().c_str()));
    assert(regions::geography().bridge.z0==priorBridge.z0);
    assert(regions::load());assert(!regions::bridgeAt({7800,8300})&&regions::waterAt({7800,8300}));
    std::ifstream terrainFile(data_file::resourcePath("terrain.ini"));std::string shape((std::istreambuf_iterator<char>(terrainFile)),{});
    auto center=terrain::landforms()[0].center;float height=terrain::baseHeight(center);
    auto position=shape.find("Height=");assert(position!=std::string::npos);auto end=shape.find('\n',position);
    shape.replace(position,end-position,"Height=0");write(shape);
    assert(terrain::load(temporary.string().c_str()));assert(std::abs(terrain::baseHeight(center)-height)>10);
    auto revision=terrain::revision();auto heights=terrain::heights();
    position=shape.find("RoadFade=500");assert(position!=std::string::npos);shape.replace(position,12,"RoadFade=0");write(shape);
    assert(!terrain::load(temporary.string().c_str()));assert(terrain::revision()==revision&&terrain::heights()==heights);
    assert(terrain::load());
    std::ifstream propsFile(data_file::resourcePath("props.ini"));std::string propsText((std::istreambuf_iterator<char>(propsFile)),{});
    position=propsText.find("X=150");assert(position!=std::string::npos);propsText.replace(position,5,"X=170");write(propsText);
    assert(props::load(temporary.string().c_str()));props::reset();assert(game::props.front().p.x==170);
    assert(props::load());std::filesystem::remove(temporary);game::reset();
    std::puts("Edited terrain, regional objects, spatial indexes, prop locations and invalid-file rollback: PASS");
}
