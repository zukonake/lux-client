#include <glm/glm.hpp>
//
#include <lux/alias/hash_map.hpp>
#include <lux/util/log.hpp>
#include <lux/net/server/chunk.hpp>
//
#include <config.h>
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

    for(SizeT i = 0; i < CHK_VOLUME; ++i)
    {
        chunk.tiles.emplace_back(&db.get_tile(new_chunk.tile_ids[i]));
    }

}

void Map::try_mesh(ChkPos const &pos)
{
    constexpr MapPos offsets[6] =
        {{-1,  0,  0}, { 1,  0,  0},
         { 0, -1,  0}, { 0,  1,  0},
         { 0,  0, -1}, { 0,  0,  1}};

    for(SizeT side = 0; side < 6; ++side)
    {
        if(chunks.count(pos + (ChkPos)offsets[side]) == 0) return;
        /* surrounding chunks need to be loaded to mesh */
    }
    map::Chunk &chunk = chunks.at(pos);
    HashMap<tile::Id, map::TileType const *> materials;
    for(U32 i = 0; i < CHK_VOLUME; ++i)
    {
        map::TileType const *tile_type = chunk.tiles[i].type;
        tile::Id id = tile_type->id;
        if(materials.count(id) == 0)
        {
            materials[id] = tile_type;
        }
    }
    render::Mesh &mesh = chunk.mesh;
    {
        SizeT worst_case_len = CHK_VOLUME / 2;
        /* this is the size of a checkerboard pattern, the worst possible case
         */
        mesh.vertices.reserve(worst_case_len * 6 * 4);
        mesh.indices.reserve(worst_case_len * 6 * 6);
    }
    for(auto const &material : materials)
    {
        build_material_mesh(chunk, pos, *material.second);
    }
    mesh.vertices.shrink_to_fit();
    mesh.indices.shrink_to_fit();

    if(mesh.vertices.size() > 0)
    {
        glGenBuffers(1, &mesh.vbo_id);
        glGenBuffers(1, &mesh.ebo_id);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_id);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(render::Vertex) * mesh.vertices.size(),
                     mesh.vertices.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     sizeof(render::Index) * mesh.indices.size(),
                     mesh.indices.data(),
                     GL_STATIC_DRAW);
    }
    chunk.is_mesh_generated = true;
}

void Map::build_material_mesh(map::Chunk &chunk, ChkPos const &pos,
                              map::TileType const &material)
{
    constexpr MapPos quads[6][4] =
        {{{0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}},
         {{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}},
         {{0, 0, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0}},
         {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}},
         {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}},
         {{0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}}};
    constexpr MapPos offsets[6] =
        {{-1,  0,  0}, { 1,  0,  0},
         { 0, -1,  0}, { 0,  1,  0},
         { 0,  0, -1}, { 0,  0,  1}};
    constexpr F32 unit = 1.f - FLT_EPSILON;
    constexpr glm::vec2 tex_positions[6][4] =
        {{{0.0f, 0.0f}, {unit, 0.0f}, {unit, unit}, {0.0f, unit}},
         {{unit, 0.0f}, {unit, unit}, {0.0f, unit}, {0.0f, 0.0f}},
         {{unit, 0.0f}, {unit, unit}, {0.0f, unit}, {0.0f, 0.0f}},
         {{0.0f, 0.0f}, {unit, 0.0f}, {unit, unit}, {0.0f, unit}},
         {{0.0f, 0.0f}, {unit, 0.0f}, {unit, unit}, {0.0f, unit}},
         {{0.0f, 0.0f}, {unit, 0.0f}, {unit, unit}, {0.0f, unit}}};

    render::Mesh &mesh = chunk.mesh;
    render::Index index_offset = mesh.vertices.size();
    tile::Id void_id = db.get_tile_id("void");
    //TODO move to class

    auto is_solid = [&] (MapPos const &pos)
    {
        //TODO use current chunk to reduce to_chk_* calls, and chunks access
        return chunks[to_chk_pos(pos)].tiles[to_chk_idx(pos)].type->id != void_id;
    };

    for(SizeT i = 0; i < CHK_VOLUME; ++i)
    {
        MapPos map_pos = to_map_pos(pos, i);
        if(is_solid(map_pos) && chunk.tiles[i].type->id == material.id)
        {
            for(SizeT side = 0; side < 6; ++side)
            {
                if(!is_solid(map_pos + offsets[side]))
                {
                    for(unsigned j = 0; j < 4; ++j)
                    {
                        glm::vec4 col = glm::vec4(1.0);
                        mesh.vertices.emplace_back(
                            map_pos + quads[side][j], col,
                            (glm::vec2)material.tex_pos +
                                tex_positions[side][j]);
                    }
                    for(auto const &idx : {0, 1, 2, 2, 3, 0})
                    {
                        mesh.indices.emplace_back(idx + index_offset);
                    }
                    index_offset += 4;
                }
            }
        }
    }
}
