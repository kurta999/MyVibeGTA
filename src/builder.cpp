#include "builder.h"
#include "data_file.h"
#include "destruction.h"
#include "jolt_world.h"
#include "game_internal.h"
#include "camera.h"
#include "physics.h"
#include "terrain.h"
#include "excavation.h"
#include "scenery_edits.h"
#include "surface_work.h"
#include "builder_feedback.h"
#include "regions.h"
#include "savegame.h"
#include "grapple.h"
#include "traversal.h"
#include "wildlife.h"
#include "audio.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace builder {
namespace {
std::vector<Item> catalog;
std::vector<Recipe> crafting;
int craftScroll=0;
std::string error;
bool enabled=false,initialized=false,menu=false,hasChest=false;
bool useHeld=false,normalCrouched=false;
int phase=0,hotbar=0;float phaseTime=0,progress=0,placeCooldown=0,miningTime=0,useAnimation=0,finishAnimation=0,miningCycle=.5f;
game::CameraMode normalCamera=game::CameraMode::ThirdNear;
std::uint64_t generation=0;
std::map<Cell,Block> placed;
std::vector<Drop> loose;
std::array<Stack,INVENTORY_SLOTS> slots{};
Stack held{};
Cell openedChest{};
Target aim{},mining{};
struct Scenery {std::vector<game::Building> buildings;std::vector<game::Tree> trees;};
Scenery normalState,builderState;
savegame::WriteReceipt outgoingSave;
bool preparedBaseline=false;
Scenery scenery(){return {game::buildings,game::trees};}
void notice(const std::string& text){game::message=text;game::messageTime=3;}
bool sameTarget(const Target& a,const Target& b){return a.source==b.source&&a.cell==b.cell&&a.index==b.index&&a.item==b.item&&a.objectId==b.objectId;}
bool validStack(const Stack& s){
    if(s.item==-1)return s.count==0&&s.durability==0;
    if(s.item<0||s.item>=int(catalog.size())||s.count<1)return false;
    const auto& i=catalog[s.item];return s.count<=(i.tool==Tool::None?64:1)&&
        s.durability>=(i.tool==Tool::None?0:1)&&s.durability<=i.durability;
}
bool fits(const Stack& a,const Stack& b){return a.item==b.item&&a.durability==b.durability&&a.item>=0;}
int capacity(int item){return catalog[item].tool==Tool::None?64:1;}
bool insert(std::array<Stack,INVENTORY_SLOTS>& inventory,int item,int count,int durability){
    Stack incoming{item,1,durability};int available=0;
    for(const auto& s:inventory)if(s.item<0)available+=capacity(item);else if(fits(s,incoming))available+=capacity(item)-s.count;
    if(available<count)return false;
    for(auto& s:inventory)if(fits(s,incoming)){int amount=std::min(count,capacity(item)-s.count);s.count+=amount;count-=amount;}
    for(auto& s:inventory)if(s.item<0&&count){int amount=std::min(count,capacity(item));s={item,amount,durability};count-=amount;}return true;
}
bool stationNearby(const std::string& id){for(const auto& block:placed)if(catalog[block.second.item].id==id&&
    game::len(cellLow(block.first)+game::Vec3{20,20,20}-game::Vec3{game::player.x,game::playerY+20,game::player.z})<=REACH)return true;return false;}
bool consume(std::array<Stack,INVENTORY_SLOTS>& inventory,const std::string& id,int count,int skip=-1){
    for(int n=0;n<INVENTORY_SLOTS&&count;++n)if(n!=skip&&inventory[n].item>=0){auto& stack=inventory[n];const auto& item=catalog[stack.item];
        if(item.id!=id&&!(id=="stone-material"&&item.craftGroup==id))continue;int amount=std::min(count,stack.count);count-=amount;stack.count-=amount;if(!stack.count)stack={};}return count==0;}
bool repairResult(std::array<Stack,INVENTORY_SLOTS>& result){
    if(!enabled||transitioning()||held.item>=0||!stationNearby("crafting-bench"))return false;
    const auto& stack=slots[hotbar];if(stack.item<0)return false;const auto& item=catalog[stack.item];
    if(item.tool==Tool::None||stack.durability>=item.durability)return false;result=slots;
    if(!consume(result,item.repairMaterial,item.repairCount,hotbar))return false;
    result[hotbar].durability=std::min(item.durability,stack.durability+item.repairAmount);return true;
}
bool suitableTool(const Item* equipment,const Target& target){
    if(!equipment)return false;
    if(equipment->tool==Tool::Hoe&&target.source==Source::Scenery&&catalog[target.item].id=="leaves"){
        scenery_edits::Object object;return scenery_edits::find(target.objectId,object)&&object.kind==scenery_edits::Kind::Plant;
    }
    return equipment->tool==catalog[target.item].harvestTool&&catalog[target.item].harvestTool!=Tool::None;
}
bool craftResult(int index,std::array<Stack,INVENTORY_SLOTS>& result){
    if(!enabled||transitioning()||index<0||index>=int(crafting.size())||held.item>=0)return false;
    const auto& recipe=crafting[index];
    if(recipe.station!="none"){
        bool nearby=false;for(const auto& block:placed)if(catalog[block.second.item].id==recipe.station&&
            game::len(cellLow(block.first)+game::Vec3{20,20,20}-game::Vec3{game::player.x,game::playerY+20,game::player.z})<=REACH)nearby=true;
        if(!nearby)return false;
    }
    result=slots;
    for(const auto& input:recipe.inputs){int remaining=input.count;
        for(auto& stack:result)if(stack.item>=0){const auto& item=catalog[stack.item];
            bool matches=item.id==input.id;
            if(input.id=="stone-material")matches=item.craftGroup==input.id;
            if(!matches)continue;int used=std::min(remaining,stack.count);remaining-=used;stack.count-=used;if(!stack.count)stack={};if(!remaining)break;}
        if(remaining)return false;
    }
    return insert(result,recipe.result,recipe.count,catalog[recipe.result].durability);
}
void clearInputs(){game::leftMouse=game::rightMouse=false;std::fill(std::begin(game::keys),std::end(game::keys),false);
    useHeld=false;game::scopeBlend=0;game::telescopeActive=false;progress=miningTime=useAnimation=finishAnimation=0;mining={};builder_feedback::clear();}
void safeActors(){
    auto blocked=[](game::Vec3 p,float radius,float height){
        for(const auto& b:game::buildings)for(const auto& box:destruction::boxes(b))
            if(p.x+radius>box.low.x&&p.x-radius<box.high.x&&p.z+radius>box.low.z&&p.z-radius<box.high.z&&
                p.y+height>box.low.y+.01f&&p.y<box.high.y-.01f)return true;
        if(enabled)for(const auto& entry:placed){auto lo=cellLow(entry.first),hi=lo+game::Vec3{BLOCK_SIZE,BLOCK_SIZE,BLOCK_SIZE};
            if(p.x+radius>lo.x&&p.x-radius<hi.x&&p.z+radius>lo.z&&p.z-radius<hi.z&&p.y+height>lo.y+.01f&&p.y<hi.y-.01f)return true;}
        return false;
    };
    auto resolve=[&](game::Vec2 original,float y,float radius,float height){
        y=std::max(y,excavation::floorBelow({original.x,y+.1f,original.z}));
        if(!blocked({original.x,y,original.z},radius,height))return game::Vec3{original.x,y,original.z};
        for(int ring=1;ring<=100;++ring)for(int side=0;side<16;++side){float angle=side*game::PI/8;
            auto p=original+game::Vec2{std::cos(angle),std::sin(angle)}*(ring*BLOCK_SIZE);
            if(p.x<radius||p.z<radius||p.x>regions::WIDTH-radius||p.z>regions::DEPTH-radius||regions::waterAt(p))continue;
            float floor=terrain::height(p);if(!blocked({p.x,floor,p.z},radius,height))return game::Vec3{p.x,floor,p.z};}
        return game::Vec3{300,terrain::height({300,235})+2000,235};
    };
    auto p=resolve(game::player,game::playerY,10,36);
    game::player=game::previousPlayer={p.x,p.z};game::playerY=p.y;game::playerVelocity={};game::playerVerticalSpeed=0;
    jolt_world::teleportCharacter(game::player,p.y);
    for(std::size_t n=0;n<game::vehicles.size();++n){auto& v=game::vehicles[n];if(game::len(v.p-game::player)>1400)continue;
        float r=physics::vehicleRadius(v.kind);
        if(v.rideHeight<excavation::floorBelow({v.p.x,v.rideHeight+.1f,v.p.z})-1||blocked({v.p.x,v.rideHeight,v.p.z},r,42*physics::vehicleScale(v.kind))){auto q=resolve(v.p,v.rideHeight,r,42*physics::vehicleScale(v.kind));
            jolt_world::teleportVehicle(n,{q.x,q.z},v.angle,q.y);}}
    for(std::size_t n=0;n<game::peds.size();++n){const auto& ped=game::peds[n];
        if(!ped.alive||ped.drivingVehicle>=0||game::len(ped.p-game::player)>1400)continue;
        auto q=resolve(ped.p,jolt_world::pedHeight(n),9,36);jolt_world::teleportPed(n,{q.x,q.z},q.y);
    }
    wildlife::reconcileScenery();
    jolt_world::reconcileLooseActors();
}
bool boxRay(game::Vec3 origin,game::Vec3 direction,game::Vec3 lo,game::Vec3 hi,float limit,float& hit,game::Vec3& normal){
    float entry=0,exit=limit;normal={};
    const float p[]={origin.x,origin.y,origin.z},d[]={direction.x,direction.y,direction.z},low[]={lo.x,lo.y,lo.z},high[]={hi.x,hi.y,hi.z};
    for(int axis=0;axis<3;++axis){if(std::abs(d[axis])<.000001f){if(p[axis]<low[axis]||p[axis]>high[axis])return false;continue;}
        float first=(low[axis]-p[axis])/d[axis],last=(high[axis]-p[axis])/d[axis];float sign=-1;
        if(first>last){std::swap(first,last);sign=1;}
        if(first>entry){entry=first;normal={};if(axis==0)normal.x=sign;else if(axis==1)normal.y=sign;else normal.z=sign;}
        exit=std::min(exit,last);if(entry>exit)return false;}
    if(exit<0||entry>=limit)return false;hit=entry;return true;
}
void writeStack(std::ostream& out,const Stack& stack){out<<' '<<std::quoted(stack.item>=0?catalog[stack.item].id:"none")<<' '<<stack.count<<' '<<stack.durability;}
bool readStack(std::istream& in,Stack& stack){std::string id;if(!(in>>std::quoted(id)>>stack.count>>stack.durability))return false;
    stack.item=id=="none"?-1:itemIndex(id);return (id=="none"||stack.item>=0)&&validStack(stack);}
void writeScenery(std::ostream& out,char mode,const Scenery& state,int& row){
    for(const auto& b:state.buildings)if(b.damaged){out<<"Record"<<row++<<'='<<mode<<"B "<<std::quoted(b.id)<<' '<<b.pieces.size()<<' '<<b.cuts.size();
        for(const auto& list:{&b.pieces,&b.cuts})for(const auto& box:*list)out<<' '<<box.low.x<<' '<<box.low.y<<' '<<box.low.z<<' '<<box.high.x<<' '<<box.high.y<<' '<<box.high.z;out<<'\n';}
    for(const auto& t:state.trees)if(t.destroyed||t.health<100){out<<"Record"<<row++<<'='<<mode<<"T "<<std::quoted(t.id)<<' '<<t.health<<' '<<t.destroyed<<'\n';}
}
}
bool loadCatalog(const char* path){
    data_file::Ini file;error.clear();int count=0;
    if(!file.load(path?path:data_file::resourcePath("builder.ini"))||!file.version(1)||!file.integer("Catalog","Count",count,20,256)){error=file.lastError();return false;}
    std::vector<Item> parsed;std::set<std::string> ids;const char* tools[]={"none","pickaxe","axe","shovel","hoe","shears","brush"};
    for(int n=0;n<count;++n){auto section="Item"+std::to_string(n);Item i;int block=0;std::string tool,harvestTool;
        if(!file.string(section,"Id",i.id)||!data_file::validId(i.id)||!ids.insert(i.id).second||!file.string(section,"Name",i.name)||
            !file.integer(section,"Block",block,0,1)||!file.string(section,"Tool",tool)||!file.integer(section,"Tier",i.tier,0,5)||
            !file.integer(section,"Durability",i.durability,0,10000)||!file.real(section,"HP",i.hp,1,10000)||
            !file.real(section,"Speed",i.speed,.1f,100)||!file.real(section,"BlastResistance",i.blastResistance,1,10000)||
            !file.string(section,"HarvestTool",harvestTool)||!file.integer(section,"HarvestTier",i.harvestTier,0,5)||
            !file.real(section,"HandSpeed",i.handSpeed,.1f,10000)||!file.string(section,"HarvestDrop",i.harvestDrop)||
            !file.string(section,"CraftGroup",i.craftGroup)||!file.string(section,"RepairMaterial",i.repairMaterial)||
            !file.integer(section,"RepairCount",i.repairCount,0,64)||!file.integer(section,"RepairAmount",i.repairAmount,0,10000)){
            error=file.lastError().empty()?"Invalid builder item":file.lastError();return false;}
        int kind=-1,harvestKind=-1;for(int j=0;j<7;++j){if(tool==tools[j])kind=j;if(harvestTool==tools[j])harvestKind=j;}
        if(kind<0||harvestKind<0||i.name.size()>64||(block&&kind)||(!kind&&i.durability)||(kind&&!i.durability)||
            (i.harvestTier&&!harvestKind)||(i.craftGroup!="none"&&i.craftGroup!="stone-material")||
            (kind&&(!i.repairCount||!i.repairAmount||i.repairAmount>i.durability||i.repairMaterial=="none"))||
            (!kind&&(i.repairCount||i.repairAmount||i.repairMaterial!="none"))){
            error="Invalid builder tool properties";return false;}
        i.block=block!=0;i.tool=Tool(kind);i.harvestTool=Tool(harvestKind);parsed.push_back(i);
    }
    for(const auto& i:parsed){if(i.harvestDrop!="none"&&!ids.count(i.harvestDrop)){error="Unknown builder harvest drop";return false;}
        if(i.repairMaterial!="none"&&i.repairMaterial!="stone-material"&&!ids.count(i.repairMaterial)){error="Unknown repair material";return false;}}
    auto previous=std::move(catalog);catalog=std::move(parsed);
    data_file::Ini recipesFile;int recipeCount=0;
    if(!recipesFile.load(data_file::resourcePath("builder-recipes.ini"))||!recipesFile.version(1)||!recipesFile.integer("Recipes","Count",recipeCount,1,128)){
        error=recipesFile.lastError();catalog=std::move(previous);return false;}
    std::vector<Recipe> parsedRecipes;std::set<std::string> recipeIds;
    for(int n=0;n<recipeCount;++n){auto section="Recipe"+std::to_string(n);Recipe r;std::string output;int inputs=0;
        if(!recipesFile.string(section,"Id",r.id)||!data_file::validId(r.id)||!recipeIds.insert(r.id).second||
            !recipesFile.string(section,"Station",r.station)||!recipesFile.string(section,"Result",output)||
            !recipesFile.integer(section,"Count",r.count,1,64)||!recipesFile.integer(section,"Inputs",inputs,1,4)){
            error=recipesFile.lastError();catalog=std::move(previous);return false;}
        r.result=itemIndex(output);
        if(r.result<0||(r.station!="none"&&itemIndex(r.station)<0)||r.count>capacity(r.result)){error="Invalid builder recipe result/station";catalog=std::move(previous);return false;}
        for(int j=0;j<inputs;++j){Ingredient i;auto prefix="Input"+std::to_string(j);
            if(!recipesFile.string(section,prefix.c_str(),i.id)||!recipesFile.integer(section,(prefix+"Count").c_str(),i.count,1,256)||
                (i.id!="stone-material"&&itemIndex(i.id)<0)){error="Invalid builder recipe ingredient";catalog=std::move(previous);return false;}r.inputs.push_back(i);}
        parsedRecipes.push_back(std::move(r));
    }
    if(!surface_work::load()){error=surface_work::lastError();catalog=std::move(previous);return false;}
    crafting=std::move(parsedRecipes);return true;
}
const std::string& lastError(){return error;}
const std::vector<Item>& items(){return catalog;}
int itemIndex(const std::string& id){for(int n=0;n<int(catalog.size());++n)if(catalog[n].id==id)return n;return -1;}
void reset(){enabled=initialized=menu=hasChest=false;phase=0;phaseTime=progress=placeCooldown=miningTime=0;hotbar=0;
    outgoingSave={};preparedBaseline=false;
    useHeld=normalCrouched=false;useAnimation=finishAnimation=0;builder_feedback::clear();placed.clear();loose.clear();slots={};held={};normalState={};builderState={};aim=mining={};craftScroll=0;excavation::clear();scenery_edits::clear();surface_work::clear();++generation;}
