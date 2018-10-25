#pragma once

#include <config.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>

struct VoxelType {
    DynStr str_id;
    DynStr name;
    Vec2U tex_pos;
    enum Shape {
        EMPTY,
        FLOOR,
        BLOCK,
        HALF_BLOCK,
    } shape;
    bool connected_tex;
};

struct EntitySprite {
    Vec2<U8> pos;
    Vec2<U8> sz;
};

//@CONSIDER voxel -> vox
void db_init();
EntitySprite const& db_entity_sprite(U32 id);
VoxelType    const& db_voxel_type(VoxelId id);
VoxelType    const& db_voxel_type(SttStr const& str_id);
VoxelId      const& db_voxel_id(SttStr const& str_id);
