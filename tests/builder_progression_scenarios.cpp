#include "builder_test_support.h"
#include "../src/game_internal.h"
#include "../src/scenery_edits.h"
#include "../src/excavation.h"
#include "../src/surface_work.h"
#include "../src/jolt_world.h"
#include "../src/savegame.h"
#include "../src/terrain.h"
#include "../src/regions.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>

namespace {
using namespace game;
builder::Cell camp{};
std::set<std::pair<int,int>> shafts;
std::set<int> crafted;
float minedSeconds=0;
int index(const std::string& id){int result=builder::itemIndex(id);assert(result>=0);return result;}
int quantity(int item){int result=0;for(auto s:builder::inventory())if(s.item==item)result+=s.count;return result;}
int quantity(const std::string& id){return quantity(index(id));}
int stoneQuantity(){int result=0;for(auto s:builder::inventory())if(s.item>=0&&builder::items()[s.item].craftGroup=="stone-material")result+=s.count;return result;}
int slot(int item){for(int n=0;n<36;++n)if(builder::inventory()[n].item==item)return n;return -1;}
void toggle(){bool next=!builder::active();assert(builder::requestToggle());assert(finishBuilderTransition()&&builder::active()==next);}
void move(Vec2 p,float y){player=previousPlayer=p;playerY=y;jolt_world::teleportCharacter(p,y);}
void aim(Vec3 point,Vec2 standing){move(standing,terrain::baseHeight(standing));cameraMode=CameraMode::FirstWide;
    auto direction=norm(point-Vec3{player.x,playerY+31,player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);builder::update(.01f);}
void clickSlot(int n,bool shift=false){auto r=builder::slotRect(n,screenW,screenH);builder::mouse(r.x+3,r.y+3,false,shift);}
void equip(int item){int n=slot(item);assert(n>=0);
    if(n>=9){builder::handleKey('E');clickSlot(n);clickSlot(8);if(builder::cursor().item>=0)clickSlot(n);assert(builder::cursor().item<0);builder::closeInventory();n=8;}
    assert(builder::handleKey('1'+n)&&builder::inventory()[builder::selected()].item==item);}
void equip(const std::string& id){equip(index(id));}
void campPosition(){auto low=builder::cellLow(camp);move({low.x+100,low.z-70},0);}
void craft(const std::string& id){campPosition();int recipe=-1;
    for(int n=0;n<int(builder::recipes().size());++n)if(builder::recipes()[n].id==id)recipe=n;assert(recipe>=0);
    const auto& definition=builder::recipes()[recipe];int before=quantity(definition.result);
    std::vector<int> ingredients;for(const auto& ingredient:definition.inputs)ingredients.push_back(ingredient.id=="stone-material"?stoneQuantity():quantity(ingredient.id));
    if(!builder::canCraft(recipe))std::printf("cannot craft %s at %.0f %.0f, station %s\n",id.c_str(),player.x,player.z,definition.station.c_str());
    assert(builder::canCraft(recipe));builder::handleKey('E');
    while(recipe<builder::recipeScroll())builder::wheel(1);
    while(recipe>=builder::recipeScroll()+8)builder::wheel(-1);
    auto r=builder::recipeRect(recipe,screenW,screenH);builder::mouse(r.x+3,r.y+3,false,false);builder::closeInventory();
    assert(quantity(definition.result)==before+definition.count);
    for(std::size_t n=0;n<definition.inputs.size();++n){const auto& ingredient=definition.inputs[n];
        assert((ingredient.id=="stone-material"?stoneQuantity():quantity(ingredient.id))==ingredients[n]-ingredient.count);}
    if(builder::items()[definition.result].tool!=builder::Tool::None)assert(crafted.insert(definition.result).second);
}
bool removed(const builder::Target& target){return target.source==builder::Source::Ground?excavation::removed(target.cell):scenery_edits::removed(target.objectId,target.cell);}
void harvest(){auto target=builder::target();assert(target.item>=0&&(target.source==builder::Source::Tree||target.source==builder::Source::Scenery||target.source==builder::Source::Ground));
    int resource=index(builder::items()[target.item].harvestDrop),before=quantity(resource);
    auto equipped=builder::inventory()[builder::selected()];minedSeconds=0;leftMouse=true;
    for(int n=0;n<1200&&!removed(target);++n){builder::update(.05f);minedSeconds+=.05f;}
    leftMouse=false;if(!removed(target))std::printf("unmined target %s (%d %d %d), tool %d, progress %.3f\n",builder::items()[target.item].id.c_str(),target.cell.x,target.cell.y,target.cell.z,equipped.item,builder::miningProgress());
    assert(removed(target)&&quantity(resource)==before+1);
    const auto& material=builder::items()[target.item];const auto* tool=equipped.item>=0?&builder::items()[equipped.item]:nullptr;
    float rate=tool&&tool->tool==material.harvestTool?tool->speed*40:material.handSpeed;
    assert(minedSeconds+.001f>=material.hp/rate&&minedSeconds<material.hp/rate+.051f);
    if(equipped.item>=0&&builder::items()[equipped.item].tool!=builder::Tool::None){auto now=builder::inventory()[builder::selected()];assert(now.item==equipped.item&&now.durability==equipped.durability-1);}
}
bool treeTarget(const char* material){
    for(std::size_t n=0;n<trees.size();++n){scenery_edits::Object object;if(!scenery_edits::treeObject(n,object)||terrain::baseHeight(trees[n].p)!=0||object.size.y>240)continue;
        for(float y=5;y<std::min(190.0f,object.size.y);y+=20)for(float dz:{0.0f,-12.0f,12.0f}){
            Vec3 origin=object.position+Vec3{-object.size.x*.6f-55,y,dz};builder::Target hit;hit.distance=builder::REACH;
            if(!scenery_edits::trace(origin,{1,0,0},builder::REACH,hit)||hit.objectId!=object.id||hit.item!=index(material))continue;
            aim(hit.point,{origin.x,origin.z});auto live=builder::target();
            if(live.objectId==object.id&&live.item==hit.item&&!removed(live))return true;
        }}return false;
}
void logs(int amount){int desired=quantity("log")+amount;
    while(quantity("log")<desired){assert(treeTarget("log"));harvest();}}
bool flatColumn(int x,int z){if(shafts.count({x,z}))return false;auto low=builder::cellLow({x,0,z});
    for(float dx:{.01f,39.99f})for(float dz:{.01f,39.99f})if(terrain::baseHeight({low.x+dx,low.z+dz})!=0||regions::waterAt({low.x+dx,low.z+dz}))return false;
    if(surface_work::validDeposit({x,-1,z}))return false;
    return excavation::validCell({x,-1,z});}
builder::Cell column(const std::string& wanted,int tier=4){
    // Start with shallow deposits and reject columns whose overburden needs
    // a higher tier. Nothing is cut or seeded by this search.
    for(int y=-2;y>=-8;--y)for(int x=20;x<130;++x)for(int z=20;z<130;++z){if(!flatColumn(x,z))continue;
        builder::Cell cell{x,y,z};const auto& material=builder::items()[excavation::material(cell)];
        bool match=wanted=="stone-material"?material.craftGroup==wanted:material.id==wanted;if(!match)continue;
        bool permitted=true;for(int above=-2;above>=y;--above)permitted&=builder::items()[excavation::material({x,above,z})].harvestTier<=tier;
        if(!permitted)continue;move({x*40.0f+20,z*40.0f+20},0);cameraMode=CameraMode::FirstWide;cameraYaw=0;cameraPitch=-PI/2;builder::update(.01f);
        if(builder::target().source==builder::Source::Ground&&builder::target().cell==builder::Cell{x,-1,z}){shafts.insert({x,z});return cell;}}
    assert(false);return {};
}
void settle(){for(int n=0;n<90;++n){jolt_world::moveCharacter({},false,1.0f/60);jolt_world::step(1.0f/60);}}
void digColumn(builder::Cell bottom,const char* pick,const char* shovel){
    for(int y=-1;y>=bottom.y;--y){equip(y==-1?shovel:pick);cameraMode=CameraMode::FirstWide;cameraYaw=0;cameraPitch=-PI/2;builder::update(.01f);
        builder::Cell expected{bottom.x,y,bottom.z};auto target=builder::target();
        if(target.source!=builder::Source::Ground||!(target.cell==expected))std::printf("shaft aim expected %d %d %d, got source %d cell %d %d %d at player %.1f\n",expected.x,expected.y,expected.z,int(target.source),target.cell.x,target.cell.y,target.cell.z,playerY);
        assert(target.source==builder::Source::Ground&&target.cell==expected);harvest();settle();assert(grounded&&std::abs(playerY-y*40.0f)<1);}
}
void minerals(const char* material,int needed,const char* pick,const char* shovel){
    int tier=builder::items()[index(pick)].tier;
    while((std::string(material)=="stone-material"?stoneQuantity():quantity(builder::items()[index(material)].harvestDrop))<needed){auto target=column(material,tier);digColumn(target,pick,shovel);}
    std::printf("progression: harvested %s to %d through live mining and Jolt shaft falls\n",material,needed);
}
void findCamp(){bool found=false;for(int x=20;x<90&&!found;++x)for(int z=20;z<90&&!found;++z){move({x*40.0f+100,z*40.0f-70},0);
    bool clear=true;for(int offset:{0,2,4}){builder::Cell cell{x+offset,0,z};auto low=builder::cellLow(cell);
        clear&=terrain::baseHeight({low.x+20,low.z+20})==0&&!surface_work::validDeposit({cell.x,-1,cell.z})&&builder::canPlace(cell,index("crafting-bench"));}
    if(clear){camp={x,0,z};found=true;}}assert(found);}
void placeStation(const char* id,int offset){equip(id);builder::Cell cell{camp.x+offset,0,camp.z};auto low=builder::cellLow(cell);int before=quantity(id);
    aim(low+Vec3{20,0,20},{low.x-35,low.z+20});assert(builder::target().source==builder::Source::Ground&&builder::target().adjacent==cell);
    builder::update(.3f);
    builder::setUseHeld(true);builder::setUseHeld(false);assert(builder::blocks().count(cell)&&quantity(id)==before-1);}
void openChest(){auto low=builder::cellLow({camp.x+2,0,camp.z});aim(low+Vec3{20,20,20},{low.x-35,low.z+20});
    assert(builder::target().source==builder::Source::Block&&builder::target().item==index("chest"));builder::update(.3f);builder::use();assert(builder::chest());}
void modelCheck(int item,bool dropped=false){const auto& tool=builder::items()[item];auto* source=dx11::mesh("builder/"+tool.id);assert(source&&source->textured&&!source->vertices.empty());
    assert(std::filesystem::exists(source->textureFile));auto icon=std::filesystem::path(source->textureFile).parent_path()/(tool.id+".icon.png");assert(std::filesystem::exists(icon));
    std::vector<dx11::Vertex> groups[dx11::MATERIAL_GROUPS];std::vector<dx11::ModelInstance> instances;dx11::buildScene(groups,instances);int count=0;
    for(const auto& instance:instances){bool match=instance.source==source;
        if(!dropped&&tool.tool==builder::Tool::Shears)match=instance.source==dx11::mesh("builder/shears-half-left")||instance.source==dx11::mesh("builder/shears-half-right")||instance.source==dx11::mesh("builder/shears-pivot");
        if(match){++count;assert(instance.scaleX==instance.scaleY&&instance.scaleY==instance.scaleZ);}}
    assert(count==(!dropped&&tool.tool==builder::Tool::Shears?3:1));}
void roundTripTool(int item,int durability){equip(item);settle();cameraMode=CameraMode::FirstWide;modelCheck(item);assert(builder::handleKey('C'));modelCheck(item);
    auto before=builder::drops().size();assert(builder::handleKey('Q'));assert(builder::drops().size()==before+1);
    const auto drop=builder::drops().back();assert(drop.stack.item==item&&drop.stack.durability==durability);modelCheck(item,true);
    cameraMode=CameraMode::FirstWide;move({drop.p.x,drop.p.z},drop.p.y-5);builder::update(.01f);
    assert(builder::drops().size()==before&&slot(item)>=0&&builder::inventory()[slot(item)].durability==durability);
    openChest();int n=slot(item);assert(n>=0);clickSlot(n,true);int found=0;
    for(auto s:*builder::chest())if(s.item==item){assert(s.count==1&&s.durability==durability);++found;}assert(found==1&&slot(item)<0);builder::closeInventory();
    auto snapshot=builder::capture();toggle();assert(!builder::contains(builder::cellLow({camp.x+2,0,camp.z})+Vec3{20,20,20}));toggle();assert(builder::capture()==snapshot);
    assert(savegame::save()&&savegame::load()&&!builder::active());
    // Keep dynamic crowd/traffic outside this controlled progression fixture;
    // generated terrain, trees, buildings and their stable IDs are untouched.
    peds.clear();vehicles.clear();props.clear();wildlife::animals.clear();birds::flock.clear();jolt_world::reset();toggle();assert(builder::capture()==snapshot);
    openChest();found=0;for(auto s:*builder::chest())if(s.item==item){assert(s.count==1&&s.durability==durability);++found;}assert(found==1);builder::closeInventory();
}
}
void builderProgressionScenarios(){
    using namespace game;reset();dx11::loadMeshes(L"assets/models/baked");assert(builder::loadCatalog());shafts.clear();crafted.clear();
    peds.clear();vehicles.clear();props.clear();wildlife::animals.clear();birds::flock.clear();jolt_world::reset();toggle();findCamp();
    for(auto s:builder::inventory())assert(s.item<0);assert(builder::blocks().empty()&&excavation::cells().empty()&&scenery_edits::records().empty());
    builder::handleKey('9');logs(2);craft("planks");craft("planks");craft("sticks");craft("wood-pickaxe");craft("wood-axe");
    equip("wood-axe");logs(18);for(int n=0;n<18;++n)craft("planks");for(int n=0;n<10;++n)craft("sticks");
    craft("wood-shovel");craft("bench");placeStation("crafting-bench",0);craft("chest");placeStation("chest",2);
    std::puts("progression: empty inventory -> hand-mined generated logs -> UI-crafted starter tools, placed bench and chest");
    minerals("stone-material",4,"wood-pickaxe","wood-shovel");craft("stone-pickaxe");craft("stone-shovel");
    minerals("stone-material",13,"stone-pickaxe","stone-shovel");craft("furnace");placeStation("furnace",4);
    minerals("coal-ore",21,"stone-pickaxe","stone-shovel");minerals("iron-ore",11,"stone-pickaxe","stone-shovel");
    for(int n=0;n<11;++n)craft("smelt-iron");craft("iron-pickaxe");
    minerals("gold-ore",9,"iron-pickaxe","stone-shovel");minerals("diamond-ore",9,"iron-pickaxe","stone-shovel");
    for(int n=0;n<9;++n)craft("smelt-gold");
    std::ofstream report("builder-progression-report.tsv");assert(report);report<<"item\tmodel\ttexture\ticon\trecipe\tspeed\ttier\tmax_durability\tremaining_durability\ttarget\tdrop\taction_seconds\tcrafted_used_dropped_stored_reloaded\n";
    int checked=0;
    for(int item=0;item<int(builder::items().size());++item){const auto& tool=builder::items()[item];if(tool.tool==builder::Tool::None)continue;
        if(!crafted.count(item))craft(tool.id);equip(item);int before=builder::inventory()[builder::selected()].durability;std::string target,resource;float duration=0;
        if(tool.tool==builder::Tool::Axe||tool.tool==builder::Tool::Shears){assert(treeTarget(tool.tool==builder::Tool::Axe?"log":"leaves"));
            target=builder::items()[builder::target().item].id;resource=builder::items()[builder::target().item].harvestDrop;harvest();duration=minedSeconds;}
        else if(tool.tool==builder::Tool::Pickaxe){auto cell=column("stone-material",tool.tier);equip("stone-shovel");cameraYaw=0;cameraPitch=-PI/2;builder::update(.01f);harvest();settle();
            equip(item);cameraYaw=0;cameraPitch=-PI/2;builder::update(.01f);assert(builder::target().source==builder::Source::Ground&&builder::target().cell==cell);
            target=builder::items()[builder::target().item].id;resource=builder::items()[builder::target().item].harvestDrop;harvest();duration=minedSeconds;}
        else if(tool.tool==builder::Tool::Shovel||tool.tool==builder::Tool::Hoe){auto cell=column("stone-material");
            if(tool.tool==builder::Tool::Shovel){target="soil";resource="soil";harvest();duration=minedSeconds;}
            else{assert(builder::target().source==builder::Source::Ground&&surface_work::validSoil(builder::target().cell));auto ground=builder::target().cell;
                builder::update(.3f);
                builder::setUseHeld(true);builder::setUseHeld(false);assert(surface_work::tilled(ground));target="soil";resource="tilled-soil surface";}}
        else{assert(tool.tool==builder::Tool::Brush);bool found=false;
            for(int z=2;z<160&&!found;z+=4)for(int x=2;x<160&&!found;x+=4)for(auto d:surface_work::nearby({x*40.0f,z*40.0f},100)){
                for(int dz=-5;dz<=5&&!found;dz+=2)for(int dx=-5;dx<=5&&!found;dx+=2){aim(d.position+Vec3{float(dx),1.5f,float(dz)},{d.position.x-65,d.position.z});
                    if(builder::target().source!=builder::Source::Deposit||!(builder::target().cell==d.cell))continue;
                    int count=quantity(d.resource);builder::setUseHeld(true);duration=0;
                    for(int n=0;n<100&&!surface_work::brushedCells().count(d.cell);++n){builder::update(.05f);duration+=.05f;}builder::setUseHeld(false);
                    assert(surface_work::brushedCells().count(d.cell)&&quantity(d.resource)==count+1);target="surface-deposit";resource=builder::items()[d.resource].id;found=true;}}
            assert(found);}
        int after=builder::inventory()[builder::selected()].durability;assert(after==before-1);roundTripTool(item,after);++checked;
        auto* model=dx11::mesh("builder/"+tool.id);auto icon=std::filesystem::path(model->textureFile).parent_path()/(tool.id+".icon.png");
        report<<tool.id<<'\t'<<"assets/models/baked/builder/"<<tool.id<<".m3d\t"<<std::filesystem::path(model->textureFile).generic_string()<<'\t'<<icon.generic_string()<<'\t'<<tool.id<<'\t'<<tool.speed<<'\t'<<tool.tier<<'\t'<<tool.durability<<'\t'<<after<<'\t'<<target<<'\t'<<resource<<'\t'<<duration<<"\tPASS\n";
        report.flush();
        std::printf("tool loop %d/22: %s -> %s -> %s, durability %d, modeled pickup/chest/F5/reload passed\n",checked,tool.id.c_str(),target.c_str(),resource.c_str(),after);
    }
    assert(checked==22&&crafted.size()==22);report.close();openChest();int tools=0;
    for(auto s:*builder::chest())if(s.item>=0&&builder::items()[s.item].tool!=builder::Tool::None)++tools;assert(tools==22);builder::closeInventory();
    std::printf("progression: all 22 crafted tools retained in one chest; %zu ground cuts and %zu edited scenery objects persisted\n",excavation::cells().size(),scenery_edits::records().size());
    assert(savegame::save());std::filesystem::copy_file("savegame.ini","builder-progression-save.ini",std::filesystem::copy_options::overwrite_existing);
    reset();assert(savegame::save());jolt_world::shutdown();
}
