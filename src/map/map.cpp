#include <lux/util/log.hpp>
//
#include <data/database.hpp>
#include <data/obj.hpp>
#include <map/tile/tile_type.hpp>
#include "map.hpp"

#include <iostream> //TODO

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
    Chunk const *chunk_ptr   = this->operator[](chunk_pos);
    if(chunk_ptr != nullptr) return &chunk_ptr->tiles[chunk_index];
    else return nullptr;
}

Chunk const *Map::operator[](chunk::Pos const &pos) const
{
    if(chunks.count(pos) == 0)
    {
        return nullptr;
    }
    else return &chunks.at(pos);
}

void Map::add_chunk(serial::ChunkData const &new_chunk)
{
    if(chunks.count(new_chunk.pos) > 0) return;
    util::log("MAP", util::TRACE, "adding new chunk %zd, %zd, %zd",
        new_chunk.pos.x,
        new_chunk.pos.y,
        new_chunk.pos.z);
    chunks[new_chunk.pos] = { };
    //TODO optimize
    auto &chunk = chunks[new_chunk.pos];
    chunk.vertices.reserve(chunk::TILE_SIZE * block_model.vertices.size());
    chunk.indices.reserve(chunk::TILE_SIZE * block_model.indices.size());
    glGenBuffers(1, &chunk.vbo_id);
    glGenBuffers(1, &chunk.ebo_id);
    render::Index index_offset = 0;
    for(SizeT i = 0; i < chunk::TILE_SIZE; ++i)
    {
        chunk.tiles[i].type = db.tile_types.at(new_chunk.tiles[i].db_hash);
        if(chunk.tiles[i].type->id != "void")
        {
            map::Pos map_pos = chunk::to_map_pos(new_chunk.pos, i);
            for(auto const &vertex : block_model.vertices)
            {
                chunk.vertices.emplace_back(vertex.pos + (glm::vec3)map_pos,
                    vertex.col);
            }
            for(auto const &index : block_model.indices)
            {
                chunk.indices.emplace_back(index + index_offset);
            }
            index_offset += block_model.vertices.size();
        }
    }
    chunk.vertices.shrink_to_fit();
    chunk.indices.shrink_to_fit();
    glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo_id);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(render::Vertex) * chunk.vertices.size(),
                 chunk.vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.ebo_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(render::Index) * chunk.indices.size(),
                 chunk.indices.data(),
                 GL_STATIC_DRAW);
}

}
