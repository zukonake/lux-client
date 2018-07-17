#pragma once

#include <set>
//
#include <lux/alias/hash_map.hpp>
#include <lux/common/chunk.hpp>
#include <lux/common/map.hpp>
#include <lux/net/array.hpp>
#include <lux/net/server/chunk_data.hpp>
//
#include <map/chunk.hpp>

namespace map
{

struct Tile;

class Map
{
    public:
    Tile const &operator[](MapPos const &pos) const;

    void add_chunks(net::Array<net::ChunkData> const &new_chunks);
    std::set<ChunkPos> const &get_requests() const;
    private:
    Chunk const &get_chunk(ChunkPos const &pos) const;

    HashMap<ChunkPos, Chunk> chunks;
    std::set<ChunkPos> chunk_requests;
}

}
