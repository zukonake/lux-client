#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/hash_map.hpp>
#include <lux/common/map.hpp>
//
#include <map/chunk.hpp>

namespace data { class Database; }
namespace net::server { struct Chunk; }
namespace map { struct Tile; }

class Map
{
    public:
    Map(data::Database const &db);

    map::Tile  const *get_tile(MapPos const &pos) const;
    map::Chunk const *get_chunk(ChkPos const &pos) const;

    void add_chunk(net::server::Chunk const &new_chunk);
    void try_mesh(ChkPos const &pos);
    private:
    void build_mesh(map::Chunk &chunk, ChkPos const &pos);

    HashMap<ChkPos, map::Chunk> chunks;
    data::Database const &db;
};
