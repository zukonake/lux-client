#include <map/voxel_type.hpp>
#include "database.hpp"

namespace data
{

Database::Database()
{
    add_voxel("void", "Void", render::TexPos(0, 0));
    add_voxel("stone_floor", "Stone Floor", render::TexPos(1, 0));
    add_voxel("stone_wall", "Stone Wall", render::TexPos(2, 0));
    add_voxel("raw_stone", "Raw Stone", render::TexPos(3, 0));
    add_voxel("dirt", "Dirt", render::TexPos(0, 1));
    add_voxel("gravel", "Gravel", render::TexPos(1, 1));
    add_voxel("grass", "Grass", render::TexPos(2, 1));
}

VoxelType const &Database::get_voxel(String const &str_id) const
{
    return voxels[get_voxel_id(str_id)];
}

VoxelId const &Database::get_voxel_id(String const &str_id) const
{
    return voxels_lookup.at(str_id);
}

}

