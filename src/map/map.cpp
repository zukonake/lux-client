#include <glm/glm.hpp>
//
#include <lux/alias/hash_map.hpp>
#include <lux/util/log.hpp>
#include <lux/net/server/chunk.hpp>
//
#include <config.h>
#include <data/database.hpp>
#include <map/voxel_type.hpp>
#include "map.hpp"

Map::Map(data::Database const &db) :
    db(db)
{

}

VoxelId const *Map::get_voxel(MapPos const &pos) const
{
    ChkPos chunk_pos   = to_chk_pos(pos);
    ChkIdx chunk_idx = to_chk_idx(pos);
    Chunk const *chunk_ptr = get_chunk(chunk_pos);
    if(chunk_ptr != nullptr) return &chunk_ptr->voxels[chunk_idx];
    else return nullptr;
}

Chunk const *Map::get_chunk(ChkPos const &pos) const
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
    //TODO assert
    if(chunks.count(chunk_pos) > 0) return;

    util::log("MAP", util::TRACE, "adding new chunk %d, %d, %d",
        chunk_pos.x,
        chunk_pos.y,
        chunk_pos.z);

    auto &chunk = chunks[chunk_pos];
    /* this creates a new chunk */

    std::copy(new_chunk.voxels.cbegin(), new_chunk.voxels.cend(),
              chunk.voxels.begin());
    std::copy(new_chunk.light_lvls.cbegin(), new_chunk.light_lvls.cend(),
              chunk.light_lvls.begin());
}

void Map::try_mesh(ChkPos const &pos)
{
    constexpr MapPos vert_offsets[27] =
        {{-1, -1, -1}, {-1, -1,  0}, {-1, -1,  1},
         {-1,  0, -1}, {-1,  0,  0}, {-1,  0,  1},
         {-1,  1, -1}, {-1,  1,  0}, {-1,  1,  1},
         { 0, -1, -1}, { 0, -1,  0}, { 0, -1,  1},
         { 0,  0, -1}, { 0,  0,  0}, { 0,  0,  1},
         { 0,  1, -1}, { 0,  1,  0}, { 0,  1,  1},
         { 1, -1, -1}, { 1, -1,  0}, { 1, -1,  1},
         { 1,  0, -1}, { 1,  0,  0}, { 1,  0,  1},
         { 1,  1, -1}, { 1,  1,  0}, { 1,  1,  1}};
    assert(chunks.count(pos) > 0);
    assert(!chunks.at(pos).is_mesh_generated);

    for(SizeT side = 0; side < 27; ++side) {
        /* the chunks on positive offsets need to be loaded,
         * we also load the negative offsets so there is no asymmetry */
        //TODO this would be fixed if client controls chunk loading,
        //     it would simply request required chunks
        if(chunks.count(pos + (ChkPos)vert_offsets[side]) == 0) return;
    }
    Chunk &chunk = chunks.at(pos);
    build_mesh(chunk, pos);
}

void Map::build_mesh(Chunk &chunk, ChkPos const &pos)
    //TODO number of iterations over each blocks can probably be reduced by
    //joining the loops somehow
{
    constexpr MapPos quads[3][4] =
        {{{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}},
         {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}},
         {{0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}}};
    constexpr MapPos offsets[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    constexpr F32 unit = 1.f - FLT_EPSILON;
    constexpr glm::vec2 tex_positions[3][4] =
        {{{unit, 0.0f}, {unit, unit}, {0.0f, unit}, {0.0f, 0.0f}},
         {{0.0f, 0.0f}, {unit, 0.0f}, {unit, unit}, {0.0f, unit}},
         {{0.0f, 0.0f}, {unit, 0.0f}, {unit, unit}, {0.0f, unit}}};

    render::Mesh &mesh = chunk.mesh;

    /* this is the size of a checkerboard pattern, worst case */
    mesh.vertices.reserve(CHK_VOLUME * 3 * 4);
    mesh.indices.reserve(CHK_VOLUME * 3 * 6);

    render::Index index_offset = 0;
    VoxelId void_id = db.get_voxel_id("void");
    //TODO ^ move to class

    auto get_voxel = [&] (MapPos const &pos) -> VoxelId
    {
        //TODO use current chunk to reduce to_chk_* calls, and chunks access
        return chunks[to_chk_pos(pos)].voxels[to_chk_idx(pos)];
    };
    auto has_face = [&] (MapPos const &a, MapPos const &b) -> bool
    {
        /* only one of the blocks must be non-void to have a face */
        return (get_voxel(a) == void_id) !=
               (get_voxel(b) == void_id);
    };

    //TODO use vector with indices to reduce iteration in second phase?
    bool face_map[3][CHK_VOLUME];
    for(ChkIdx i = 0; i < CHK_VOLUME; ++i) {
        MapPos map_pos = to_map_pos(pos, i);
        for(U32 a = 0; a < 3; ++a) {
            face_map[a][i] = has_face(map_pos, map_pos + offsets[a]);
        }
    }

    for(ChkIdx i = 0; i < CHK_VOLUME; ++i) {
        MapPos map_pos = to_map_pos(pos, i);
        for(U32 a = 0; a < 3; ++a) {
            if(face_map[a][i]) {
                /* if the current chunk is empty, we need to take the texture
                 * of the second chunk with that face */
                bool is_solid = chunk.voxels[i] != void_id;
                MapPos vox_pos = map_pos + offsets[a] * (I32)(!is_solid);
                MapPos air_pos = map_pos + offsets[a] * (I32)(is_solid);
                VoxelType vox_type = db.voxels[get_voxel(vox_pos)];
                for(U32 j = 0; j < 4; ++j) {
                    constexpr MapPos vert_offsets[8] =
                        {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
                         {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
                    glm::vec4 col_avg(0.f);
                    MapPos v_sign = glm::sign((Vec3F)quads[a][j] - Vec3F(0.5, 0.5, 0.5));
                    for(auto const &vert_offset : vert_offsets) {
                        MapPos v_off_pos = map_pos + //quads[a][j] +
                                           vert_offset * v_sign;
                        LightLvl light_lvl =
                            chunks.at(to_chk_pos(v_off_pos)).light_lvls[to_chk_idx(v_off_pos)];
                        col_avg += glm::vec4(
                        (F32)((light_lvl & 0xF000) >> 12) / 16.f,
                        (F32)((light_lvl & 0x0F00) >>  8) / 16.f,
                        (F32)((light_lvl & 0x00F0) >>  4) / 16.f,
                        1.f);
                    }
                    col_avg /= 8.f;
                    mesh.vertices.emplace_back(
                        map_pos + quads[a][j], col_avg,
                        (glm::vec2)vox_type.tex_pos + tex_positions[a][j]);
                }
                constexpr render::Index  cw_order[6] = {0, 1, 2, 2, 3, 0};
                constexpr render::Index ccw_order[6] = {0, 3, 2, 2, 1, 0};
                render::Index const (&order)[6] =
                    is_solid ? cw_order : ccw_order;
                for(render::Index idx : order) {
                    mesh.indices.emplace_back(idx + index_offset);
                }
                index_offset += 4;
            }
        }
    }
    mesh.vertices.shrink_to_fit();
    mesh.indices.shrink_to_fit();

    if(mesh.vertices.size() > 0) {
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
