#pragma once

#include <lux/alias/scalar.hpp>
#include <lux/alias/vector.hpp>
#include <lux/alias/hash_map.hpp>
#include <lux/alias/string.hpp>
#include <lux/world/map.hpp>
//
#include <map/voxel_type.hpp>

namespace data
{

class Database //TODO code repetition from server
{
public:
    Database();

    template<typename... Args>
    void add_voxel(Args &&...args);

    VoxelType const &get_voxel(String const &str_id) const;
    VoxelId   const &get_voxel_id(String const &str_id) const;

    Vector<VoxelType> voxels;
    HashMap<String, VoxelId> voxels_lookup;
};

template<typename... Args>
void Database::add_voxel(Args &&...args)
{
    auto &voxel = voxels.emplace_back(args...);
    voxels_lookup[voxel.str_id] = voxels.size() - 1;
}

}
