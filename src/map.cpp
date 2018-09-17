#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>
#include <lux_shared/net/net_order.hpp>
#include <lux_shared/util/packer.hpp>
//
#include <db.hpp>
#include "map.hpp"

HashMap<ChkPos, Chunk, util::Packer<ChkPos>> chunks;

static void build_mesh(Chunk &chunk, ChkPos const &pos);

bool is_chunk_loaded(ChkPos const& pos) {
    return chunks.count(pos) > 0;
}

void load_chunk(NetServerSignal::MapLoad::Chunk const& net_chunk) {
    ChkPos pos = net_order(net_chunk.pos);
    LUX_LOG("loading chunk");
    LUX_LOG("    pos: {%zd, %zd, %zd}", pos.x, pos.y, pos.z);
    if(is_chunk_loaded(pos)) {
        LUX_LOG("chunk already loaded, ignoring it");
        return;
    }
    Chunk& chunk = chunks[pos];
    for(Uns i = 0; i < CHK_VOL; ++i) {
        chunk.voxels[i]     = net_order(net_chunk.voxels[i]);
        chunk.light_lvls[i] = net_order(net_chunk.light_lvls[i]);
    }
}

Chunk const& get_chunk(ChkPos const& pos) {
    LUX_ASSERT(is_chunk_loaded(pos));
    return chunks.at(pos);
}

void try_build_mesh(ChkPos const& pos) {
    ///would use a loop, but atleast it's guaranteed compile-time...
    constexpr MapPos offsets[27] =
        {{-1, -1, -1}, {-1, -1,  0}, {-1, -1,  1},
         {-1,  0, -1}, {-1,  0,  0}, {-1,  0,  1},
         {-1,  1, -1}, {-1,  1,  0}, {-1,  1,  1},
         { 0, -1, -1}, { 0, -1,  0}, { 0, -1,  1},
         { 0,  0, -1}, { 0,  0,  0}, { 0,  0,  1},
         { 0,  1, -1}, { 0,  1,  0}, { 0,  1,  1},
         { 1, -1, -1}, { 1, -1,  0}, { 1, -1,  1},
         { 1,  0, -1}, { 1,  0,  0}, { 1,  0,  1},
         { 1,  1, -1}, { 1,  1,  0}, { 1,  1,  1}};

    if(!is_chunk_loaded(pos)) return;

    Chunk &chunk = chunks.at(pos);

    if(chunk.mesh.state == Chunk::Mesh::BUILT_NO_MESH ||
       chunk.mesh.state == Chunk::Mesh::BUILT_MESH) return;

    for(SizeT side = 0; side < 27; ++side) {
        //@TODO request required chunks
        if(!is_chunk_loaded(pos + (ChkPos)offsets[side])) return;
    }
    build_mesh(chunk, pos);
}

