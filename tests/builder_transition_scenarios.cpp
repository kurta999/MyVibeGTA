#include "builder_test_support.h"
#include "../src/game_internal.h"
#include "../src/jolt_world.h"
#include "../src/savegame.h"
#include "../src/excavation.h"
#include "../src/destruction.h"
#include "../src/terrain.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace {
std::string savePath(){char filename[MAX_PATH]{};GetModuleFileNameA(nullptr,filename,MAX_PATH);
    std::string file(filename);return file.substr(0,file.find_last_of("\\/")+1)+"savegame.ini";}
std::string bytes(const std::string& path){std::ifstream input(path,std::ios::binary);assert(input);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};}
void toggle(){bool next=!builder::active();assert(builder::requestToggle());assert(finishBuilderTransition(true)&&builder::active()==next);}
void failedSwitch(const std::string& file,bool blockTemporary){
    using namespace game;
    savegame::flush();auto disk=bytes(file),snapshot=builder::capture();bool mode=builder::active();
    auto position=player,previous=previousPlayer;float y=playerY,hour=gameHour;auto view=cameraMode;bool wasCrouched=crouched;
    auto colliders=jolt_world::activeBuilderColliderCount();auto building=buildings.front();auto tree=trees.front();
    auto blocked=file+(blockTemporary?".tmp":"");
    HANDLE lock=CreateFileA(blocked.c_str(),blockTemporary?GENERIC_READ|GENERIC_WRITE:GENERIC_READ,FILE_SHARE_READ,nullptr,
        blockTemporary?CREATE_ALWAYS:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(lock!=INVALID_HANDLE_VALUE);
    leftMouse=rightMouse=true;keys['W']=true;
    assert(builder::requestToggle());assert(builder::transitioning()&&builder::transitionProgress()==0);
    assert(!leftMouse&&!rightMouse&&!keys['W']);game::update(.12f);
    assert(builder::transitioning()&&builder::active()==mode&&builder::transitionProgress()==0);
    assert(finishBuilderTransition(true));
    assert(builder::active()==mode&&cameraMode==view&&crouched==wasCrouched);
    assert(len(player-position)==0&&len(previousPlayer-previous)==0&&playerY==y&&gameHour==hour);
    assert(jolt_world::activeBuilderColliderCount()==colliders&&builder::capture()==snapshot);
    assert(buildings.front().damaged==building.damaged&&buildings.front().cuts.size()==building.cuts.size());
    assert(trees.front().destroyed==tree.destroyed&&trees.front().health==tree.health);
    assert(message.find("Mode unchanged; press F5 to retry")!=std::string::npos&&!builder::modal());
    assert(!savegame::takeFailure()); // The transition owns this failure notification.
    CloseHandle(lock);if(blockTemporary)assert(DeleteFileA(blocked.c_str()));
    assert(bytes(file)==disk&&GetFileAttributesA((file+".tmp").c_str())==INVALID_FILE_ATTRIBUTES);
}
}
void builderTransitionScenarios(){
    using namespace game;
    reset();dx11::loadMeshes(L"assets/models/baked");assert(builder::loadCatalog());
    peds.clear();vehicles.clear();props.clear();wildlife::animals.clear();birds::flock.clear();
    player=previousPlayer={300,235};playerY=terrain::baseHeight(player);occupied=enteringVehicle=-1;
    cameraMode=CameraMode::ThirdFar;crouched=true;jolt_world::reset();assert(savegame::save());
    // A failed first entry keeps the original world and camera, and retry uses
    // the same ordinary F5 path after releasing the real Windows file lock.
    const auto file=savePath();failedSwitch(file,false);assert(!builder::active());toggle();
    assert(builder::active()&&cameraMode==CameraMode::FirstWide&&!crouched);
    std::puts("transition: failed normal entry leaves layer/camera/actors intact, then F5 retry succeeds");
    int chest=builder::itemIndex("chest"),axe=builder::itemIndex("iron-axe"),sand=builder::itemIndex("sand");
    builder::Cell cell{};bool found=false;
    for(int dx=-3;dx<=3&&!found;++dx)for(int dz=-3;dz<=3&&!found;++dz){auto candidate=builder::cellAt({player.x+dx*40,0,player.z+dz*40});
        if(terrain::baseHeight({candidate.x*40.0f+20,candidate.z*40.0f+20})!=0)continue;
        if(builder::place(candidate,chest,false)){cell=candidate;found=true;}}
    assert(found&&builder::addItem(axe,1,17)&&builder::addItem(sand,7));auto low=builder::cellLow(cell);
    player=previousPlayer={low.x-35,low.z+20};playerY=0;cameraYaw=cameraPitch=0;jolt_world::teleportCharacter(player,0);
    builder::use();assert(builder::inventoryOpen()&&builder::chest());auto r=builder::slotRect(0,screenW,screenH);
    builder::mouse(r.x+3,r.y+3,false,true);assert((*builder::chest())[0].item==axe&&(*builder::chest())[0].durability==17);
    builder::closeInventory();builder::handleKey('2');builder::handleKey('Q');assert(builder::drops().size()==1);
    assert(builder::mineTerrain({cell.x,-1,cell.z}));auto b=buildings.front();
    assert(destruction::cut(0,{{b.x,0,b.z},{b.x+40,std::min(40.0f,b.h),b.z+40}}));
    trees.front().health=17;trees.front().destroyed=true;
    jolt_world::refreshScenery();assert(jolt_world::activeBuilderColliderCount()>0&&savegame::save());
    auto saved=builder::capture();auto center=low+Vec3{20,20,20};assert(builder::contains(center));
    // Check failure at both creation and atomic replacement, in a populated
    // builder layer with chest durability, loose items and world cuts.
    failedSwitch(file,false);assert(builder::contains(center)&&excavation::removed({cell.x,-1,cell.z}));
    failedSwitch(file,true);assert(builder::contains(center)&&builder::capture()==saved);
    std::puts("transition: failed save creation/replacement preserves cuts, collision, chest tools, drops and exact disk bytes");
    toggle();assert(!builder::active()&&cameraMode==CameraMode::ThirdFar&&crouched&&!builder::contains(center));
    assert(!buildings.front().damaged&&!trees.front().destroyed&&jolt_world::activeBuilderColliderCount()==0);
    failedSwitch(file,false);assert(!builder::active());toggle();assert(builder::capture()==saved&&builder::contains(center));
    assert(buildings.front().damaged&&trees.front().destroyed);
    assert(savegame::save()&&savegame::load()&&!builder::active());toggle();
    assert(builder::capture()==saved&&builder::blocks().at(cell).contents[0].durability==17);
    assert(excavation::removed({cell.x,-1,cell.z})&&buildings.front().damaged&&trees.front().destroyed);
    std::puts("transition: successful retries preserve F5 separation and complete edit/item records across on-disk reload");
    reset();assert(savegame::save());jolt_world::shutdown();
}
