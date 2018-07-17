#pragma once

#include <lux/alias/hash.hpp>
#include <lux/alias/hash_map.hpp>

namespace map
{
    struct TileType;
}

namespace data
{

struct Database
{
    HashMap<Hash, map::TileType const *> tile_types;
};

}
