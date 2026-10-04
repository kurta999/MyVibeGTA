#pragma once
#include "dx11_assets.h"
#include "game.h"
namespace dx11 {
// World-space subtraction retains the source facade's UVs and PBR sections.
Mesh clipBuildingMesh(const ModelInstance& instance,const std::vector<game::BuildingPiece>& cuts);
Mesh buildingInteriorFaces(const game::Building& building);
}
