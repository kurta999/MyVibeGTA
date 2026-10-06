#include "../src/builder.h"
#include "builder_test_support.h"
#include "../src/game.h"
#include "../src/game_internal.h"
#include "../src/destruction.h"
#include "../src/jolt_world.h"
#include "../src/savegame.h"
#include "../src/dx11_assets.h"
#include "../src/terrain.h"
#include "../src/camera.h"
#include "../src/wildlife.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <set>

namespace {
void toggle(){bool next=!builder::active();assert(builder::requestToggle());bool done=finishBuilderTransition();
    if(!done||builder::active()!=next)std::fprintf(stderr,"Builder transition failure: finished %d, active %d, requested %d, stage %s, progress %.2f, message [%s]\n",
        int(done),int(builder::active()),int(next),builder::transitionLabel(),builder::transitionProgress(),game::message.c_str());
    assert(done&&builder::active()==next);}
void toolInteractions(){
    using namespace game;
    reset();buildings.clear();trees.clear();vehicles.clear();peds.clear();props.clear();
    const builder::Cell cell{10,0,10};auto low=builder::cellLow(cell);
    const int granite=builder::itemIndex("granite"),diamondOre=builder::itemIndex("diamond-ore");
    auto setup=[&](int material,int equipment,int durability=-1,float distance=35){
        builder::reset();player=previousPlayer={low.x-distance,low.z+20};playerY=0;cameraYaw=cameraPitch=0;
        occupied=-1;crouched=false;toggle();
        if(equipment>=0)assert(builder::addItem(equipment,1,durability));
        assert(builder::place(cell,material,false));builder::update(.01f);
        assert(builder::target().source==builder::Source::Block&&builder::target().cell==cell);
    };
    const int pick=builder::itemIndex("iron-pickaxe"),axe=builder::itemIndex("iron-axe");
    // Equip the same textured asset in both views, using the live skin hand in third person.
    setup(granite,pick);const auto* pickModel=dx11::mesh("builder/iron-pickaxe");
    auto renderedTool=[&](bool expect){
        std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];std::vector<dx11::ModelInstance> instances;
        dx11::buildScene(groups,instances);int matches=0;Vec3 hand{};
        for(const auto& instance:instances)if(instance.source==pickModel){++matches;hand={instance.x,instance.y,instance.z};}
        assert(matches==(expect?1:0));return hand;
    };
    renderedTool(true);assert(builder::handleKey('C')&&cameraMode==CameraMode::ThirdNear);
    auto inspection=camera::compute(player,playerY,false,-1);
    assert(inspection.eye.z>player.z+30); // Right-hand side is visible.
    assert(inspection.eye.x<player.x-90&&inspection.eye.y>playerY+50);
    auto idleHand=renderedTool(true);assert(len(idleHand-Vec3{player.x,playerY+20,player.z})<40);
    leftMouse=true;builder::update(.15f);assert(builder::miningProgress()>0&&builder::miningPhase()>0);
    auto swingHand=renderedTool(true);assert(len(swingHand-idleHand)>.1f);
    builder::handleKey('E');renderedTool(false);builder::closeInventory();
    toggle();renderedTool(false);
    cameraMode=CameraMode::ThirdNear;auto normalView=camera::compute(player,playerY,false,-1);
    assert(std::abs(normalView.eye.z-player.z)<.01f&&normalView.eye.x<player.x-120); // Existing normal camera.
    setup(granite,pick);leftMouse=true;builder::update(.2f);float pickProgress=builder::miningProgress();
    assert(pickProgress>0&&pickProgress<1&&builder::inventory()[0].durability==250);
    leftMouse=false;builder::update(.01f);assert(builder::miningProgress()==0&&builder::blocks().count(cell));
    leftMouse=true;builder::update(.1f);assert(builder::miningProgress()>0);
    assert(builder::handleKey('2')&&builder::miningProgress()==0);builder::update(.01f);
    assert(builder::miningProgress()<.01f&&builder::inventory()[0].durability==250);
    builder::handleKey('E');assert(builder::miningProgress()==0&&!leftMouse);builder::closeInventory();
    setup(granite,axe);leftMouse=true;builder::update(.2f);assert(builder::miningProgress()<pickProgress/4);
    assert(builder::harvestHint()=="Faster with pickaxe");
    for(const auto& pair:{std::pair<const char*,const char*>{"log","iron-axe"},{"sand","iron-shovel"},{"leaves","shears"}}){
        int material=builder::itemIndex(pair.first),equipment=builder::itemIndex(pair.second);
        setup(material,equipment);leftMouse=true;builder::update(.5f);
        assert(!builder::blocks().count(cell)&&builder::inventory()[0].durability==builder::items()[equipment].durability-1);
        assert(builder::inventory()[1].item==material&&builder::inventory()[1].count==1);
    }
    // A matching tool is faster; a family mismatch cannot bypass an ore's tier gate.
    for(int wrong:{builder::itemIndex("gold-pickaxe"),builder::itemIndex("diamond-axe")}){
        setup(diamondOre,wrong);leftMouse=true;builder::update(20);
        assert(builder::blocks().count(cell)&&builder::miningProgress()==0);
        assert(builder::inventory()[0].durability==builder::items()[wrong].durability);
        assert(builder::harvestHint()=="Requires tier 3 pickaxe");
    }
    setup(diamondOre,pick);leftMouse=true;
    for(int n=0;n<200&&builder::blocks().count(cell);++n)builder::update(.01f);
    assert(!builder::blocks().count(cell)&&builder::inventory()[0].durability==249);
    assert(builder::inventory()[1].item==builder::itemIndex("diamond")&&builder::inventory()[1].count==1);
    builder::update(2);assert(builder::inventory()[1].count==1);
    auto save=builder::capture();assert(builder::restore(save));toggle();
    assert(builder::inventory()[0].durability==249&&builder::inventory()[1].count==1);
    setup(granite,pick,1);leftMouse=true;builder::update(2);
    assert(!builder::blocks().count(cell)&&builder::inventory()[0].item==-1);
    assert(builder::inventory()[1].item==granite&&builder::inventory()[1].count==1);
    assert(!builder::addItem(pick,1,0)); // Broken tools cannot be introduced through pickup or restore.
    // Held use repeats at a bounded cadence and stops at the player's occupied cell.
    setup(granite,granite,-1,95);assert(builder::addItem(granite,31));
    builder::setUseHeld(true);assert(builder::blocks().size()==2&&builder::inventory()[0].count==31);
    builder::update(.1f);assert(builder::blocks().size()==2);
    builder::update(.11f);assert(builder::blocks().size()==3&&builder::inventory()[0].count==30);
    builder::update(.3f);assert(builder::blocks().size()==3&&builder::inventory()[0].count==30);
    builder::setUseHeld(false);assert(!rightMouse);builder::update(1);
    assert(builder::blocks().size()==3&&builder::inventory()[0].count==30);
    setup(builder::itemIndex("chest"),granite,-1,95);builder::setUseHeld(true);
    assert(builder::inventoryOpen());builder::closeInventory();builder::update(1);
    assert(builder::blocks().size()==1); // Closing a chest does not resume a stale held press.
    keys[VK_SHIFT]=true;builder::update(.01f);assert(crouched);
    builder::setUseHeld(true);assert(!builder::inventoryOpen()&&builder::blocks().size()==2);
    keys[VK_SHIFT]=false;builder::setUseHeld(false);builder::update(.01f);assert(!crouched);
    toggle();assert(!builder::active());toggle();builder::update(1);assert(builder::blocks().size()==2);
    reset();
}
}
void builderScenarios(){
    using namespace game;
    reset();assert(builder::loadCatalog());
    assert(builder::items().size()==67);std::set<std::string> ids;int tools=0,rocks=0;
    for(const auto& i:builder::items()){assert(ids.insert(i.id).second);tools+=i.tool!=builder::Tool::None;}
    assert(tools==22);for(const char* id:{"chalk","mudstone","shale","tuff","pumice","sandstone","limestone","travertine","dolostone","conglomerate","slate","marble","schist","gneiss","andesite","granite","diorite","gabbro","basalt","quartzite"}){assert(builder::itemIndex(id)>=0);++rocks;}assert(rocks==20);
    dx11::loadMeshes(L"assets/models/baked");
    for(const auto& item:builder::items()){auto model=dx11::mesh("builder/"+item.id);assert(model&&model->textured&&!model->vertices.empty());}
    const auto* ingot=dx11::mesh("builder/iron-ingot");const auto* stick=dx11::mesh("builder/stick");
    const auto* gem=dx11::mesh("builder/diamond");
    assert(ingot->maxY-ingot->minY<(ingot->maxX-ingot->minX)*.3f);
    assert(stick->maxY-stick->minY>(stick->maxX-stick->minX)*8);
    assert(gem->vertices.size()>36); // Faceted gem geometry, rather than the generic resource cube.
    // Keep the real generated definitions and IDs so save/load reconstructs the same world.
    player=previousPlayer={300,235};playerY=terrain::height(player);occupied=-1;
    jolt_world::teleportCharacter(player,playerY);
    toggle();assert(builder::active()&&cameraMode==CameraMode::FirstWide);
    int granite=builder::itemIndex("granite"),chest=builder::itemIndex("chest"),pick=builder::itemIndex("iron-pickaxe");
    assert(builder::addItem(granite,64));assert(builder::addItem(granite,10));assert(builder::addItem(pick,1));
    assert(builder::inventory()[0].count==64&&builder::inventory()[1].count==10&&builder::inventory()[2].durability==250);
    assert(!builder::place(builder::cellAt({player.x,playerY,player.z}),granite,false));
    // Find a valid empty site in reach of the starting player without editing actors.
    builder::Cell cell{};bool found=false;
    for(int dx=-3;dx<=3&&!found;++dx)for(int dz=-3;dz<=3&&!found;++dz){cell=builder::cellAt({player.x+dx*40,0,player.z+dz*40});if(builder::place(cell,granite,false))found=true;}
    assert(found);auto low=builder::cellLow(cell);auto center=low+Vec3{20,20,20};
    assert(builder::contains(center));float entry=0;assert(builder::segment(center+Vec3{-70,0,0},center+Vec3{70,0,0},entry));
    Vec3 impact{};assert(bulletSolidSegment(center+Vec3{-70,0,0},center+Vec3{70,0,0},impact));assert(std::abs(impact.x-low.x)<.01f);
    jolt_world::step(1.0f/60);assert(jolt_world::activeBuilderColliderCount()>0);
    // The live capsule can stand on a builder block, then returns to normal ground.
    player=previousPlayer={center.x,center.z};playerY=builder::BLOCK_SIZE+3;jolt_world::teleportCharacter(player,playerY);
    for(int n=0;n<120;++n){jolt_world::moveCharacter({},false,1.0f/60);jolt_world::step(1.0f/60);}
    assert(grounded&&std::abs(playerY-builder::BLOCK_SIZE)<1);
    toggle();assert(!builder::active()&&!builder::contains(center)&&jolt_world::activeBuilderColliderCount()==0);
    for(int n=0;n<120;++n){jolt_world::moveCharacter({},false,1.0f/60);jolt_world::step(1.0f/60);}
    assert(grounded&&std::abs(playerY-terrain::height(player))<1);
    toggle();assert(builder::active()&&builder::contains(center));
    // Re-entry cannot leave the capsule inside a restored builder block.
    assert(!builder::contains({player.x,playerY+18,player.z},10));
    auto before=builder::capture();assert(!builder::restore("Version=999\n"));assert(builder::capture()==before);
    // Builder scenery changes are isolated in both directions, including whole-object legacy damage.
    assert(!buildings.empty()&&!trees.empty());const auto original=buildings[0];bool oldTree=trees[0].destroyed;
    assert(destruction::cut(0,{{original.x,0,original.z},{original.x+40,40,original.z+40}}));trees[0].destroyed=true;trees[0].health=0;
    toggle();assert(!buildings[0].damaged&&trees[0].destroyed==oldTree);
    toggle();assert(buildings[0].damaged&&trees[0].destroyed);
    assert(savegame::save());assert(savegame::load());assert(!builder::active()&&!builder::contains(center));
    toggle();assert(builder::contains(center)&&buildings[0].damaged&&trees[0].destroyed);assert(builder::inventory()[2].durability==250);
    // Persist the carried inventory stack, and conserve items during splitting/transfers.
    assert(builder::handleKey('E'));auto r=builder::slotRect(0,screenW,screenH);
    builder::mouse(r.x+3,r.y+3,true,false);assert(builder::cursor().count==32&&builder::inventory()[0].count==32);
    auto serialized=builder::capture();builder::closeInventory();assert(builder::restore(serialized));
    assert(builder::cursor().count==32&&builder::inventory()[0].count==32);
    toggle();builder::closeInventory();assert(builder::inventory()[0].count==64);
    assert(builder::mineBlock(cell));assert(!builder::contains(center));assert(!builder::mineBlock(cell));
    // Chest model, placement and inventory transfer interface are present.
    assert(chest>=0&&builder::place(cell,chest,false));
    toggle();assert(!builder::contains(center));toggle();assert(builder::contains(center));
    player=previousPlayer={low.x-35,low.z+20};playerY=low.y;cameraYaw=0;cameraPitch=0;
    jolt_world::teleportCharacter(player,playerY);builder::use();assert(builder::inventoryOpen()&&builder::chest());
    r=builder::slotRect(0,screenW,screenH);int stored=builder::inventory()[0].count;assert(stored>0);
    builder::mouse(r.x+3,r.y+3,false,true);assert(builder::inventory()[0].item==-1&&(*builder::chest())[0].count==stored);
    auto chestSave=builder::capture();builder::closeInventory();assert(builder::restore(chestSave));toggle();
    player=previousPlayer={low.x-35,low.z+20};playerY=low.y;cameraYaw=0;cameraPitch=0;builder::update(.3f);builder::use();
    assert(builder::chest()&&(*builder::chest())[0].count==stored);builder::closeInventory();
    int dropsBefore=int(builder::drops().size());builder::blast(center,80,10000);assert(!builder::contains(center));
    assert(int(builder::drops().size())==dropsBefore+2);builder::blast(center,80,10000);assert(int(builder::drops().size())==dropsBefore+2);
    reset();assert(!builder::active()&&builder::blocks().empty());
    toggle();assert(builder::recipes().size()==30);
    assert(!builder::canCraft(0));auto empty=builder::capture();assert(!builder::craft(0)&&builder::capture()==empty);
    assert(builder::addItem(builder::itemIndex("log"),6));
    assert(builder::craft(0)&&builder::inventory()[0].count==5);
    assert(builder::craft(0)&&builder::craft(1)&&builder::craft(2));
    auto recipeIndex=[](const char* id){for(int n=0;n<int(builder::recipes().size());++n)if(builder::recipes()[n].id==id)return n;return -1;};
    assert(builder::addItem(builder::itemIndex("stick"),60));
    assert(builder::addItem(builder::itemIndex("plank"),64));
    assert(builder::addItem(granite,64));
    assert(builder::addItem(builder::itemIndex("iron-ingot"),32));
    assert(builder::addItem(builder::itemIndex("gold-ingot"),32));
    assert(builder::addItem(builder::itemIndex("diamond"),32));
    int ironRecipe=recipeIndex("iron-pickaxe");assert(ironRecipe>=0&&!builder::canCraft(ironRecipe));
    builder::Cell benchCell{};found=false;for(int dx=-3;dx<=3&&!found;++dx)for(int dz=-3;dz<=3&&!found;++dz){benchCell=builder::cellAt({player.x+dx*40,0,player.z+dz*40});if(builder::place(benchCell,builder::itemIndex("crafting-bench"),false))found=true;}assert(found);
    int craftedTools=0;for(int n=0;n<int(builder::recipes().size());++n)if(builder::items()[builder::recipes()[n].result].tool!=builder::Tool::None){assert(builder::canCraft(n)&&builder::craft(n));++craftedTools;}assert(craftedTools==22);
    assert(builder::addItem(builder::itemIndex("coal"),4)&&builder::addItem(builder::itemIndex("iron-ore"),2));
    int smelt=recipeIndex("smelt-iron");assert(smelt>=0&&!builder::canCraft(smelt));
    // A furnace is required for processing; placing the station enables the recipe.
    found=false;for(int dx=-3;dx<=3&&!found;++dx)for(int dz=-3;dz<=3&&!found;++dz){auto c=builder::cellAt({player.x+dx*40,0,player.z+dz*40});if(builder::place(c,builder::itemIndex("furnace"),false))found=true;}assert(found);
    assert(builder::canCraft(smelt)&&builder::craft(smelt));
    reset();toolInteractions();
    std::puts("builder layer, inventory, persistence, textured assets and Jolt isolation scenarios passed");
}
