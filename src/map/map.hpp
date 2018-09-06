#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/pos_map.hpp>
#include <lux/world/map.hpp>
//
#include <map/chunk.hpp>

namespace data { class Database; }
namespace net::server { struct Chunk; }
struct Voxel;

class Map
{
    public:
    Map(data::Database const &db);

    VoxelId const *get_voxel(MapPos const &pos) const;
    Chunk   const *get_chunk(ChkPos const &pos) const;

    void add_chunk(net::server::Chunk const &new_chunk);
    void try_mesh(ChkPos const &pos);
    private:
    void build_mesh(Chunk &chunk, ChkPos const &pos);

    PosMap<ChkPos, Chunk> chunks;
    data::Database const &db;
};
