#include "../src/builder.h"
#include "builder_test_support.h"
#include "../src/excavation.h"
#include "../src/terrain.h"
#include "../src/jolt_world.h"
#include "../src/game_internal.h"
#include "../src/savegame.h"
#include "../src/camera.h"
#include "../src/dx11_terrain.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
#include <map>

namespace {
void toggle(){bool next=!builder::active();assert(builder::requestToggle());assert(finishBuilderTransition()&&builder::active()==next);}
void settle(int ticks=120){for(int n=0;n<ticks;++n){jolt_world::moveCharacter({},false,1.0f/60);jolt_world::step(1.0f/60);}}
}
void excavationScenarios(){
    using namespace game;
    reset();assert(builder::loadCatalog());buildings.clear();trees.clear();peds.clear();props.clear();vehicles.clear();wildlife::animals.clear();
    player=previousPlayer={100,60};playerY=0;occupied=-1;jolt_world::reset();toggle();
    const builder::Cell pit{2,-1,2};const Vec2 center{100,100};
    int shovel=builder::itemIndex("iron-shovel"),soil=builder::itemIndex("soil");
    assert(builder::addItem(shovel,1));cameraYaw=PI/2;cameraPitch=-std::atan2(31.0f,40.0f);builder::update(.01f);
    assert(builder::target().source==builder::Source::Ground&&builder::target().cell==pit);
    leftMouse=true;for(int n=0;n<100&&!excavation::removed(pit);++n)builder::update(.01f);leftMouse=false;
    assert(excavation::removed(pit)&&builder::inventory()[0].durability==249);
    assert(builder::inventory()[1].item==soil&&builder::inventory()[1].count==1);
    auto mined=builder::capture();assert(!builder::mineTerrain(pit)&&builder::capture()==mined);
    assert(terrain::height(center)==-40&&terrain::baseHeight(center)==0);
    assert(!terrain::contains({100,-20,100})&&terrain::contains({100,-41,100}));
    // The real capsule falls through both former city floor and former heightfield.
    player=previousPlayer=center;playerY=3;jolt_world::teleportCharacter(player,playerY);settle();
    std::printf("pit capsule %.3f\n",playerY);assert(grounded&&std::abs(playerY+40)<1);
    float fraction=0;assert(terrain::segmentHit({100,50,100},{100,-60,100},fraction));assert(std::abs(50-110*fraction+40)<.1f);
    Vec3 impact{};assert(bulletSolidSegment({100,50,100},{100,-60,100},impact));assert(std::abs(impact.y+40)<.1f);
    camera::Pose down{{100,-9,100},{100,-100,100}};assert(std::abs(camera::traceReticle(down,100).y+40)<.1f);
    // A builder jump clears one grid block; the normal game's impulse remains unchanged.
    for(int n=0;n<80;++n){jolt_world::moveCharacter({-60,0},n==0,1.0f/60);jolt_world::step(1.0f/60);}
    assert(player.x<79&&std::abs(playerY)<1);
    player=previousPlayer=center;playerY=-40;jolt_world::teleportCharacter(player,playerY);settle();
    // Dig down and horizontally beneath a retained roof, including a 200-unit patch boundary.
    for(builder::Cell c:{builder::Cell{2,-2,2},{3,-2,2},{4,-2,2},{5,-2,2},{6,-2,2}})assert(builder::mineTerrain(c));
    settle();assert(grounded&&std::abs(playerY+80)<1);
    for(int n=0;n<130;++n){jolt_world::moveCharacter({60,0},false,1.0f/60);jolt_world::step(1.0f/60);}
    std::printf("tunnel capsule %.3f %.3f\n",player.x,playerY);
    assert(player.x>225&&player.x<270&&grounded&&std::abs(playerY+80)<1);
    assert(terrain::height(player)==0&&excavation::floorBelow({player.x,playerY+.1f,player.z})==-80);
    float highest=playerY;for(int n=0;n<40;++n){jolt_world::moveCharacter({},true,1.0f/60);jolt_world::step(1.0f/60);highest=std::max(highest,playerY);}
    assert(highest<-74);settle(); // A heightfield lowered to a pit cannot supply this ceiling.
    assert(terrain::segmentHit({240,-60,100},{240,-20,100},fraction));assert(std::abs(-60+40*fraction+40)<.1f);
    assert(bulletSolidSegment({240,-60,100},{330,-60,100},impact));assert(std::abs(impact.x-280)<.1f);
    Vec3 anchor{};assert(jolt_world::staticAnchor({240,-60,100},{1,0,0},100,anchor));assert(std::abs(anchor.x-280)<.1f);
    int granite=builder::itemIndex("granite");assert(builder::place({4,-2,2},granite,false));
    for(int n=0;n<100;++n){jolt_world::moveCharacter({-60,0},false,1.0f/60);jolt_world::step(1.0f/60);}
    assert(player.x>=209&&std::abs(playerY+80)<1);assert(builder::mineBlock({4,-2,2}));
    // City pavement triangles above the cut must be removed, with original UVs retained elsewhere.
    std::vector<dx11::Vertex> staticGroups[dx11::MATERIAL_GROUPS];dx11::buildStaticScene(staticGroups);
    for(const auto& group:staticGroups)for(std::size_t i=0;i<group.size();i+=3){
        auto a=group[i],b=group[i+1],c=group[i+2];float x=(a.x+b.x+c.x)/3,z=(a.z+b.z+c.z)/3,y=(a.y+b.y+c.y)/3;
        assert(!(x>80.01f&&x<119.99f&&z>80.01f&&z<119.99f&&y>-.1f&&y<.5f));
    }
    auto faces=excavation::faces({0,0});bool floor=false,ceiling=false,wall=false;
    for(const auto& face:faces){assert(face.item>=0);auto v=face.vertices[0];floor|=v.ny>.5f;ceiling|=v.ny<-.5f;wall|=std::abs(v.nx)>.5f;}
    assert(floor&&ceiling&&wall);
    auto snapshot=builder::capture();assert(!builder::restore(snapshot+"Record99999=E 2 -20 2\n"));assert(builder::capture()==snapshot);
    assert(savegame::save());assert(savegame::load());assert(!builder::active()&&terrain::height(center)==0&&playerY>=0);
    toggle();assert(terrain::height(center)==-80&&excavation::removed({6,-2,2}));
    player=previousPlayer={240,100};playerY=-80;jolt_world::teleportCharacter(player,playerY);settle();assert(std::abs(playerY+80)<1);
    toggle();assert(!builder::active()&&terrain::height(center)==0&&playerY>=0);
    assert(bulletSolidSegment({100,-20,100},{100,-10,100},impact));assert(!builder::mineTerrain(pit));
    toggle();assert(!bulletSolidSegment({100,-20,100},{100,-10,100},impact)&&terrain::height(center)==-80);
    // Deterministic formations and deposits cover all requested rocks plus the ore progression.
    std::set<std::string> deposits;
    for(int x=20;x<120;++x)for(int z=20;z<120;++z)for(int y=-9;y<=-2;++y){int item=excavation::material({x,y,z});assert(item>=0);deposits.insert(builder::items()[item].id);}
    for(const auto& item:builder::items())if(item.craftGroup=="stone-material"&&item.id!="brick"&&item.id!="concrete")assert(deposits.count(item.id));
    for(const char* ore:{"coal-ore","iron-ore","gold-ore","diamond-ore"})assert(deposits.count(ore));
    // A surface pedestrian must not obstruct someone walking beneath its retained roof.
    peds.clear();props.clear();vehicles.clear();wildlife::animals.clear();birds::flock.clear();
    Ped overhead{};overhead.id="excavation-ped";overhead.p=overhead.target={260,100};peds.push_back(overhead);jolt_world::reset();
    player=previousPlayer={240,100};playerY=-80;jolt_world::teleportCharacter(player,playerY);
    for(int n=0;n<20;++n){jolt_world::moveCharacter({60,0},false,1.0f/60);jolt_world::step(1.0f/60);}
    assert(player.x>255&&std::abs(playerY+80)<1&&std::abs(jolt_world::pedHeight(0))<1);
    jolt_world::teleportPed(0,{220,100},-80);jolt_world::movePed(0,{},1.0f/60);
    assert(std::abs(jolt_world::pedHeight(0)+80)<1);
    camera::Pose underground{{240,-60,100},{100,-60,100}};
    assert(std::abs(camera::traceReticle(underground,140).x-220)<.1f);
    peds[0].target=peds[0].p;peds[0].speed=0;
    assert(!bulletSolidSegment({240,-60,100},{220,-60,100},impact));
    bullets.push_back({{240,-60,100},{-1000,0,0},1,10,0});update(.02f);
    assert(peds[0].health<100&&peds[0].alive);
    peds[0].alive=false;jolt_world::spawnRagdoll(peds[0],{});peds[0].alive=true;jolt_world::step(1.0f/60);
    assert(!ragdollParts.empty());for(const auto& part:ragdollParts)assert(std::abs(part.origin.y+80)<1);
    jolt_world::clearRagdolls();
    toggle();assert(jolt_world::pedHeight(0)>=0);toggle();peds.clear();
    // An edited mountain cell retains the sloped baseline around its hole.
    player=previousPlayer={13900,1820};float mountain=terrain::baseHeight(player);playerY=mountain;
    jolt_world::teleportCharacter(player,playerY);auto mountainCell=builder::cellAt({player.x,mountain-.01f,player.z});
    assert(builder::mineTerrain(mountainCell));settle();
    std::printf("mountain pit capsule %.3f vs %.3f\n",playerY,mountainCell.y*40.0f);
    assert(grounded&&std::abs(playerY-mountainCell.y*40)<1);
    toggle();assert(terrain::height(player)==terrain::baseHeight(player)&&playerY>=mountain-1);
    reset();assert(excavation::cells().empty());buildings.clear();trees.clear();peds.clear();props.clear();vehicles.clear();wildlife::animals.clear();
    player=previousPlayer={100,60};playerY=0;occupied=-1;jolt_world::reset();toggle();
    assert(builder::addItem(builder::itemIndex("diamond-pickaxe"),1));assert(builder::addItem(builder::itemIndex("stone-pickaxe"),1));
    // Find actual dry, level deposits for each rock and ore, then open test access shafts.
    // Only the access shaft uses the low-level fixture helper; target mining uses held LMB.
    std::set<std::string> required;
    for(const auto& item:builder::items())if(item.craftGroup=="stone-material"&&item.id!="brick"&&item.id!="concrete")required.insert(item.id);
    for(const char* id:{"coal-ore","iron-ore","gold-ore","diamond-ore"})required.insert(id);
    std::map<std::string,builder::Cell> targets;std::set<std::pair<int,int>> columns;
    for(int x=20;x<130&&targets.size()<required.size();++x)for(int z=20;z<130&&targets.size()<required.size();++z){
        bool flat=true;for(float px:{x*40.0f+.01f,x*40.0f+39.99f})for(float pz:{z*40.0f+.01f,z*40.0f+39.99f})flat&=std::abs(terrain::baseHeight({px,pz}))<.001f;
        if(!flat)continue;
        for(int y=-8;y<=-2;++y){builder::Cell cell{x,y,z};auto id=builder::items()[excavation::material(cell)].id;
            if(required.count(id)&&!targets.count(id)&&!columns.count({x,z})&&excavation::validCell(cell)){
                targets[id]=cell;columns.insert({x,z});break;
            }
        }
    }
    assert(targets.size()==24);
    auto quantity=[](int item){int count=0;for(const auto& stack:builder::inventory())if(stack.item==item)count+=stack.count;return count;};
    int successful=0;
    for(const auto& entry:targets){auto cell=entry.second;
        for(int y=-1;y>cell.y;--y)assert(excavation::cut({cell.x,y,cell.z}));
        player=previousPlayer={cell.x*40.0f+20,cell.z*40.0f+20};playerY=(cell.y+1)*40.0f;
        jolt_world::teleportCharacter(player,playerY);cameraPitch=-PI/2;cameraYaw=0;
        builder::handleKey('1');builder::update(.01f);assert(builder::target().source==builder::Source::Ground&&builder::target().cell==cell);
        if(entry.first=="diamond-ore"){
            builder::handleKey('2');auto before=builder::capture();leftMouse=true;builder::update(2);leftMouse=false;
            assert(!excavation::removed(cell)&&builder::capture()==before);builder::handleKey('1');
        }
        int resource=builder::itemIndex(builder::items()[builder::target().item].harvestDrop),before=quantity(resource);
        leftMouse=true;for(int n=0;n<150&&!excavation::removed(cell);++n)builder::update(.01f);leftMouse=false;
        assert(excavation::removed(cell)&&quantity(resource)==before+1);++successful;
    }
    assert(builder::inventory()[0].durability==1562-successful);
    auto depositSave=builder::capture();assert(savegame::save());assert(savegame::load());assert(builder::capture()==depositSave);toggle();
    // A full Jolt vehicle falls into a sufficiently wide pit and is recovered on F5 exit.
    builder::reset();buildings.clear();trees.clear();peds.clear();props.clear();vehicles.clear();wildlife::animals.clear();birds::flock.clear();
    player=previousPlayer={140,60};playerY=0;Vehicle car{};car.id="excavation-car";car.kind=Kind::Car;car.p={140,180};vehicles.push_back(car);jolt_world::reset();toggle();
    for(int x=2;x<=4;++x)for(int z=2;z<=6;++z)for(int y=-2;y<0;++y)assert(builder::mineTerrain({x,y,z}));
    for(int n=0;n<240;++n){jolt_world::driveVehicle(0,0,0,1.0f/60,true);jolt_world::step(1.0f/60);}
    assert(std::abs(vehicles[0].rideHeight+80)<12&&jolt_world::wheelContactCount(0)>=2);
    toggle();assert(vehicles[0].rideHeight>=-.1f);
    for(int n=0;n<120;++n){jolt_world::driveVehicle(0,0,0,1.0f/60,true);jolt_world::step(1.0f/60);}
    assert(std::abs(vehicles[0].rideHeight)<12&&jolt_world::wheelContactCount(0)>=2);
    toggle();for(int n=0;n<240;++n){jolt_world::driveVehicle(0,0,0,1.0f/60,true);jolt_world::step(1.0f/60);}
    assert(std::abs(vehicles[0].rideHeight+80)<12&&jolt_world::wheelContactCount(0)>=2);
    reset();
    std::puts("excavation pit, roofed tunnel, Jolt capsule, camera/projectile/grapple, save and F5 scenarios passed");
}