bool active(){return enabled;}
bool transitioning(){return phase!=0;}
bool modal(){return menu||transitioning();}
bool inventoryOpen(){return menu;}
const std::vector<Recipe>& recipes(){return crafting;}
bool canCraft(int recipe){std::array<Stack,INVENTORY_SLOTS> result;return craftResult(recipe,result);}
bool craft(int recipe){std::array<Stack,INVENTORY_SLOTS> result;if(!craftResult(recipe,result)){notice("Need materials, free inventory space, and the recipe's station");return false;}
    slots=result;notice("Crafted "+catalog[crafting[recipe].result].name);savegame::request();return true;}
int recipeScroll(){return craftScroll;}
Rect recipeRect(int index,int width,int height){return {width/2+320,height/2-180+(index-craftScroll)*42,300,40};}
Rect repairRect(int width,int height){return {width/2+320,height/2+164,300,44};}
bool canRepair(){std::array<Stack,INVENTORY_SLOTS> result;return repairResult(result);}
bool repair(){std::array<Stack,INVENTORY_SLOTS> result;if(!repairResult(result)){notice(repairHint());return false;}
    slots=result;notice("Repaired "+catalog[slots[hotbar].item].name);savegame::request();return true;}
std::string repairHint(){
    const auto& stack=slots[hotbar];if(stack.item<0||catalog[stack.item].tool==Tool::None)return "Select a tool in hotbar (1-9)";
    const auto& item=catalog[stack.item];if(stack.durability>=item.durability)return "Tool is fully repaired";
    if(held.item>=0)return "Store the cursor item first";
    if(!stationNearby("crafting-bench"))return "Need a nearby crafting bench";
    return std::to_string(item.repairCount)+" "+item.repairMaterial+" -> +"+std::to_string(std::min(item.repairAmount,item.durability-stack.durability))+" durability";
}
float transitionProgress(){return phase==0?1:float(phase-1)/4;}
const char* transitionLabel(){const char* names[]={"Ready","Saving outgoing layer","Loading world layer","Rebuilding world collision","Checking safe positions"};return names[std::clamp(phase,0,4)];}
bool requestToggle(){
    if(phase||game::occupied>=0||game::enteringVehicle>=0||game::health<=0||wildlife::riding()||traversal::active()){notice("F5 building mode requires standing on foot");return false;}
    if(catalog.empty()&&!loadCatalog()){notice("Builder catalog unavailable: "+error);return false;}
    closeInventory();clearInputs();grapple::release();outgoingSave={};preparedBaseline=false;phase=1;phaseTime=0;return true;
}
bool advance(float dt){
    if(!phase)return menu;
    phaseTime+=dt;if(phaseTime<.12f)return true;phaseTime=0;
    if(phase==1){
        if(!outgoingSave){if(!initialized){normalState=scenery();builderState=normalState;initialized=preparedBaseline=true;}
            else if(enabled)builderState=scenery();else normalState=scenery();
            outgoingSave=savegame::requestCheckpoint();return true;}
        auto status=outgoingSave.status();if(status==savegame::WriteStatus::Pending)return true;
        if(status!=savegame::WriteStatus::Succeeded){outgoingSave={};clearInputs();phase=0;
            if(preparedBaseline){normalState={};builderState={};initialized=preparedBaseline=false;}
            if(status==savegame::WriteStatus::Failed)savegame::takeFailure();
            notice(status==savegame::WriteStatus::Failed?"Could not save world edits. Mode unchanged; press F5 to retry.":
                "World save was replaced by another request. Mode unchanged; press F5 to retry.");return true;}
        outgoingSave={};preparedBaseline=false;++phase;
    }else if(phase==2){enabled=!enabled;const auto& state=enabled?builderState:normalState;
        game::buildings=state.buildings;game::trees=state.trees;
        if(enabled){normalCamera=game::cameraMode;normalCrouched=game::crouched;game::crouched=false;game::cameraMode=game::CameraMode::FirstWide;}
        else {game::cameraMode=normalCamera;game::crouched=normalCrouched;}++generation;++phase;
    }else if(phase==3){jolt_world::refreshScenery();++phase;
    }else{safeActors();clearInputs();phase=0;savegame::request();notice(enabled?"BUILDING MODE | E inventory | LMB mine | RMB place | F5 exit":"NORMAL WORLD | F5 restore building layer");}
    return true;
}
const std::map<Cell,Block>& blocks(){return placed;}
const std::vector<Drop>& drops(){return loose;}
const std::array<Stack,INVENTORY_SLOTS>& inventory(){return slots;}
const std::array<Stack,27>* chest(){auto it=placed.find(openedChest);return menu&&hasChest&&it!=placed.end()?&it->second.contents:nullptr;}
const Stack& cursor(){return held;}
int selected(){return hotbar;}
const Target& target(){return aim;}
float miningProgress(){return progress;}
float miningPhase(){return progress>0?std::fmod(miningTime,miningCycle)/miningCycle:useAnimation>0?1-useAnimation/.32f:finishAnimation>0?.5f+.5f*(1-finishAnimation/.18f):0;}
std::string harvestHint(){
    if(!enabled||aim.item<0)return {};
    const auto& material=catalog[aim.item];const auto& stack=slots[hotbar];
    const Item* equipment=stack.item>=0?&catalog[stack.item]:nullptr;
    bool suitable=suitableTool(equipment,aim);
    if(equipment&&equipment->tool==Tool::Hoe&&(aim.source==Source::Ground||aim.source==Source::Block)&&(material.id=="soil"||material.id=="tilled-soil"))return material.id=="tilled-soil"||surface_work::tilled(aim.cell)?"Soil already tilled":"RMB till exposed soil top";
    if(equipment&&equipment->tool==Tool::Brush)return aim.source==Source::Deposit?"Hold RMB or LMB to brush":"Find a loose surface deposit";
    const char* names[]={"hand","pickaxe","axe","shovel","hoe","shears","brush"};
    if(material.harvestTier&&(!suitable||equipment->tier<material.harvestTier))
        return "Requires tier "+std::to_string(material.harvestTier)+" "+names[int(material.harvestTool)];
    if(!suitable&&material.harvestTool!=Tool::None)return std::string("Faster with ")+names[int(material.harvestTool)];
    return {};
}
Rect slotRect(int index,int width,int height){int x=width/2-270,y=height/2-180;
    if(hasChest)y=std::max(244,y);
    if(index<9)return {x+(index%9)*60,y+240,56,56};
    if(index<36)return {x+((index-9)%9)*60,y+60+((index-9)/9)*60,56,56};
    return {x+((index-36)%9)*60,y-200+((index-36)/9)*60,56,56};}
