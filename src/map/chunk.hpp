#pragma once

#include <lux/alias/array.hpp>
#include <lux/common/chunk.hpp>
//
#include <map/tile/tile.hpp>

namespace map
{

struct Chunk
{
    Array<Tile, chunk::TILE_SIZE> tiles;
};

}
