#pragma once

#include <lux/alias/hash.hpp>
#include <lux/alias/hash_map.hpp>

struct TileType;

namespace data
{

struct Database
{
    HashMap<Hash, TileType const *> tile_types;
};

}
