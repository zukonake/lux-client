#include <glm/glm.hpp>
//
#include <lux/alias/set.hpp>
#include <lux/util/log.hpp>
#include <lux/net/server/chunk.hpp>
//
#include <data/database.hpp>
#include <data/obj.hpp>
#include <map/tile/tile_type.hpp>
#include "map.hpp"

Map::Map(data::Database const &db) :
    db(db)
{

}

map::Tile const *Map::operator[](map::Pos const &pos) const
{
    chunk::Pos   chunk_pos   = chunk::to_pos(pos);
    chunk::Index chunk_index = chunk::to_index(pos);
    map::Chunk const *chunk_ptr   = this->operator[](chunk_pos);
    if(chunk_ptr != nullptr) return &chunk_ptr->tiles[chunk_index];
    else return nullptr;
}

map::Chunk const *Map::operator[](chunk::Pos const &pos) const
{
    if(chunks.count(pos) == 0)
    {
        return nullptr;
    }
    else return &chunks.at(pos);
}

void Map::add_chunk(net::server::Chunk const &new_chunk)
{
    chunk::Pos const &chunk_pos = new_chunk.pos;
    if(chunks.count(chunk_pos) > 0) return;

    util::log("MAP", util::TRACE, "adding new chunk %d, %d, %d",
        chunk_pos.x,
        chunk_pos.y,
        chunk_pos.z);

    auto &chunk = chunks[chunk_pos];
    /* this creates a new chunk */
    chunk.tiles.reserve(chunk::TILE_SIZE);
    {
        SizeT worst_case_len = chunk::TILE_SIZE / 2 +
            (chunk::TILE_SIZE % 2 == 0 ? 0 : 1);
        /* this is the size of a checkerboard pattern, the worst case for this
         * algorithm.
         */
        chunk.vertices.reserve(worst_case_len * 6 * 4); //TODO magic numbers
        chunk.indices.reserve(worst_case_len * 6 * 6);  //
    }

    glGenBuffers(1, &chunk.vbo_id);
    glGenBuffers(1, &chunk.ebo_id);

    /* MESHING BEGINS */
    // TODO put it into another function

    render::Index index_offset = 0;
    const glm::vec3 quads[6][4] =
        {{{0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 1.0, 1.0}, {0.0, 0.0, 1.0}},
         {{1.0, 0.0, 0.0}, {1.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0, 0.0}},
         {{0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, {1.0, 0.0, 0.0}},
         {{0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}, {1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}},
         {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}},
         {{0.0, 0.0, 1.0}, {0.0, 1.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 0.0, 1.0}}};
    const map::Pos offsets[6] =
        {{-1,  0,  0}, { 1,  0,  0},
         { 0, -1,  0}, { 0,  1,  0},
         { 0,  0, -1}, { 0,  0,  1}};
    const render::TexPos tex_positions[6][4] =
        {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}, {{1, 0}, {1, 1}, {0, 1}, {0, 0}},
         {{1, 0}, {1, 1}, {0, 1}, {0, 0}}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}},
         {{0, 0}, {1, 0}, {1, 1}, {0, 1}}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}};

    const Hash void_hash = std::hash<String>()("void"); //TODO constexpr

    bool solid_map[chunk::TILE_SIZE];
    for(SizeT i = 0; i < chunk::TILE_SIZE; ++i)
    {
        chunk.tiles.emplace_back(db.tile_types.at(new_chunk.tiles[i].db_hash));
        solid_map[i] = new_chunk.tiles[i].db_hash != void_hash;
    }
    for(SizeT i = 0; i < chunk::TILE_SIZE; ++i)
    {
        if(solid_map[i])
        {
            map::Pos map_pos = chunk::to_map_pos(chunk_pos, i);
            for(SizeT side = 0; side < 6; ++side)
            {
                map::Pos side_pos = map_pos + offsets[side];
                if(chunk::to_pos(side_pos) != chunk_pos ||
                   !solid_map[chunk::to_index(side_pos)])
                {
                    for(unsigned j = 0; j < 4; ++j)
                    {
                        glm::vec4 col = glm::vec4(1.0);
                        chunk.vertices.emplace_back(
                            (glm::vec3)map_pos + quads[side][j], col,
                            chunk.tiles[i].type->tex_pos +
                                tex_positions[side][j]);
                    }
                    for(auto const &idx : {0, 1, 2, 2, 3, 0})
                    {
                        chunk.indices.emplace_back(idx + index_offset);
                    }
                    index_offset += 4;
                }
            }
        }
    }
    /* MESHING ENDS */

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
