#pragma once

#include <lux/alias/array.hpp>
#include <lux/common/map.hpp>
//
#include <render/mesh.hpp>

struct Chunk
{
    Array<VoxelId, CHK_VOLUME> voxels;

    render::Mesh mesh; //TODO ptr?
    bool is_mesh_generated;
};