game::Vec3 cellLow(Cell c){return {c.x*BLOCK_SIZE,c.y*BLOCK_SIZE,c.z*BLOCK_SIZE};}
Cell cellAt(game::Vec3 p){return {int(std::floor(p.x/BLOCK_SIZE)),int(std::floor(p.y/BLOCK_SIZE)),int(std::floor(p.z/BLOCK_SIZE))};}
bool addItem(int item,int count,int durability){
    if(item<0||item>=int(catalog.size())||count<0)return false;
    if(durability<0)durability=catalog[item].durability;
    Stack incoming{item,1,durability};if(!validStack(incoming))return false;
    int available=0;for(const auto& s:slots)if(s.item<0)available+=capacity(item);else if(fits(s,incoming))available+=capacity(item)-s.count;
    if(available<count)return false;
    for(auto& s:slots)if(fits(s,incoming)){int amount=std::min(count,capacity(item)-s.count);s.count+=amount;count-=amount;}
    for(auto& s:slots)if(s.item<0&&count){int amount=std::min(count,capacity(item));s={item,amount,durability};count-=amount;}
    return true;
}
void closeInventory(){
    if(held.item>=0){if(!addItem(held.item,held.count,held.durability))loose.push_back({{game::player.x,game::playerY+5,game::player.z},held});held={};}
    menu=hasChest=false;clearInputs();
}
bool handleKey(int key){
    if(transitioning())return true;
    if(!enabled)return false;
    if(key==VK_F5){requestToggle();return true;}
    if(key=='E'||(key==VK_ESCAPE&&menu)){if(menu)closeInventory();else{menu=true;clearInputs();}return true;}
    if(key>='1'&&key<='9'){hotbar=key-'1';progress=miningTime=useAnimation=finishAnimation=0;mining={};useHeld=false;return true;}
    if(menu){if(key=='R'&&!hasChest)repair();return true;}
    if(key=='Q'){auto& s=slots[hotbar];if(s.item>=0){loose.push_back({{game::player.x,game::playerY+5,game::player.z},{s.item,1,s.durability}});if(--s.count==0)s={};savegame::request();}return true;}
    if(key=='C'){game::cameraMode=game::cameraMode==game::CameraMode::FirstWide?game::CameraMode::ThirdNear:game::CameraMode::FirstWide;return true;}
    if(key=='R'||key=='B'||key=='F'||key=='G'||key==VK_TAB||key=='X'||key=='K')return true;
    return false;
}
void wheel(int direction){if(enabled&&menu){craftScroll=std::clamp(craftScroll-direction,0,std::max(0,int(crafting.size())-8));return;}
    if(enabled&&!modal()){hotbar=(hotbar-direction+9)%9;progress=miningTime=useAnimation=finishAnimation=0;mining={};useHeld=false;}}
