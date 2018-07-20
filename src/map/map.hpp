#pragma once

#include <set>
//
#include <lux/alias/hash_map.hpp>
#include <lux/alias/set.hpp>
#include <lux/common/chunk.hpp>
#include <lux/common/map.hpp>
#include <lux/net/array.hpp>
#include <lux/net/server/chunk_data.hpp>
//
#include <map/chunk.hpp>

namespace data
{
    struct Database;
}

namespace map
{

struct Tile;

class Map
{
    public:
    Map(data::Database const &db);

    void add_chunk(net::ChunkData const &new_chunk);

    Chunk const *get_chunk(ChunkPos const &pos) const;
    private:

    HashMap<ChunkPos, Chunk> chunks;
    data::Database const &db;
};

}
