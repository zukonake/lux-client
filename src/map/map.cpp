#include <lux/util/log.hpp>
//
#include <data/database.hpp>
#include <map/tile/tile_type.hpp>
#include "map.hpp"

namespace map
{

Map::Map(data::Database const &db) :
    db(db)
{

}

void Map::add_chunk(net::ChunkData const &new_chunk)
{
    util::log("MAP", util::TRACE, "adding new chunk %zd, %zd, %zd",
        new_chunk.pos.x,
        new_chunk.pos.y,
        new_chunk.pos.z);
    chunks[new_chunk.pos] = { };
    //TODO optimize
    for(SizeT i = 0; i < consts::CHUNK_TILE_SIZE; ++i)
    {
        chunks[new_chunk.pos].tiles[i].type = db.tile_types.at(new_chunk.tiles[i].db_hash);
    }
}

Chunk const *Map::get_chunk(ChunkPos const &pos) const
{
    if(chunks.count(pos) == 0)
    {
        return nullptr;
    }
    else return &chunks.at(pos);
}

}
