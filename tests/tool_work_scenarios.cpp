#include "../src/surface_work.h"
#include "builder_test_support.h"
#include "../src/builder_feedback.h"
#include "../src/excavation.h"
#include "../src/scenery_edits.h"
#include "../src/game_internal.h"
#include "../src/jolt_world.h"
#include "../src/terrain.h"
#include "../src/regions.h"
#include "../src/savegame.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <set>
#include <climits>

namespace {
using namespace game;
void toggle(){bool next=!builder::active();assert(builder::requestToggle());assert(finishBuilderTransition()&&builder::active()==next);}
void clean(){builder::reset();buildings.clear();trees.clear();peds.clear();vehicles.clear();props.clear();wildlife::animals.clear();birds::flock.clear();
    player=previousPlayer={100,100};playerY=terrain::baseHeight(player);occupied=enteringVehicle=-1;leftMouse=rightMouse=false;jolt_world::reset();toggle();}
void aim(Vec3 point,float height=0){player=previousPlayer={point.x-65,point.z};playerY=terrain::baseHeight(player)+height;cameraMode=CameraMode::FirstWide;
    jolt_world::teleportCharacter(player,playerY);auto direction=norm(point-Vec3{player.x,playerY+31,player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);builder::update(.01f);}
int count(int item){int result=0;for(auto s:builder::inventory())if(s.item==item)result+=s.count;for(auto d:builder::drops())if(d.stack.item==item)result+=d.stack.count;return result;}
builder::Cell soilCell(){for(int z=2;z<100;++z)for(int x=2;x<100;++x){Vec2 p{x*40.0f+20,z*40.0f+20};auto cell=builder::cellAt({p.x,terrain::baseHeight(p)-.01f,p.z});
    if(surface_work::validSoil(cell)&&!surface_work::validDeposit(cell)&&terrain::baseHeight(p)==0)return cell;}assert(false);return {};}
surface_work::Deposit exposedDeposit(){
    for(int z=2;z<160;z+=4)for(int x=2;x<160;x+=4)for(auto d:surface_work::nearby({x*40.0f,z*40.0f},100)){
        // Aim at a real mesh triangle through the game's reticle. Avoid incidental plant geometry.
        for(int dz=-5;dz<=5;dz+=2)for(int dx=-5;dx<=5;dx+=2){aim(d.position+Vec3{float(dx),1.5f,float(dz)});
            if(builder::target().source==builder::Source::Deposit&&builder::target().cell==d.cell)return d;}}
    assert(false);return {};
}
void harvestDeposit(const surface_work::Deposit& d){assert(builder::target().source==builder::Source::Deposit&&builder::target().cell==d.cell);
    builder::setUseHeld(true);for(int n=0;n<250&&!surface_work::brushedCells().count(d.cell);++n)builder::update(.01f);builder::setUseHeld(false);
    assert(surface_work::brushedCells().count(d.cell));}
std::string record(const char* type,builder::Cell c){return "Record99999="+std::string(type)+" "+std::to_string(c.x)+" "+std::to_string(c.y)+" "+std::to_string(c.z)+"\n";}
}
void toolWorkScenarios(){
    using namespace game;
    dx11::loadMeshes(L"assets/models/baked");reset();assert(builder::loadCatalog());clean();
    int soil=builder::itemIndex("soil"),tilled=builder::itemIndex("tilled-soil"),brush=builder::itemIndex("brush");assert(soil>=0&&tilled>=0&&brush>=0);
    assert(!surface_work::validSoil({INT_MAX,INT_MAX,INT_MIN})&&!surface_work::validDeposit({-1,0,0}));
    auto cell=soilCell();auto low=builder::cellLow(cell);Vec3 ground{low.x+20,terrain::baseHeight({low.x+20,low.z+20}),low.z+20};
    // All five modeled hoe variants perform an actual RMB action, once, on natural soil.
    for(const auto& tool:builder::items())if(tool.tool==builder::Tool::Hoe){clean();int index=builder::itemIndex(tool.id);assert(builder::addItem(index,1));aim(ground);
        assert(builder::target().source==builder::Source::Ground&&builder::target().cell==cell);float before=terrain::height({ground.x,ground.z});
        builder::setUseHeld(true);assert(surface_work::tilled(cell));assert(builder::inventory()[0].durability==tool.durability-1);
        assert(builder_feedback::lastContact().cue==builder_feedback::Cue::Soil&&builder_feedback::lastContact().tool==builder::Tool::Hoe&&!builder_feedback::particles().empty());
        builder::update(.1f);assert(builder::miningPhase()>0);builder::update(.5f);builder::setUseHeld(false);assert(builder::inventory()[0].durability==tool.durability-1);
        assert(terrain::height({ground.x,ground.z})==before&&!excavation::removed(cell));
        std::vector<dx11::ModelInstance> instances;surface_work::append(instances,600);bool textured=false;
        for(auto& instance:instances)if(instance.source->textureFile==dx11::mesh("builder/tilled-soil")->textureFile){textured=true;assert(!instance.source->vertices.empty());
            for(auto v:instance.source->vertices)assert(v.u>=-.001f&&v.u<=1.001f&&v.v>=-.001f&&v.v<=1.001f);}
        assert(textured);auto snapshot=builder::capture();toggle();assert(!surface_work::tilled(cell));instances.clear();surface_work::append(instances,600);assert(instances.empty());
        toggle();assert(surface_work::tilled(cell));assert(builder::restore(snapshot));assert(!builder::active());toggle();assert(surface_work::tilled(cell));
        auto saved=builder::capture();assert(!builder::restore(saved+record("G",cell))&&builder::capture()==saved);
        assert(!builder::restore(saved+record("G",{INT_MAX,0,0}))&&builder::capture()==saved);
        assert(!builder::restore(saved+record("E",cell))&&builder::capture()==saved);
        assert(builder::mineTerrain(cell));assert(!surface_work::tilledCells().count(cell)&&count(soil)==1);assert(!builder::mineTerrain(cell)&&count(soil)==1);
    }
    std::puts("hoe variants: natural soil, texture, F5, snapshot validation and one-time soil harvest passed");
    // A hoe prepares the top of a placed soil cube. A shovel harvests the original soil resource.
    clean();int hoe=builder::itemIndex("iron-hoe"),shovel=builder::itemIndex("iron-shovel");assert(builder::addItem(hoe,1));assert(builder::addItem(shovel,1));
    builder::Cell cube{cell.x,cell.y+1,cell.z};aim(ground,60);assert(builder::place(cube,soil,false));aim(ground+Vec3{0,40,0},60);
    assert(builder::target().source==builder::Source::Block&&builder::target().normal.y>.7f);builder::use();assert(builder::blocks().at(cube).item==tilled);
    assert(builder::inventory()[0].durability==builder::items()[hoe].durability-1);builder::update(.3f);builder::use();assert(builder::inventory()[0].durability==builder::items()[hoe].durability-1);
    builder::handleKey('2');leftMouse=true;for(int n=0;n<100&&builder::blocks().count(cube);++n)builder::update(.01f);leftMouse=false;
    assert(!builder::blocks().count(cube)&&count(soil)==1&&count(tilled)==0);assert(builder::inventory()[1].durability==builder::items()[shovel].durability-1);
    // Hoes clear actual generated plant triangles, using their suitability rule rather than hand speed.
    clean();assert(builder::addItem(hoe,1));builder::Target plant;bool foundPlant=false;
    for(const auto& prop:regions::decorations())if(prop.modelId.rfind("bush_",0)==0){scenery_edits::Object object;if(!scenery_edits::find("decoration:"+prop.id,object))continue;
        for(int y=3;y<int(object.size.y)&&!foundPlant;y+=4)for(int dz=-20;dz<=20&&!foundPlant;dz+=4){builder::Target hit;hit.distance=200;
            if(scenery_edits::trace(object.position+Vec3{-65,float(y),float(dz)},{1,0,0},200,hit)&&hit.objectId==object.id&&hit.item==builder::itemIndex("leaves")){
                aim(hit.point);if(builder::target().objectId==object.id&&builder::target().item==hit.item){plant=builder::target();foundPlant=true;}}}
        if(foundPlant)break;
    }assert(foundPlant);leftMouse=true;for(int n=0;n<100&&!scenery_edits::removed(plant.objectId,plant.cell);++n)builder::update(.01f);leftMouse=false;
    assert(scenery_edits::removed(plant.objectId,plant.cell)&&count(builder::itemIndex("leaves"))==1&&builder::inventory()[0].durability==builder::items()[hoe].durability-1);
    // Brush use must cancel without a yield or durability charge, then consume the deposit exactly once.
    clean();assert(builder::addItem(brush,1));auto d=exposedDeposit();int initial=builder::inventory()[0].durability;auto originalCuts=excavation::cells();
    builder::setUseHeld(true);builder::update(.25f);assert(builder::miningProgress()>0&&builder::miningProgress()<1);
    assert(builder_feedback::lastContact().cue==builder_feedback::Cue::Brush&&!builder_feedback::particles().empty()&&builder_feedback::cracks().empty());
    {std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];std::vector<dx11::ModelInstance> instances;dx11::buildScene(groups,instances);
        auto* dust=dx11::mesh("effect/builder-dust");assert(dust&&dust->transparent&&dust->unlit);
        assert(!dx11::mesh("effect/smoke")->unlit);int rendered=0;
        for(const auto& instance:instances)if(instance.source==dust){++rendered;assert(instance.y>terrain::baseHeight({instance.x,instance.z}));}
        assert(rendered==int(builder_feedback::particles().size()));}
    builder::setUseHeld(false);builder::update(.01f);
    assert(builder::miningProgress()==0&&count(d.resource)==0&&builder::inventory()[0].durability==initial);
    builder::setUseHeld(true);builder::update(.2f);builder::handleKey('2');builder::update(.1f);assert(builder::miningProgress()==0&&!surface_work::brushedCells().count(d.cell));builder::handleKey('1');
    builder::setUseHeld(true);builder::update(.2f);builder::handleKey('E');assert(builder::miningProgress()==0);builder::closeInventory();builder::update(3);assert(count(d.resource)==0);
    builder::setUseHeld(true);builder::update(.2f);toggle();toggle();builder::update(3);assert(!surface_work::brushedCells().count(d.cell));
    // Re-establish a real target after actor reconciliation.
    d=exposedDeposit();harvestDeposit(d);assert(count(d.resource)==1&&builder::inventory()[0].durability==initial-1&&excavation::cells()==originalCuts);
    int ignored=-1;assert(!surface_work::brush(d.cell,ignored));builder::update(3);assert(count(d.resource)==1);
    auto saved=builder::capture();assert(!builder::restore(saved+record("R",d.cell))&&builder::capture()==saved);
    assert(!builder::restore(saved+record("R",{-1,0,0}))&&builder::capture()==saved);
    assert(savegame::save());assert(savegame::load());buildings.clear();trees.clear();peds.clear();vehicles.clear();props.clear();wildlife::animals.clear();birds::flock.clear();toggle();
    surface_work::Deposit absent;assert(!surface_work::deposit(d.cell,absent)&&count(d.resource)==1&&builder::inventory()[0].durability==initial-1);
    toggle();assert(!surface_work::deposit(d.cell,absent));toggle();assert(!surface_work::deposit(d.cell,absent));
    // Wrong tools cannot recover brush deposits or spend their durability.
    clean();int axe=builder::itemIndex("diamond-axe");assert(builder::addItem(axe,1));d=exposedDeposit();leftMouse=true;builder::update(20);leftMouse=false;
    assert(!surface_work::brushedCells().count(d.cell)&&count(d.resource)==0&&builder::inventory()[0].durability==builder::items()[axe].durability);
    // Full inventory creates exactly one loose resource without replacing a stack.
    clean();assert(builder::addItem(brush,1));assert(builder::addItem(builder::itemIndex("granite"),35*64));d=exposedDeposit();harvestDeposit(d);
    assert(builder::drops().size()==1&&builder::drops()[0].stack.item==d.resource&&builder::drops()[0].stack.count==1);
    assert(builder::inventory()[0].durability==builder::items()[brush].durability-1);builder::update(3);assert(builder::drops().size()==1);
    std::puts("brush: live RMB geometry targeting, cancellation, single yield, full inventory and restart passed");
    // Repair every tool with its configured material: station gate, atomic shortage, quarter repair and clamp.
    int repairs=0;for(const auto& tool:builder::items())if(tool.tool!=builder::Tool::None){clean();int index=builder::itemIndex(tool.id);assert(tool.repairCount==1&&tool.repairAmount>0);
        assert(builder::addItem(index,1,1));auto before=builder::capture();assert(!builder::repair()&&builder::capture()==before);
        assert(builder::place({4,0,2},builder::itemIndex("crafting-bench"),false));before=builder::capture();assert(!builder::canRepair()&&!builder::repair()&&builder::capture()==before);
        int material=builder::itemIndex(tool.repairMaterial=="stone-material"?"granite":tool.repairMaterial);assert(material>=0&&builder::addItem(material,1));
        assert(builder::canRepair()&&builder::repair());assert(builder::inventory()[0].durability==1+tool.repairAmount&&count(material)==0);
        assert(!builder::repair());auto snapshot=builder::capture();assert(builder::restore(snapshot));toggle();assert(builder::inventory()[0].durability==1+tool.repairAmount);
        clean();assert(builder::addItem(index,1,tool.durability-1));assert(builder::addItem(material,1));assert(builder::place({4,0,2},builder::itemIndex("crafting-bench"),false));
        builder::handleKey('E');auto rect=builder::repairRect(screenW,screenH);builder::mouse(rect.x+5,rect.y+5,false,false);
        assert(builder::inventory()[0].durability==tool.durability&&count(material)==0&&!builder::canRepair());assert(builder::handleKey('R'));assert(builder::inventory()[0].durability==tool.durability);
        ++repairs;
    }assert(repairs==22);
    // Picking up a cursor stack blocks repair; selection works while inventory is open.
    clean();assert(builder::addItem(hoe,1,10)&&builder::addItem(brush,1,10)&&builder::addItem(builder::itemIndex("plank"),2));
    assert(builder::place({4,0,2},builder::itemIndex("crafting-bench"),false));builder::handleKey('E');assert(builder::handleKey('2')&&builder::selected()==1);
    auto slot=builder::slotRect(2,screenW,screenH);builder::mouse(slot.x+4,slot.y+4,false,false);assert(builder::cursor().item>=0&&!builder::canRepair());
    builder::mouse(slot.x+4,slot.y+4,false,false);assert(builder::cursor().item<0&&builder::canRepair());builder::handleKey('R');assert(builder::inventory()[1].durability==10+builder::items()[brush].repairAmount);
    std::puts("repair: all 22 tool costs, station/shortage/cursor gates, clamp, UI and snapshot persistence passed");
    // One on-disk generation retains soil work, a consumed deposit and repaired durability together.
    clean();assert(builder::addItem(hoe,1,80)&&builder::addItem(brush,1,80));aim(ground);builder::use();assert(surface_work::tilled(cell));
    builder::handleKey('2');d=exposedDeposit();harvestDeposit(d);assert(builder::inventory()[1].durability==79);
    assert(builder::addItem(builder::itemIndex("plank"),1));aim(ground);assert(builder::place({cell.x+2,cell.y+1,cell.z},builder::itemIndex("crafting-bench"),false));
    assert(builder::repair());int repaired=79+builder::items()[brush].repairAmount;assert(builder::inventory()[1].durability==repaired);
    assert(savegame::save());assert(savegame::load());assert(!builder::active()&&builder::inventory()[0].durability==79&&builder::inventory()[1].durability==repaired);
    assert(surface_work::tilledCells().count(cell)&&surface_work::brushedCells().count(d.cell));toggle();assert(surface_work::tilled(cell)&&!surface_work::deposit(d.cell,absent));
    toggle();assert(!surface_work::tilled(cell));toggle();assert(surface_work::tilled(cell)&&count(d.resource)==1);
    std::puts("combined soil/deposit/repair on-disk persistence and repeated F5 isolation passed");
    reset();assert(savegame::save());std::puts("tool work scenarios passed");
}
