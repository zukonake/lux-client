#pragma once

#include <lux/consts.hpp>
//
#include <map/tile/tile.hpp>

namespace map
{

struct Chunk
{
    Tile tiles[consts::CHUNK_TILE_SIZE];
};

}
