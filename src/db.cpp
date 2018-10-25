#include <config.hpp>
//
#include <functional>
//
#include "db.hpp"

static DynArr<VoxelType>    voxels;
static DynArr<EntitySprite> entities;
static SortMap<DynStr, VoxelId> voxels_lookup;

void add_voxel(VoxelType &&voxel_type) {
    auto &voxel = voxels.emplace_back(voxel_type);
    voxels_lookup[voxel.str_id] = voxels.size() - 1;
}

void db_init() {
    add_voxel({"void", "Void"              , {0, 0}, VoxelType::EMPTY, false});
    add_voxel({"stone_floor", "Stone Floor", {1, 0}, VoxelType::FLOOR, false});
    add_voxel({"stone_wall", "Stone Wall"  , {0, 1}, VoxelType::BLOCK, true});
    add_voxel({"raw_stone", "Raw Stone"    , {2, 0}, VoxelType::BLOCK, false});
    add_voxel({"dirt", "Dirt"              , {5, 1}, VoxelType::BLOCK, false});
    add_voxel({"gravel", "Gravel"          , {3, 1}, VoxelType::BLOCK, false});
    entities.push_back({{0, 0}, {2, 2}});
}

EntitySprite const& db_entity_sprite(U32 id) {
    LUX_ASSERT(id < entities.size());
    return entities[id];
}

VoxelType const& db_voxel_type(VoxelId id) {
    LUX_ASSERT(id < voxels.size());
    return voxels[id];
}

VoxelType const& db_voxel_type(SttStr const& str_id) {
    return db_voxel_type(db_voxel_id(str_id));
}

VoxelId const& db_voxel_id(SttStr const& str_id) {
    LUX_ASSERT(voxels_lookup.count(str_id) > 0);
    return voxels_lookup.at(str_id);
}
