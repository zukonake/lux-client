#pragma once

#include <lux/consts.hpp>

namespace map
{

struct Chunk
{
    Tile tiles[consts::CHUNK_TILE_SIZE];
};

}