void mouse(int x,int y,bool right,bool shift){
    if(!menu||!enabled)return;
    if(!hasChest&&!right){auto r=repairRect(game::screenW,game::screenH);if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h){repair();return;}}
    if(!hasChest&&!right)for(int n=craftScroll;n<std::min(craftScroll+8,int(crafting.size()));++n){auto r=recipeRect(n,game::screenW,game::screenH);
        if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h){craft(n);return;}}
    int hit=-1;int total=chest()?63:36;for(int n=0;n<total;++n){auto r=slotRect(n,game::screenW,game::screenH);if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h){hit=n;break;}}
    if(hit<0)return;
    auto& stack=hit<36?slots[hit]:placed.at(openedChest).contents[hit-36];
    if(shift&&held.item<0&&stack.item>=0){
        auto transfer=[&](Stack& dst){if(stack.item<0)return;int cap=capacity(stack.item);
            if(dst.item<0){dst=stack;stack={};}else if(fits(dst,stack)){int amount=std::min(stack.count,cap-dst.count);dst.count+=amount;stack.count-=amount;if(!stack.count)stack={};}};
        if(hit>=36){for(auto& dst:slots)transfer(dst);}else if(chest()){for(auto& dst:placed.at(openedChest).contents)transfer(dst);}
        else{int first=hit<9?9:0,last=hit<9?36:9;for(int n=first;n<last;++n)transfer(slots[n]);}
    }else if(held.item<0){held=stack;if(right&&held.item>=0){held.count=(stack.count+1)/2;stack.count-=held.count;if(!stack.count)stack={};}else stack={};
    }else if(stack.item<0||fits(stack,held)){int amount=std::min(right?1:held.count,capacity(held.item)-(stack.item<0?0:stack.count));
        if(amount){if(stack.item<0)stack={held.item,0,held.durability};stack.count+=amount;held.count-=amount;if(!held.count)held={};}
    }else if(!right)std::swap(stack,held);
    savegame::request();
}
bool contains(game::Vec3 p,float pad){if(!enabled)return false;for(const auto& b:placed){auto low=cellLow(b.first),high=low+game::Vec3{BLOCK_SIZE,BLOCK_SIZE,BLOCK_SIZE};
    if(p.x>low.x-pad&&p.x<high.x+pad&&p.y>low.y-pad&&p.y<high.y+pad&&p.z>low.z-pad&&p.z<high.z+pad)return true;}return false;}
