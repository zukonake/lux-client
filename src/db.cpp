#include <functional>
#include "db.hpp"

static DynArr<VoxelType> voxels;
static HashMap<String, VoxelId, std::hash<String>> voxels_lookup;

void add_voxel(VoxelType &&voxel_type) {
    auto &voxel = voxels.emplace_back(voxel_type);
    voxels_lookup[voxel.str_id] = voxels.size() - 1;
}

void db_init() {
    add_voxel({"void", "Void"              , {0, 0}});
    add_voxel({"stone_floor", "Stone Floor", {1, 0}});
    add_voxel({"stone_wall", "Stone Wall"  , {2, 0}});
    add_voxel({"raw_stone", "Raw Stone"    , {3, 0}});
    add_voxel({"dirt", "Dirt"              , {0, 1}});
    add_voxel({"gravel", "Gravel"          , {1, 1}});
}

VoxelType const& db_voxel_type(VoxelId id) {
    LUX_ASSERT(id < voxels.size());
    return voxels[id];
}

VoxelType const& db_voxel_type(String const& str_id) {
    return db_voxel_type(db_voxel_id(str_id));
}

VoxelId const& db_voxel_id(String const& str_id) {
    LUX_ASSERT(voxels_lookup.count(str_id) > 0);
    return voxels_lookup.at(str_id);
}