static void build_mesh(Chunk &chunk, ChkPos const &pos) {
    static DynArr<Chunk::Mesh::GVert> g_verts;
    static DynArr<Chunk::Mesh::LVert> l_verts;
    static DynArr<Chunk::Mesh::Idx>  idxs;
    static const VoxelId void_id = db_voxel_id("void");
    g_verts.clear();
    l_verts.clear();
    idxs.clear();

    //@TODO this should be done only once
    /* this is the size of a checkerboard pattern, worst case */
    g_verts.reserve(CHK_VOL * 3 * 4);
    l_verts.reserve(CHK_VOL * 3 * 4);
    idxs.reserve(CHK_VOL * 3 * 6);


    //TODO number of iterations over each blocks can probably be reduced by
    //joining the loops somehow
    constexpr MapPos quads[3][4] =
        {{{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}},
         {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}},
         {{0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}}};
    constexpr MapPos offsets[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    constexpr F32 unit = 1.f - FLT_EPSILON;
    constexpr Vec2F tex_positions[3][4] =
        {{{unit, 0.0f}, {unit, unit}, {0.0f, unit}, {0.0f, 0.0f}},
         {{0.0f, 0.0f}, {unit, 0.0f}, {unit, unit}, {0.0f, unit}},
         {{0.0f, 0.0f}, {unit, 0.0f}, {unit, unit}, {0.0f, unit}}};

    Chunk::Mesh& mesh = chunk.mesh;
    Chunk::Mesh::Idx idx_offset = 0;

    auto get_voxel = [&] (MapPos const &pos) -> VoxelId
    {
        //@TODO use current chunk to reduce to_chk_* calls, and chunks access
        return chunks[to_chk_pos(pos)].voxels[to_chk_idx(pos)];
    };
    auto has_face = [&] (MapPos const &a, MapPos const &b) -> bool
    {
        /// only one of the blocks must be non-void to have a face
        return (get_voxel(a) == void_id) != (get_voxel(b) == void_id);
    };

    //TODO use vector with indices to reduce iteration in second phase?
    //TODO store every two planes (we check 2 voxels at once)
    bool face_map[3][CHK_VOL];
    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        MapPos map_pos = to_map_pos(pos, i);
        for(U32 a = 0; a < 3; ++a) {
            face_map[a][i] = has_face(map_pos, map_pos + offsets[a]);
        }
    }

    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        MapPos map_pos = to_map_pos(pos, i);
        for(U32 a = 0; a < 3; ++a) {
            if(face_map[a][i]) {
                ///if the current chunk is empty, we need to take the texture
                ///of the second chunk with that face
                //@TODO clean and split this mess up
                bool is_solid = chunk.voxels[i] != void_id;
                MapPos vox_pos = map_pos + offsets[a] * (MapCoord)(!is_solid);
                VoxelType vox_type = db_voxel_type(get_voxel(vox_pos));
                for(U32 j = 0; j < 4; ++j) {
                    constexpr MapPos vert_offsets[8] =
                        {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
                         {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
                    Vec4F col_avg(0.f);
                    MapPos v_sign = glm::sign((Vec3F)quads[a][j] - Vec3F(0.5, 0.5, 0.5));
                    for(auto const &vert_offset : vert_offsets) {
                        MapPos v_off_pos = map_pos + vert_offset * v_sign;
                        LightLvl light_lvl =
                            get_chunk(to_chk_pos(v_off_pos)).light_lvls[to_chk_idx(v_off_pos)];
                        col_avg += Vec4F(
                        (F32)((light_lvl & 0xF000) >> 12) / 15.f,
                        (F32)((light_lvl & 0x0F00) >>  8) / 15.f,
                        (F32)((light_lvl & 0x00F0) >>  4) / 15.f,
                        1.f);
                    }
                    col_avg /= 8.f;
                    Chunk::Mesh::GVert& g_vert = g_verts.emplace_back();
                    g_vert.pos = map_pos + quads[a][j],
                    g_vert.tex_pos = (Vec2F)vox_type.tex_pos + tex_positions[a][j];
                    Chunk::Mesh::LVert& l_vert = l_verts.emplace_back();
                    l_vert.col = col_avg;
                }
                constexpr Chunk::Mesh::Idx  cw_order[6] = {0, 1, 2, 2, 3, 0};
                constexpr Chunk::Mesh::Idx ccw_order[6] = {0, 3, 2, 2, 1, 0};
                Chunk::Mesh::Idx const (&order)[6] =
                    is_solid ? cw_order : ccw_order;
                for(auto const &idx : order) {
                    idxs.emplace_back(idx + idx_offset);
                }
                idx_offset += 4;
            }
        }
    }
    if(idxs.size() > 0) {
        glGenBuffers(1, &mesh.g_vbo);
        glGenBuffers(1, &mesh.l_vbo);
        glGenBuffers(1, &mesh.ebo);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.g_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(Chunk::Mesh::GVert) * g_verts.size(),
                     g_verts.data(),
                     GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.l_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(Chunk::Mesh::LVert) * l_verts.size(),
                     l_verts.data(),
                     GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     sizeof(Chunk::Mesh::Idx) * idxs.size(),
                     idxs.data(),
                     GL_STATIC_DRAW);
        mesh.state = Chunk::Mesh::BUILT_MESH;
    } else {
        mesh.state = Chunk::Mesh::BUILT_NO_MESH;
    }
}