bool segment(game::Vec3 start,game::Vec3 end,float& entry){if(!enabled)return false;float distance=game::len(end-start);if(distance<.00001f){entry=0;return contains(start);}
    float best=distance+.001f;game::Vec3 normal;for(const auto& b:placed){auto lo=cellLow(b.first);float hit=0;
        if(boxRay(start,(end-start)*(1/distance),lo,lo+game::Vec3{BLOCK_SIZE,BLOCK_SIZE,BLOCK_SIZE},best,hit,normal))best=hit;}
    if(best>distance)return false;entry=best/distance;return true;}
Target trace(game::Vec3 origin,game::Vec3 direction){
    Target result;direction=game::norm(direction);
    auto test=[&](Source source,int index,int item,game::Vec3 low,game::Vec3 high){float hit=0;game::Vec3 normal;
        if(boxRay(origin,direction,low,high,result.distance,hit,normal)){result.source=source;result.index=index;result.item=item;result.distance=hit;
            result.point=origin+direction*hit;result.normal=normal;result.cell=cellAt(result.point-normal*.01f);result.adjacent=cellAt(result.point+normal*.01f);}};
    if(enabled)for(const auto& b:placed){auto lo=cellLow(b.first);test(Source::Block,-1,b.second.item,lo,lo+game::Vec3{BLOCK_SIZE,BLOCK_SIZE,BLOCK_SIZE});}
    for(int n=0;n<int(game::buildings.size());++n)for(const auto& box:destruction::boxes(game::buildings[n]))test(Source::Building,n,itemIndex("brick"),box.low,box.high);
    scenery_edits::trace(origin,direction,result.distance,result);
    if(enabled)surface_work::trace(origin,direction,result.distance,result);
    float fraction=0;if(terrain::segmentHit(origin,origin+direction*result.distance,fraction)){
        result.source=Source::Ground;result.index=-1;result.distance*=fraction;result.point=origin+direction*result.distance;
        result.normal=excavation::surfaceNormal(result.point,direction);result.cell=cellAt(result.point-result.normal*.02f);
        result.adjacent=cellAt(result.point+result.normal*.02f);result.item=surface_work::tilled(result.cell)?itemIndex("tilled-soil"):excavation::material(result.cell);result.objectId.clear();}
    // Entities and props occlude edits even though they are not mineable scenery.
    auto pose=camera::Pose{origin,origin+direction};auto blocker=camera::traceReticle(pose,REACH);
    if(game::len(blocker-origin)+.5f<result.distance)result={};return result;
}
bool canPlace(Cell cell,int item){
    if(!enabled||modal()||item<0||item>=int(catalog.size())||!catalog[item].block||placed.count(cell))return false;
    auto lo=cellLow(cell),hi=lo+game::Vec3{BLOCK_SIZE,BLOCK_SIZE,BLOCK_SIZE};
    if(lo.x<0||lo.z<0||hi.x>regions::WIDTH||hi.z>regions::DEPTH||lo.y<-400||hi.y>3000)return false;
    if(!excavation::removed(cell))for(float x:{lo.x+.1f,hi.x-.1f})for(float z:{lo.z+.1f,hi.z-.1f})if(lo.y<terrain::baseHeight({x,z})-.2f)return false;
    auto overlap=[&](game::Vec3 a,game::Vec3 b){return a.x<hi.x&&b.x>lo.x&&a.y<hi.y&&b.y>lo.y&&a.z<hi.z&&b.z>lo.z;};
    for(const auto& b:game::buildings)for(const auto& box:destruction::boxes(b))if(overlap(box.low,box.high))return false;
    if(overlap({game::player.x-10,game::playerY,game::player.z-10},{game::player.x+10,game::playerY+36,game::player.z+10}))return false;
    for(const auto& p:game::peds)if(p.alive){float y=jolt_world::pedHeight(p);if(overlap({p.p.x-10,y,p.p.z-10},{p.p.x+10,y+36,p.p.z+10}))return false;}
    for(const auto& v:game::vehicles){float r=physics::vehicleRadius(v.kind);if(overlap({v.p.x-r,v.rideHeight,v.p.z-r},{v.p.x+r,v.rideHeight+42*physics::vehicleScale(v.kind),v.p.z+r}))return false;}
    for(const auto& a:wildlife::animals){
        const auto& s=wildlife::species()[a.species];float y=wildlife::originHeight(a),h=a.health>0?s.height:s.width;
        if(y+.5f>=hi.y||y+h<=lo.y)continue;
        auto f=game::forward(a.angle);game::Vec2 side{-f.z,f.x},delta{lo.x+BLOCK_SIZE*.5f-a.p.x,lo.z+BLOCK_SIZE*.5f-a.p.z};
        float halfLength=s.length*.5f,halfWidth=(a.health>0?s.width:s.height)*.5f;
        if(std::abs(delta.x)<BLOCK_SIZE*.5f+std::abs(f.x)*halfLength+std::abs(side.x)*halfWidth&&
           std::abs(delta.z)<BLOCK_SIZE*.5f+std::abs(f.z)*halfLength+std::abs(side.z)*halfWidth&&
           std::abs(delta.x*f.x+delta.z*f.z)<halfLength+BLOCK_SIZE*.5f*(std::abs(f.x)+std::abs(f.z))&&
           std::abs(delta.x*side.x+delta.z*side.z)<halfWidth+BLOCK_SIZE*.5f*(std::abs(side.x)+std::abs(side.z)))return false;
    }
    for(const auto& object:scenery_edits::nearby({lo.x+20,lo.z+20},60))if(scenery_edits::intersects(object,lo+game::Vec3{.01f,.01f,.01f},hi-game::Vec3{.01f,.01f,.01f}))return false;
    if(placed.size()>=16384){notice("Building block limit reached");return false;}
    return true;
}
bool place(Cell cell,int item,bool consume){
    if(!canPlace(cell,item))return false;
    auto& s=slots[hotbar];if(consume&&(s.item!=item||s.count<=0))return false;
    placed.emplace(cell,Block{item});if(consume&&--s.count==0)s={};++generation;savegame::request();return true;
}
bool mineBlock(Cell cell){
    if(!enabled||modal())return false;auto found=placed.find(cell);if(found==placed.end())return false;
    auto p=cellLow(cell)+game::Vec3{20,8,20};auto block=found->second;
    if(!addItem(block.item,1))loose.push_back({p,{block.item,1,0}});
    for(const auto& s:block.contents)if(s.item>=0)loose.push_back({p,s});
    placed.erase(found);++generation;savegame::request();return true;
}
bool mineTerrain(Cell cell){
    if(!enabled||modal()||!excavation::cut(cell))return false;
    surface_work::removed(cell);
    int material=excavation::material(cell),resource=material>=0?itemIndex(catalog[material].harvestDrop):-1;
    if(resource>=0&&!addItem(resource,1))loose.push_back({cellLow(cell)+game::Vec3{20,8,20},{resource,1,0}});
    ++generation;savegame::request();return true;
}
bool mineScenery(const Target& target){
    if(!enabled||modal()||target.objectId.empty()||target.item<0||target.item>=int(catalog.size())||game::len(target.normal)<.5f)return false;
    Target verified;verified.distance=4;
    if(!scenery_edits::trace(target.point+target.normal*2,target.normal*-1,4,verified)||verified.objectId!=target.objectId||!(verified.cell==target.cell)||verified.item!=target.item)return false;
    if(!scenery_edits::cut(target.objectId,target.cell))return false;
    int resource=itemIndex(catalog[target.item].harvestDrop);if(resource>=0&&!addItem(resource,1))loose.push_back({target.point,{resource,1,0}});
    scenery_edits::Object object;if(scenery_edits::find(target.objectId,object)&&object.kind==scenery_edits::Kind::Tree){
        bool foliage=catalog[target.item].id=="leaves";float width=foliage?5:std::min(12.0f,jolt_world::treeRadius(game::trees[object.index])*1.5f);
        jolt_world::spawnFragment(target.point+target.normal*width,{width,foliage?3.0f:14.0f,width},target.normal*25+game::Vec3{0,40,0},foliage?game::rgb(74,120,46):game::rgb(111,77,44));
    }
    ++generation;savegame::request();return true;
}
void blast(game::Vec3 point,float radius,int damage){
    if(!enabled||radius<=0||damage<=0)return;std::vector<Cell> broken;bool changed=false;
    for(auto& block:placed){auto center=cellLow(block.first)+game::Vec3{20,20,20};float distance=game::len(center-point);
        if(distance>=radius)continue;const auto& material=catalog[block.second.item];
        block.second.damage+=(1-distance/radius)*damage*100/material.blastResistance;changed=true;
        if(block.second.damage>=material.hp)broken.push_back(block.first);
    }
    for(auto cell:broken){auto found=placed.find(cell);if(found==placed.end())continue;auto p=cellLow(cell)+game::Vec3{20,8,20};
        loose.push_back({p,{found->second.item,1,0}});for(const auto& stack:found->second.contents)if(stack.item>=0)loose.push_back({p,stack});placed.erase(found);}
    if(changed){++generation;savegame::request();}
}
void use(){
    if(!enabled||modal()||game::carryingBody())return;
    if(placeCooldown>0)return;placeCooldown=.2f;
    auto pose=camera::compute(game::player,game::playerY,false,-1);aim=trace(pose.eye,pose.target-pose.eye);
    if(aim.source==Source::None)return;
    if(aim.source==Source::Block&&catalog[aim.item].id=="chest"&&!game::keys[VK_SHIFT]){openedChest=aim.cell;hasChest=menu=true;clearInputs();return;}
    if(aim.source==Source::Block&&(catalog[aim.item].id=="crafting-bench"||catalog[aim.item].id=="furnace")&&!game::keys[VK_SHIFT]){hasChest=false;menu=true;clearInputs();return;}
    const auto& selected=slots[hotbar];if(selected.item>=0&&catalog[selected.item].tool==Tool::Brush)return;
    auto& s=slots[hotbar];if(s.item>=0&&catalog[s.item].tool==Tool::Hoe){
        bool changed=false;if(aim.source==Source::Block&&catalog[aim.item].id=="soil"&&aim.normal.y>.7f&&!contains(aim.point+game::Vec3{0,1,0})){
            auto found=placed.find(aim.cell);if(found!=placed.end()){found->second.item=itemIndex("tilled-soil");changed=true;}}
        else changed=surface_work::till(aim);
        if(changed){useAnimation=.32f;builder_feedback::contact(aim,Tool::Hoe,true);if(--s.durability<=0){notice("Tool broke");s={};}++generation;savegame::request();}return;
    }
    if(s.item>=0&&catalog[s.item].block){if(!place(aim.adjacent,s.item))notice("Cannot place here: occupied or overlapping scenery");}
}
void setUseHeld(bool down){useHeld=down&&enabled&&!modal();if(useHeld)use();}
void update(float dt){
    if(!enabled||modal())return;builder_feedback::update(dt);placeCooldown=std::max(0.0f,placeCooldown-dt);useAnimation=std::max(0.0f,useAnimation-dt);finishAnimation=std::max(0.0f,finishAnimation-dt);
    game::crouched=game::keys[VK_SHIFT];
    bool carrying=game::carryingBody();
    if(carrying){useHeld=false;progress=miningTime=useAnimation=finishAnimation=0;mining={};}
    if(useHeld){use();if(modal())return;}
    auto pose=camera::compute(game::player,game::playerY,false,-1);aim=trace(pose.eye,pose.target-pose.eye);
    for(auto it=loose.begin();it!=loose.end();){if(game::len(it->p-game::Vec3{game::player.x,game::playerY+8,game::player.z})<30&&addItem(it->stack.item,it->stack.count,it->stack.durability)){
        notice("Collected "+catalog[it->stack.item].name);it=loose.erase(it);savegame::request();}else ++it;}
    if(carrying){aim={};return;}
    const auto& selected=slots[hotbar];bool brushing=selected.item>=0&&catalog[selected.item].tool==Tool::Brush;
    if((!game::leftMouse&&!(useHeld&&brushing))||aim.source==Source::None||(brushing&&aim.source!=Source::Deposit)){progress=miningTime=0;mining={};return;}
    if(!sameTarget(aim,mining)){mining=aim;progress=miningTime=finishAnimation=0;}
    if(aim.item<0)return;
    auto& tool=slots[hotbar];const Item* equipment=tool.item>=0?&catalog[tool.item]:nullptr;
    const auto& material=catalog[aim.item];
    bool suitable=suitableTool(equipment,aim);
    if(material.harvestTier&&(!suitable||equipment->tier<material.harvestTier)){notice(harvestHint());progress=0;return;}
    float hp=material.hp;
    float rate=suitable?equipment->speed*40:material.handSpeed;
    miningCycle=std::max(.01f,std::min(builder_feedback::cycle(equipment?equipment->tool:Tool::None),hp/rate));
    float previousTime=miningTime;
    miningTime+=dt;
    progress=std::min(1.0f,progress+dt*rate/hp);
    bool stroke=int(std::floor(miningTime/miningCycle+.5f))>int(std::floor(previousTime/miningCycle+.5f));
    if(progress<1){if(stroke)builder_feedback::contact(aim,equipment?equipment->tool:Tool::None);return;}
    bool changed=false;
    if(aim.source==Source::Block){
        if(material.harvestDrop!=material.id){
            auto found=placed.find(aim.cell);if(found!=placed.end()){auto contents=found->second.contents;placed.erase(found);
                int resource=itemIndex(material.harvestDrop);if(resource>=0&&!addItem(resource,1))loose.push_back({aim.point,{resource,1,0}});
                for(const auto& stack:contents)if(stack.item>=0)loose.push_back({aim.point,stack});changed=true;}
        }else changed=mineBlock(aim.cell);
    }
    else if(aim.source==Source::Ground)changed=mineTerrain(aim.cell);
    else if(aim.source==Source::Deposit){int resource=-1;changed=surface_work::brush(aim.cell,resource);
        if(changed&&!addItem(resource,1))loose.push_back({aim.point,{resource,1,0}});}
    else if(aim.source==Source::Building){auto lo=cellLow(aim.cell);changed=destruction::cut(std::size_t(aim.index),{lo,lo+game::Vec3{BLOCK_SIZE,BLOCK_SIZE,BLOCK_SIZE}});
        if(changed&&!addItem(aim.item,1))loose.push_back({aim.point,{aim.item,1,0}});
    }else if(aim.source==Source::Tree||aim.source==Source::Scenery)changed=mineScenery(aim);
    if(changed){if(equipment&&equipment->tool!=Tool::None){if(--tool.durability<=0){notice("Tool broke");tool={};}}
        finishAnimation=.18f;builder_feedback::contact(aim,equipment?equipment->tool:Tool::None,true);
        ++generation;savegame::request();}
    progress=miningTime=0;mining={};
}
std::uint64_t revision(){return generation;}
const std::vector<game::Tree>& normalTrees(){return enabled&&initialized?normalState.trees:game::trees;}
std::string capture(){
    std::ostringstream out;out<<std::setprecision(9)<<"Version=1\nInitialized="<<initialized<<"\nSelected="<<hotbar<<'\n';int row=0;
    if(initialized){auto normal=enabled?normalState:scenery(),built=enabled?scenery():builderState;writeScenery(out,'N',normal,row);writeScenery(out,'V',built,row);}
    for(int n=0;n<36;++n){out<<"Record"<<row++<<"=I "<<n;writeStack(out,slots[n]);out<<'\n';}
    out<<"Record"<<row++<<"=H";writeStack(out,held);out<<'\n';
    for(const auto& b:placed){out<<"Record"<<row++<<"=P "<<b.first.x<<' '<<b.first.y<<' '<<b.first.z<<' '<<std::quoted(catalog[b.second.item].id)<<' '<<b.second.damage;
        for(const auto& s:b.second.contents)writeStack(out,s);out<<'\n';}
    for(const auto& d:loose){out<<"Record"<<row++<<"=D "<<d.p.x<<' '<<d.p.y<<' '<<d.p.z;writeStack(out,d.stack);out<<'\n';}
    for(auto cell:excavation::cells())out<<"Record"<<row++<<"=E "<<cell.x<<' '<<cell.y<<' '<<cell.z<<'\n';
    for(const auto& record:scenery_edits::records())for(auto cell:record.second)out<<"Record"<<row++<<"=S "<<std::quoted(record.first)<<' '<<cell.x<<' '<<cell.y<<' '<<cell.z<<'\n';
    for(auto cell:surface_work::tilledCells())out<<"Record"<<row++<<"=G "<<cell.x<<' '<<cell.y<<' '<<cell.z<<'\n';
    for(auto cell:surface_work::brushedCells())out<<"Record"<<row++<<"=R "<<cell.x<<' '<<cell.y<<' '<<cell.z<<'\n';
    return out.str();
}
bool restore(const std::string& records){
    if(records.empty())return true;if(catalog.empty()&&!loadCatalog())return false;
    Scenery normal=scenery(),built=scenery();
    // Each mode's deltas start from healthy generated trees, not legacy normal-mode records.
    for(auto state:{&normal,&built})for(auto& tree:state->trees){tree.health=100;tree.destroyed=false;tree.burning=false;}
    std::array<Stack,INVENTORY_SLOTS> newSlots{};Stack newHeld{};std::map<Cell,Block> newBlocks;std::vector<Drop> newDrops;std::set<Cell> newCuts;
    scenery_edits::Records newSceneryCuts;std::size_t sceneryCutCount=0;
    std::set<Cell> newSoil,newBrushed;
    bool version=false,newInitialized=false;int newSelected=0;std::set<std::string> seen;std::istringstream lines(records);std::string line;
    while(std::getline(lines,line)){line=data_file::trim(line);if(line.empty())continue;auto equal=line.find('=');if(equal==std::string::npos)return false;
        auto key=line.substr(0,equal);if(!seen.insert(key).second)return false;std::istringstream in(line.substr(equal+1));
        if(key=="Version"){int value=0;if(!(in>>value)||value!=1)return false;version=true;}
        else if(key=="Initialized"){int value=0;if(!(in>>value)||value<0||value>1)return false;newInitialized=value!=0;}
        else if(key=="Selected"){if(!(in>>newSelected)||newSelected<0||newSelected>8)return false;}
        else if(key.rfind("Record",0)==0){std::string type;if(!(in>>type))return false;
            if(type=="I"){int index;if(!(in>>index)||index<0||index>=36||!readStack(in,newSlots[index]))return false;}
            else if(type=="H"){if(!readStack(in,newHeld))return false;}
            else if(type=="E"){Cell cell;if(!(in>>cell.x>>cell.y>>cell.z)||!excavation::validCell(cell)||newCuts.size()>=16384||!newCuts.insert(cell).second)return false;}
            else if(type=="S"){std::string id;Cell cell;if(!(in>>std::quoted(id)>>cell.x>>cell.y>>cell.z)||sceneryCutCount>=16384||!scenery_edits::validCut(id,cell)||!newSceneryCuts[id].insert(cell).second)return false;++sceneryCutCount;}
            else if(type=="G"||type=="R"){Cell cell;auto& set=type=="G"?newSoil:newBrushed;if(!(in>>cell.x>>cell.y>>cell.z)||set.size()>=16384||
                !(type=="G"?surface_work::validSoil(cell):surface_work::validDeposit(cell))||!set.insert(cell).second)return false;}
            else if(type=="P"){Cell c;Block b;std::string id;if(!(in>>c.x>>c.y>>c.z>>std::quoted(id)>>b.damage))return false;b.item=itemIndex(id);
                if(b.item<0||!catalog[b.item].block||c.x<0||c.z<0||c.x>=420||c.z>=420||c.y<-10||c.y>=75||!std::isfinite(b.damage)||b.damage<0||b.damage>=catalog[b.item].hp)return false;
                for(auto& s:b.contents)if(!readStack(in,s))return false;if(newBlocks.size()>=16384||!newBlocks.emplace(c,b).second)return false;}
            else if(type=="D"){Drop d;if(!(in>>d.p.x>>d.p.y>>d.p.z)||!std::isfinite(d.p.x)||!std::isfinite(d.p.y)||!std::isfinite(d.p.z)||d.p.x<0||d.p.z<0||d.p.x>regions::WIDTH||d.p.z>regions::DEPTH||!readStack(in,d.stack)||d.stack.item<0||newDrops.size()>=32768)return false;newDrops.push_back(d);}
            else if(type=="NB"||type=="VB"){auto& state=type=="NB"?normal:built;std::string id;int pieces,cuts;
                if(!(in>>std::quoted(id)>>pieces>>cuts)||pieces<0||pieces>4096||cuts<0||cuts>4096)return false;
                auto b=std::find_if(state.buildings.begin(),state.buildings.end(),[&](const game::Building& b){return b.id==id;});if(b==state.buildings.end())return false;
                b->damaged=true;b->pieces.resize(pieces);b->cuts.resize(cuts);
                for(auto list:{&b->pieces,&b->cuts})for(auto& box:*list){if(!(in>>box.low.x>>box.low.y>>box.low.z>>box.high.x>>box.high.y>>box.high.z))return false;
                    if(!std::isfinite(box.low.x)||!std::isfinite(box.low.y)||!std::isfinite(box.low.z)||!std::isfinite(box.high.x)||!std::isfinite(box.high.y)||!std::isfinite(box.high.z)||box.low.x>=box.high.x||box.low.y>=box.high.y||box.low.z>=box.high.z)return false;}}
            else if(type=="NT"||type=="VT"){auto& state=type=="NT"?normal:built;std::string id;int hp,dead;
                if(!(in>>std::quoted(id)>>hp>>dead)||hp<0||hp>100||dead<0||dead>1)return false;
                auto t=std::find_if(state.trees.begin(),state.trees.end(),[&](const game::Tree& t){return t.id==id;});if(t==state.trees.end())return false;t->health=hp;t->destroyed=dead!=0;}
            else return false;
        }else return false;
        in>>std::ws;if(!in.eof())return false;
    }
    if(!version)return false;
    for(auto cell:newSoil)if(newCuts.count(cell))return false;
    normalState=std::move(normal);builderState=std::move(built);initialized=newInitialized;enabled=false;phase=0;outgoingSave={};preparedBaseline=false;menu=hasChest=false;
    slots=newSlots;held=newHeld;placed=std::move(newBlocks);loose=std::move(newDrops);hotbar=newSelected;
    excavation::restore(newCuts);
    scenery_edits::restore(std::move(newSceneryCuts));
    surface_work::restore(std::move(newSoil),std::move(newBrushed));
    clearInputs();
    if(initialized){game::buildings=normalState.buildings;game::trees=normalState.trees;jolt_world::refreshScenery();safeActors();}
    ++generation;return true;
}
}
