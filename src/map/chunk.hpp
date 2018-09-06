#pragma once

#include <lux/alias/array.hpp>
#include <lux/world/map.hpp>
//
#include <render/mesh.hpp>

struct Chunk
{
    Array<VoxelId , CHK_VOLUME> voxels;
    Array<LightLvl, CHK_VOLUME> light_lvls;

    render::Mesh mesh; //TODO ptr?
    bool is_mesh_generated;
};
