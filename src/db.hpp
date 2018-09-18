#pragma once

#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>

struct VoxelType {
    String str_id;
    String name;
    Vec2U tex_pos;
    enum Shape {
        EMPTY,
        FLOOR,
        BLOCK,
        HALF_BLOCK,
    } shape;
};

//@CONSIDER voxel -> vox
void db_init();
VoxelType const& db_voxel_type(VoxelId id);
VoxelType const& db_voxel_type(String const& str_id);
VoxelId   const& db_voxel_id(String const& str_id);
