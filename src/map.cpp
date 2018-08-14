#include <glm/glm.hpp>
//
#include <lux/alias/set.hpp>
#include <lux/util/log.hpp>
#include <lux/net/server/chunk.hpp>
//
#include <data/database.hpp>
#include <map/tile/tile_type.hpp>
#include "map.hpp"

Map::Map(data::Database const &db) :
    db(db)
{

}

map::Tile const *Map::get_tile(MapPos const &pos) const
{
    ChkPos chunk_pos   = to_chk_pos(pos);
    ChkIdx chunk_idx = to_chk_idx(pos);
    map::Chunk const *chunk_ptr = get_chunk(chunk_pos);
    if(chunk_ptr != nullptr) return &chunk_ptr->tiles[chunk_idx];
    else return nullptr;
}

map::Chunk const *Map::get_chunk(ChkPos const &pos) const
{
    if(chunks.count(pos) == 0)
    {
        return nullptr;
    }
    else return &chunks.at(pos);
}

void Map::add_chunk(net::server::Chunk const &new_chunk)
{
    ChkPos const &chunk_pos = new_chunk.pos;
    if(chunks.count(chunk_pos) > 0) return;

    util::log("MAP", util::TRACE, "adding new chunk %d, %d, %d",
        chunk_pos.x,
        chunk_pos.y,
        chunk_pos.z);

    auto &chunk = chunks[chunk_pos];
    /* this creates a new chunk */
    chunk.tiles.reserve(CHK_VOLUME);
    {
        SizeT worst_case_len = CHK_VOLUME / 2 +
            (CHK_VOLUME % 2 == 0 ? 0 : 1);
        /* this is the size of a checkerboard pattern, the worst case for this
         * algorithm.
         */
        chunk.vertices.reserve(worst_case_len * 6 * 4); //TODO magic numbers
        chunk.indices.reserve(worst_case_len * 6 * 6);  //
    }

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
    const MapPos offsets[6] =
        {{-1,  0,  0}, { 1,  0,  0},
         { 0, -1,  0}, { 0,  1,  0},
         { 0,  0, -1}, { 0,  0,  1}};
    const render::TexPos tex_positions[6][4] =
        {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}, {{1, 0}, {1, 1}, {0, 1}, {0, 0}},
         {{1, 0}, {1, 1}, {0, 1}, {0, 0}}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}},
         {{0, 0}, {1, 0}, {1, 1}, {0, 1}}, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}};

    const tile::Id void_id = db.get_tile_id("void"); //TODO constexpr

    bool solid_map[CHK_VOLUME];
    for(SizeT i = 0; i < CHK_VOLUME; ++i)
    {
        chunk.tiles.emplace_back(&db.get_tile(new_chunk.tiles[i].id));
        solid_map[i] = new_chunk.tiles[i].id != void_id;
    }
    for(SizeT i = 0; i < CHK_VOLUME; ++i)
    {
        MapPos map_pos = to_map_pos(chunk_pos, i);
        if(solid_map[i])
        {
            for(SizeT side = 0; side < 6; ++side)
            {
                MapPos side_pos = map_pos + offsets[side];
                if(to_chk_pos(side_pos) != chunk_pos ||
                   !solid_map[to_chk_idx(side_pos)])
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

    if(chunk.vertices.size() == 0)
    {
        chunks.erase(chunk_pos);
    }
    else
    {
        glGenBuffers(1, &chunk.vbo_id);
        glGenBuffers(1, &chunk.ebo_id);

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
