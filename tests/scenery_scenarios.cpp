#include "../src/scenery_edits.h"
#include "builder_test_support.h"
#include "../src/builder_feedback.h"
#include "../src/game_internal.h"
#include "../src/jolt_world.h"
#include "../src/camera.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#include "../src/savegame.h"
#include "../src/terrain.h"
#include "../src/regions.h"
#include "../src/traversal.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <sstream>
#include <iomanip>

namespace {
void toggle(){bool next=!builder::active();assert(builder::requestToggle());bool finished=finishBuilderTransition();
    if(!finished||builder::active()!=next)std::printf("scenery transition failed: finished %d, next %d, active %d, progress %.2f, label %s, message %s\n",finished,next,builder::active(),builder::transitionProgress(),builder::transitionLabel(),game::message.c_str());
    assert(finished&&builder::active()==next);}
bool targetObject(const scenery_edits::Object& object,const char* material,builder::Target& result){
    for(int y=object.kind==scenery_edits::Kind::Rock?int(object.size.y*.4f):3;y<int(object.size.y);y+=4)for(int z=-40;z<=40;z+=4){
        builder::Target hit;hit.distance=200;auto start=object.position+game::Vec3{-100,float(y),float(z)};
        if(scenery_edits::trace(start,{1,0,0},200,hit)&&hit.objectId==object.id&&(material==nullptr||builder::items()[hit.item].id==material)){result=hit;return true;}
    }return false;
}
}
void sceneryScenarios(){
    using namespace game;
    dx11::loadMeshes(L"assets/models/baked");reset();assert(builder::loadCatalog());
    buildings.clear();trees.clear();peds.clear();vehicles.clear();props.clear();wildlife::animals.clear();birds::flock.clear();
    Tree tree{};tree.id="scenery-test-tree";tree.p={100,100};tree.variant=0;tree.scale=2;trees.push_back(tree);
    player=previousPlayer={30,100};playerY=0;occupied=-1;cameraYaw=cameraPitch=0;jolt_world::reset();toggle();
    scenery_edits::Object object;assert(scenery_edits::treeObject(0,object));builder::Target trunk;
    assert(targetObject(object,"log",trunk));assert(trunk.source==builder::Source::Tree);
    traversal::trees={{"climb-fixture",tree.id,{100,100},68,0,false}};player=previousPlayer={65,100};assert(traversal::startTree(0));traversal::detach();
    std::printf("tree target %.2f %.2f %.2f cell %d %d %d\n",trunk.point.x,trunk.point.y,trunk.point.z,trunk.cell.x,trunk.cell.y,trunk.cell.z);
    int axe=builder::itemIndex("iron-axe"),log=builder::itemIndex("log");assert(builder::addItem(axe,1));
    auto origin=trunk.point+trunk.normal*70;player=previousPlayer={origin.x,origin.z};playerY=std::max(0.0f,origin.y-31);
    auto direction=norm(trunk.point-Vec3{player.x,playerY+31,player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
    builder::update(.01f);assert(builder::target().objectId==object.id&&builder::target().item==log);
    auto minedCell=builder::target().cell;leftMouse=true;builder::update(.08f);assert(!builder_feedback::cracks().empty());
    auto contactNormal=builder::target().normal;
    for(const auto& line:builder_feedback::cracks())for(auto point:{line.a,line.b}){builder::Target hit;hit.distance=24;
        bool traced=scenery_edits::trace(point+contactNormal*12,contactNormal*-1,24,hit);
        assert(traced&&hit.objectId==object.id&&hit.cell==minedCell);
        assert(len(hit.point-point)<.5f);}
    for(int n=0;n<300&&!scenery_edits::removed(object.id,minedCell);++n)builder::update(.01f);leftMouse=false;
    assert(scenery_edits::removed(object.id,minedCell)&&!trees[0].destroyed);
    assert(builder::inventory()[0].durability==builder::items()[axe].durability-1&&builder::inventory()[1].item==log&&builder::inventory()[1].count==1);
    auto saved=builder::capture();assert(!builder::mineScenery(trunk)&&builder::capture()==saved);
    assert(!builder::restore(saved+"Record99999=S \"unknown-object\" 2 0 2\n")&&builder::capture()==saved);
    assert(!builder::restore(saved+"Record99999=S \"tree:scenery-test-tree\" 20 0 20\n")&&builder::capture()==saved);
    std::ostringstream duplicate;duplicate<<"Record99999=S "<<std::quoted(object.id)<<' '<<minedCell.x<<' '<<minedCell.y<<' '<<minedCell.z<<'\n';
    assert(!builder::restore(saved+duplicate.str())&&builder::capture()==saved);
    std::vector<dx11::ModelInstance> instances;assert(scenery_edits::appendIfEdited(object.id,instances)&&!instances.empty());
    const auto* original=dx11::mesh(object.model);bool cap=false;
    for(const auto& instance:instances){assert(instance.source!=original);cap|=instance.source->textureFile==dx11::mesh("builder/log")->textureFile;
        for(const auto& v:instance.source->vertices){auto low=builder::cellLow(minedCell);assert(!(v.x>low.x+.01f&&v.x<low.x+39.99f&&v.y>low.y+.01f&&v.y<low.y+39.99f&&v.z>low.z+.01f&&v.z<low.z+39.99f));}}
    assert(cap);auto boxes=scenery_edits::trunkPieces(0);assert(!boxes.empty());
    player=previousPlayer={65,100};assert(!traversal::startTree(0));
    player=previousPlayer={30,100};playerY=0;assert(builder::canPlace(minedCell,builder::itemIndex("granite")));
    assert(!builder::canPlace({2,1,2},builder::itemIndex("granite")));
    auto low=builder::cellLow(minedCell);float y=low.y+20;
    jolt_world::step(1.0f/60);Vec3 anchor{};
    if(y<40){assert(!jolt_world::staticAnchor({60,y,100},{1,0,0},80,anchor));assert(jolt_world::staticAnchor({60,45,100},{1,0,0},80,anchor));
        float fraction=0;assert(!scenery_edits::segment({60,y,100},{140,y,100},fraction));
        camera::Pose gap{{60,y,100},{140,y,100}};assert(std::abs(camera::traceReticle(gap,80).x-140)<.1f);
        Bullet shot{};shot.p={60,y,100};shot.v={400,0,0};shot.life=1;shot.range=200;shot.damage=20;bullets={shot};
        int health=trees[0].health;update(.2f);assert(trees[0].health==health);
        builder::Target surviving;assert(targetObject(object,"log",surviving));shot.p=surviving.point+surviving.normal*2;shot.v=surviving.normal*-400;
        bullets={shot};update(.05f);assert(trees[0].health<health);bullets.clear();
    }
    toggle();assert(!scenery_edits::edited(object.id)&&!scenery_edits::appendIfEdited(object.id,instances));
    assert(jolt_world::staticAnchor({60,20,100},{1,0,0},80,anchor));assert(!trees[0].destroyed);
    assert(trees[0].health==100);
    toggle();assert(scenery_edits::removed(object.id,minedCell));
    assert(builder::restore(saved));assert(!builder::active());toggle();assert(scenery_edits::removed(object.id,minedCell));
    // Existing scanned outcrops are real model surfaces, with caps and builder-only collision.
    scenery_edits::Object rock;bool found=false;
    for(int z=0;z<42&&!found;++z)for(int x=0;x<42&&!found;++x)if(scenery_edits::outcropObject(x,z,rock)&&rock.size.x>80)found=true;
    assert(found);assert(!scenery_edits::find("outcrop:0"+rock.id.substr(8),object));
    player=previousPlayer={rock.position.x-150,rock.position.z};playerY=terrain::baseHeight(player);jolt_world::teleportCharacter(player,playerY);
    builder::Target surface;assert(targetObject(rock,nullptr,surface));assert(surface.source==builder::Source::Scenery);
    auto before=scenery_edits::collisionTriangles(rock);assert(!before.empty());assert(builder::mineScenery(surface));
    instances.clear();assert(scenery_edits::appendIfEdited(rock.id,instances));assert(!instances.empty());
    auto after=scenery_edits::collisionTriangles(rock);assert(!after.empty());assert(scenery_edits::removed(rock.id,surface.cell));
    builder::Target remaining;assert(targetObject(rock,nullptr,remaining));jolt_world::step(1.0f/60);
    assert(jolt_world::staticAnchor(remaining.point+remaining.normal*2,remaining.normal*-1,4,anchor));
    assert(len(anchor-remaining.point)<.2f);
    Vec3 impact{};assert(bulletSolidSegment(remaining.point+remaining.normal*2,remaining.point-remaining.normal*2,impact));assert(len(impact-remaining.point)<.2f);
    toggle();assert(!scenery_edits::edited(rock.id));
    assert(!jolt_world::staticAnchor(remaining.point+remaining.normal*2,remaining.normal*-1,4,anchor));
    toggle();assert(scenery_edits::removed(rock.id,surface.cell));
    assert(jolt_world::staticAnchor(remaining.point+remaining.normal*2,remaining.normal*-1,4,anchor));
    // Builder actors can be 1,200 units from the player. A small scanned rock
    // near that boundary needs its real mesh collider, beyond the old 1,100
    // unit scenery radius (even after accounting for the rock's small width).
    scenery_edits::Object edgeRock;builder::Target edgeSurface;found=false;
    for(int z=0;z<42&&!found;++z)for(int x=0;x<42&&!found;++x)
        if(scenery_edits::outcropObject(x,z,edgeRock)&&edgeRock.size.x<50&&targetObject(edgeRock,nullptr,edgeSurface))found=true;
    assert(found);player=previousPlayer={edgeRock.position.x-1180,edgeRock.position.z};playerY=terrain::baseHeight(player);
    jolt_world::teleportCharacter(player,playerY);
    assert(jolt_world::staticAnchor(edgeSurface.point+edgeSurface.normal*2,edgeSurface.normal*-1,4,anchor));
    assert(len(anchor-edgeSurface.point)<.2f);
    std::puts("builder streaming: small scanned rock retains real mesh collision at the actor-area edge");
    // Harvest a generated bush by its textured triangles rather than its bounding box.
    builder::Target leaf;scenery_edits::Object bush;found=false;
    for(const auto& prop:regions::decorations())if(prop.modelId.rfind("bush_",0)==0){
        assert(scenery_edits::find("decoration:"+prop.id,bush));player=previousPlayer={bush.position.x-100,bush.position.z};playerY=terrain::baseHeight(player);
        if(targetObject(bush,"leaves",leaf)){found=true;break;}}
    assert(found);int shears=builder::itemIndex("shears");assert(builder::addItem(shears,1));int shearsSlot=-1;
    for(int n=0;n<9;++n)if(builder::inventory()[n].item==shears)shearsSlot=n;assert(shearsSlot>=0);builder::handleKey('1'+shearsSlot);
    player=previousPlayer={bush.position.x-60,leaf.point.z};playerY=bush.position.y;jolt_world::teleportCharacter(player,playerY);
    direction=norm(leaf.point-Vec3{player.x,playerY+31,player.z});cameraYaw=std::atan2(direction.z,direction.x);cameraPitch=std::asin(direction.y);
    builder::update(.01f);assert(builder::target().objectId==bush.id&&builder::target().item==builder::itemIndex("leaves"));leaf=builder::target();
    auto count=scenery_edits::records().size();leftMouse=true;for(int n=0;n<200&&!scenery_edits::removed(bush.id,leaf.cell);++n)builder::update(.01f);leftMouse=false;
    assert(scenery_edits::records().size()==count+1&&scenery_edits::removed(bush.id,leaf.cell));
    assert(builder::inventory()[shearsSlot].durability==builder::items()[shears].durability-1);
    assert(!builder::mineScenery(leaf));instances.clear();assert(scenery_edits::appendIfEdited(bush.id,instances));
    saved=builder::capture();assert(builder::restore(saved));toggle();assert(scenery_edits::removed(bush.id,leaf.cell));
    // Save/reload uses generated object IDs, so reconstruction can find the same source object.
    toggle();reset();assert(builder::loadCatalog());toggle();assert(scenery_edits::find(bush.id,bush));assert(targetObject(bush,"leaves",leaf));
    assert(builder::addItem(shears,1,73));assert(builder::mineScenery(leaf));assert(savegame::save());assert(savegame::load());
    assert(!builder::active()&&!scenery_edits::edited(bush.id));assert(builder::inventory()[0].item==shears&&builder::inventory()[0].durability==73);
    toggle();assert(scenery_edits::removed(bush.id,leaf.cell));toggle();
    reset();assert(savegame::save());jolt_world::shutdown();std::puts("scenery scenarios passed");
}
