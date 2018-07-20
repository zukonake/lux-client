#pragma once

#include <set>
//
#include <lux/alias/hash_map.hpp>
#include <lux/alias/set.hpp>
#include <lux/common/chunk.hpp>
#include <lux/common/map.hpp>
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

    Tile const *operator[](map::Pos const &pos) const;

    void add_chunk(net::ChunkData const &new_chunk);
    private:
    Chunk const *get_chunk(chunk::Pos const &pos) const;

    HashMap<chunk::Pos, Chunk> chunks;
    data::Database const &db;
};

}
