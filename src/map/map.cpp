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

Tile const *Map::operator[](map::Pos const &pos) const
{
    chunk::Pos   chunk_pos   = chunk::to_pos(pos);
    chunk::Index chunk_index = chunk::to_index(pos);
    Chunk const *chunk_ptr   = get_chunk(chunk_pos);
    if(chunk_ptr != nullptr) return &chunk_ptr->tiles[chunk_index];
    else return nullptr;
}

void Map::add_chunk(net::ChunkData const &new_chunk)
{
    util::log("MAP", util::TRACE, "adding new chunk %zd, %zd, %zd",
        new_chunk.pos.x,
        new_chunk.pos.y,
        new_chunk.pos.z);
    chunks[new_chunk.pos] = { };
    //TODO optimize
    for(SizeT i = 0; i < chunk::TILE_SIZE; ++i)
    {
        chunks[new_chunk.pos].tiles[i].type = db.tile_types.at(new_chunk.tiles[i].db_hash);
    }
}

Chunk const *Map::get_chunk(chunk::Pos const &pos) const
{
    if(chunks.count(pos) == 0)
    {
        return nullptr;
    }
    else return &chunks.at(pos);
}

}
