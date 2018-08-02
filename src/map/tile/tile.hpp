#pragma once

#include <map/tile/tile_type.hpp>

namespace map
{

struct Tile
{
    Tile(TileType const *type) : type(type) { }
    TileType const *type;
};

}
