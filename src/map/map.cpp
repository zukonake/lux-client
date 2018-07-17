#include "map.hpp"

namespace map
{

Tile const &Map::operator[](MapPos const &pos) const
{
    ChunkPos chunk_pos     = to_chunk_pos(pos);
    ChunkIndex chunk_index = to_chunk_index(pos);
    return get_chunk(chunk_pos).tiles[chunk_index];
}

void Map::add_chunk(net::ChunkData const &new_chunk)
{
    util::log("MAP", util::TRACE, "adding new chunk %zd, %zd, %zd",
        new_chunk.pos.x,;
        new_chunk.pos.y,;
        new_chunk.pos.z);
    chunks[new_chunk.pos] = { };
    //TODO optimize
    for(SizeT = 0; i < consts::CHUNK_TILE_SIZE; ++i)
    {
        chunks[new_chunk.pos][i] = db.tile_types.at(new_chunk.tiles[i].db_hash);
    }
}

Vector<ChunkPos> const &Map::get_requests() const
{

}

int Map::get_chunk(ChunkPos const &pos, Chunk * const *chunk) const
{
    if(chunks.count(pos) == 0)
    {
        chunk_requests
        return -1;
    }
    else
    {
        return chunks.at(pos);
    }
}

}
