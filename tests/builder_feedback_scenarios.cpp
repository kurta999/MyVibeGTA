#include "../src/builder_feedback.h"
#include "builder_test_support.h"
#include "../src/game_internal.h"
#include "../src/dx11_assets.h"
#include "../src/terrain.h"
#include "../src/savegame.h"
#include "../src/jolt_world.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

namespace {
using namespace game;
const builder::Cell cell{20,1,20};
void toggle(){bool next=!builder::active();assert(builder::requestToggle());assert(finishBuilderTransition()&&builder::active()==next);}
void setup(const char* material,const char* tool){builder::reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();wildlife::animals.clear();birds::flock.clear();
    auto low=builder::cellLow(cell);player=previousPlayer={low.x-70,low.z+10};playerY=terrain::baseHeight(player);occupied=enteringVehicle=-1;cameraYaw=0;cameraPitch=-std::atan2(10.0f,70.0f);leftMouse=rightMouse=false;jolt_world::reset();toggle();
    if(tool)assert(builder::addItem(builder::itemIndex(tool),1));assert(builder::place(cell,builder::itemIndex(material),false));builder::update(.01f);
    assert(builder::target().source==builder::Source::Block&&builder::target().cell==cell);}
dx11::ModelInstance equipped(const std::string& id){std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];std::vector<dx11::ModelInstance> instances;dx11::buildScene(groups,instances);
    auto* source=dx11::mesh("builder/"+id);for(auto instance:instances)if(instance.source==source)return instance;assert(false);return {};}
}
void builderFeedbackScenarios(){
    using namespace game;reset();assert(builder::loadCatalog());dx11::loadMeshes(L"assets/models/baked");
    // Live mining contact precedes the drop/durability event; cracks grow only on the selected occupied face.
    setup("granite","iron-pickaxe");auto initial=equipped("iron-pickaxe");leftMouse=true;builder::update(.06f);auto early=builder_feedback::cracks();if(early.empty()){auto t=builder::target();std::printf("empty cracks: progress %.3f point %.3f %.3f %.3f normal %.1f %.1f %.1f cell %d %d %d\n",builder::miningProgress(),t.point.x,t.point.y,t.point.z,t.normal.x,t.normal.y,t.normal.z,t.cell.x,t.cell.y,t.cell.z);}assert(!early.empty());
    assert(builder_feedback::lastContact().serial==0&&builder::inventory()[0].durability==250);builder::update(.2f);
    auto later=builder_feedback::cracks();assert(later.size()>early.size());assert(builder_feedback::lastContact().cue==builder_feedback::Cue::Stone&&builder_feedback::lastContact().serial==1);
    assert(!builder_feedback::particles().empty()&&builder::blocks().count(cell));auto striking=equipped("iron-pickaxe");assert(std::abs(initial.sinPitch-striking.sinPitch)>.3f);
    for(auto line:later){assert(line.a.x<400&&line.b.x<400&&line.a.x>399.5f&&line.b.x>399.5f);assert(line.a.y>0&&line.a.y<40&&line.b.y>0&&line.b.y<40);}
    leftMouse=false;builder::update(.01f);assert(builder::miningProgress()==0&&builder_feedback::cracks().empty());assert(builder::inventory()[0].durability==250);
    leftMouse=true;for(int n=0;n<150&&builder::blocks().count(cell);++n)builder::update(.01f);leftMouse=false;
    assert(!builder::blocks().count(cell)&&builder::inventory()[0].durability==249&&builder::inventory()[1].count==1);assert(builder_feedback::cracks().empty()&&builder::miningPhase()>=.5f);
    for(int n=0;n<100;++n)builder::update(.01f);assert(builder_feedback::particles().empty()&&builder::miningPhase()==0);
    // All material categories use their own contact sound and typed particle appearance.
    struct Scenario{const char* material;const char* tool;builder_feedback::Cue cue;};
    for(auto scenario:{Scenario{"log","iron-axe",builder_feedback::Cue::Wood},{"soil","iron-shovel",builder_feedback::Cue::Soil},
            {"sand","iron-shovel",builder_feedback::Cue::Sand},{"snow","iron-shovel",builder_feedback::Cue::Snow},
            {"leaves","shears",builder_feedback::Cue::Foliage},{"iron-ore","iron-pickaxe",builder_feedback::Cue::Metal}}){
        setup(scenario.material,scenario.tool);int material=builder::itemIndex(scenario.material),tool=builder::itemIndex(scenario.tool);float duration=builder::items()[material].hp/(builder::items()[tool].speed*40);
        leftMouse=true;builder::update(std::min(duration,builder_feedback::cycle(builder::items()[tool].tool))*.51f);
        assert(builder_feedback::lastContact().serial==1&&builder_feedback::lastContact().cue==scenario.cue);assert(builder::inventory()[0].durability==builder::items()[tool].durability);
        for(auto p:builder_feedback::particles())assert(p.item==material&&p.cue==scenario.cue&&!p.dust);
        if(scenario.cue==builder_feedback::Cue::Foliage)assert(builder_feedback::cracks().empty());
        leftMouse=false;builder::update(.01f);builder::handleKey('E');assert(builder_feedback::particles().empty()&&builder_feedback::lastContact().serial==0);builder::closeInventory();
    }
    // Protected ore failures produce no progress, contact, chips or durability loss.
    setup("diamond-ore","wood-pickaxe");leftMouse=true;builder::update(5);assert(builder_feedback::lastContact().serial==0&&builder_feedback::particles().empty());assert(builder_feedback::cracks().empty());
    assert(builder::inventory()[0].durability==60&&builder::blocks().count(cell));
    // Natural-ground cracks follow the real surface instead of floating over a voxel cube.
    setup("soil","iron-shovel");assert(builder::mineBlock(cell));player=previousPlayer={355,420};playerY=0;jolt_world::teleportCharacter(player,playerY);
    auto direction=norm(Vec3{420,0,420}-Vec3{player.x,playerY+(builder::active()?40.0f:31.0f),player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);builder::update(.01f);
    assert(builder::target().source==builder::Source::Ground);leftMouse=true;builder::update(.05f);auto groundLines=builder_feedback::cracks();assert(!groundLines.empty());
    for(auto line:groundLines)for(auto point:{line.a,line.b})assert(std::abs(point.y-terrain::baseHeight({point.x,point.z})-.18f)<.01f);
    // Two halves open and close around a shared pivot in both equipped views; the dropped asset stays complete.
    setup("leaves","shears");auto idle=equipped("shears-half-left"),idleRight=equipped("shears-half-right"),pivot=equipped("shears-pivot");
    assert(idle.qz>0&&idleRight.qz<0&&std::abs(idle.x-pivot.x)<.001f&&std::abs(idle.y-pivot.y)<.001f&&std::abs(idle.z-pivot.z)<.001f);
    leftMouse=true;builder::update(.04f);auto closing=equipped("shears-half-left");assert(closing.qz<idle.qz*.4f);assert(std::abs(closing.sinPitch-idle.sinPitch)<.001f);
    assert(builder::handleKey('C')&&cameraMode==CameraMode::ThirdNear);auto third=equipped("shears-half-left");auto thirdPivot=equipped("shears-pivot");
    assert(third.qz==closing.qz&&len(Vec3{third.x,third.y,third.z}-Vec3{player.x,playerY+20,player.z})<50);assert(std::abs(third.y-thirdPivot.y)<.001f);
    leftMouse=false;builder::update(.01f);assert(builder::handleKey('Q'));std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];std::vector<dx11::ModelInstance> instances;dx11::buildScene(groups,instances);
    int full=0,components=0;for(auto instance:instances){full+=instance.source==dx11::mesh("builder/shears");components+=instance.source==dx11::mesh("builder/shears-half-left");}assert(full==1&&components==0);
    // Bound cosmetic work and clear every transient effect when exiting the layer.
    setup("granite","iron-pickaxe");auto target=builder::target();for(int n=0;n<100;++n)builder_feedback::contact(target,builder::Tool::Pickaxe,true);assert(builder_feedback::particles().size()==192);
    auto saved=builder::capture();toggle();assert(builder_feedback::particles().empty()&&builder_feedback::cracks().empty());toggle();assert(builder_feedback::particles().empty());
    assert(builder::restore(saved));toggle();assert(builder_feedback::particles().empty()&&builder_feedback::lastContact().serial==0);
    reset();assert(savegame::save());jolt_world::shutdown();std::puts("builder feedback: live contact timing, material cues, cracks, shears pivots, bounded particles and F5 isolation passed");
}
