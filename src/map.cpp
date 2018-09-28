#include <cstring>
//
#define GLM_FORCE_PURE
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>
#include <lux_shared/util/packer.hpp>
//
#include <rendering.hpp>
#include <db.hpp>
#include "map.hpp"

GLuint program;
GLuint tileset;

struct {
    GLint pos;
    GLint tex_pos;
    GLint col;
} shader_attribs;

VecMap<ChkPos, Chunk> chunks;
VecSet<ChkPos> chunk_requests;

static void build_mesh(Chunk &chunk, ChkPos const &pos);
static bool try_build_mesh(ChkPos const& pos);

void map_init() {
    char const* tileset_path = "tileset.png";
    Vec2U const tile_size = {16, 16};
    program = load_program("glsl/map.vert", "glsl/map.frag");
    Vec2U tileset_size;
    tileset = load_texture(tileset_path, tileset_size);
    Vec2F tex_scale = (Vec2F)tile_size / (Vec2F)tileset_size;
    glUseProgram(program);

    shader_attribs.pos     = glGetAttribLocation(program, "pos");
    shader_attribs.tex_pos = glGetAttribLocation(program, "tex_pos");
    shader_attribs.col     = glGetAttribLocation(program, "col");

    set_uniform("tex_scale", program, glUniform2fv,
                1, glm::value_ptr(tex_scale));
}

void map_render(EntityVec const& player_pos) {
    glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D, tileset);
    {   Vec2U window_size = get_window_size();
        F32 ratio = (F32)window_size.x / (F32)window_size.y;
        F32 constexpr BASE_SCALE = 0.08f;
        glm::mat4 matrix(1.f);
        matrix = glm::scale(matrix, Vec3F(BASE_SCALE, -BASE_SCALE * ratio, 1.f));
        matrix = glm::translate(matrix, -player_pos);
        set_uniform("matrix", program, glUniformMatrix4fv,
                    1, GL_FALSE, glm::value_ptr(matrix));
    }

    U32 constexpr RENDER_DIST = 2;
    ChkPos center = to_chk_pos(player_pos);
    ChkPos iter;
    iter.z = center.z;
    for(iter.y  = center.y - RENDER_DIST;
        iter.y <= center.y + RENDER_DIST;
        iter.y++) {
        for(iter.x  = center.x - RENDER_DIST;
            iter.x <= center.x + RENDER_DIST;
            iter.x++) {
            if(is_chunk_loaded(iter)) {
                Chunk const& chunk = get_chunk(iter);
                Chunk::Mesh const& mesh = chunk.mesh;
                if(!mesh.is_built) {
                    if(!try_build_mesh(iter)) continue;
                }
#if LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
                glBindBuffer(GL_ARRAY_BUFFER, mesh.g_vbo);
                glVertexAttribPointer(shader_attribs.pos,
                    2, GL_FLOAT, GL_FALSE, sizeof(Chunk::Mesh::GVert),
                    (void*)offsetof(Chunk::Mesh::GVert, pos));
                glVertexAttribPointer(shader_attribs.tex_pos,
                    2, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(Chunk::Mesh::GVert),
                    (void*)offsetof(Chunk::Mesh::GVert, tex_pos));
                glBindBuffer(GL_ARRAY_BUFFER, mesh.l_vbo);
                glVertexAttribPointer(shader_attribs.col,
                    3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Chunk::Mesh::LVert),
                    (void*)offsetof(Chunk::Mesh::LVert, col));
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
                glBindVertexArray(mesh.vao);
#endif
                glEnableVertexAttribArray(shader_attribs.pos);
                glEnableVertexAttribArray(shader_attribs.tex_pos);
                glEnableVertexAttribArray(shader_attribs.col);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
                glDrawElements(GL_TRIANGLES, mesh.trig_count * 3,
                               Chunk::Mesh::INDEX_GL_TYPE, 0);

                glDisableVertexAttribArray(shader_attribs.pos);
                glDisableVertexAttribArray(shader_attribs.tex_pos);
                glDisableVertexAttribArray(shader_attribs.col);
            } else {
                chunk_requests.emplace(iter);
            }
        }
    }
}

bool is_chunk_loaded(ChkPos const& pos) {
    return chunks.count(pos) > 0;
}

void load_chunk(NetSsSgnl::MapLoad::Chunk const& net_chunk) {
    ChkPos const& pos = net_chunk.pos;
    LUX_LOG("loading chunk");
    LUX_LOG("    pos: {%zd, %zd, %zd}", pos.x, pos.y, pos.z);
    if(is_chunk_loaded(pos)) {
        LUX_LOG("chunk already loaded, ignoring it");
        return;
    }
    Chunk& chunk = chunks[pos];
    std::memcpy(chunk.voxels    , net_chunk.voxels,
                CHK_VOL * sizeof(VoxelId));
    std::memcpy(chunk.light_lvls, net_chunk.light_lvls,
                CHK_VOL * sizeof(LightLvl));
}

void light_update(NetSsSgnl::LightUpdate::Chunk const& net_chunk) {
    ChkPos const& pos = net_chunk.pos;
    if(!is_chunk_loaded(pos)) {
        LUX_LOG("chunk is not loaded");
        return;
    }
    Chunk& chunk = chunks.at(pos);
    std::memcpy(chunk.light_lvls, net_chunk.light_lvls,
                CHK_VOL * sizeof(LightLvl));
    Chunk::Mesh& mesh = chunk.mesh;
    if(mesh.is_built) {
        //@TODO hacky and slow
        glDeleteBuffers(1, &mesh.g_vbo);
        glDeleteBuffers(1, &mesh.l_vbo);
        glDeleteBuffers(1, &mesh.ebo);
        mesh.is_built = false;
        mesh.has_empty = false;
        mesh.trig_count = 0;
    }
}

Chunk const& get_chunk(ChkPos const& pos) {
    LUX_ASSERT(is_chunk_loaded(pos));
    return chunks.at(pos);
}

static bool try_build_mesh(ChkPos const& pos) {
    constexpr ChkPos offsets[9] =
        {{ 0,  0,  0}, {-1, -1,  0}, {-1,  0,  0}, {-1,  1,  0}, { 0, -1,  0},
         { 0,  1,  0}, { 1, -1,  0}, { 1,  0,  0}, { 1,  1,  0}};

    bool can_build = true;
    for(auto const& offset : offsets) {
        if(!is_chunk_loaded(pos + offset)) {
            chunk_requests.emplace(pos + offset);
            can_build = false;
        }
    }

    if(!can_build) return false;

    Chunk &chunk = chunks.at(pos);
    LUX_ASSERT(!chunk.mesh.is_built);

    build_mesh(chunk, pos);
    return true;
}

//@CONSIDER replacing stuff with arrays
static void build_mesh(Chunk &chunk, ChkPos const &pos) {
    static DynArr<Chunk::Mesh::GVert> g_verts;
    static DynArr<Chunk::Mesh::LVert> l_verts;
    static DynArr<Chunk::Mesh::Idx>  idxs;

    g_verts.clear();
    l_verts.clear();
    idxs.clear();

    //@TODO this should be done only once
    g_verts.reserve(CHK_VOL * 4);
    l_verts.reserve(CHK_VOL * 4);
    idxs.reserve(CHK_VOL * 6);

    constexpr MapPos quad[4] =
         {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}};
    constexpr Vec2U tex_positions[4] =
        {{0, 0}, {0, 1}, {1, 1}, {1, 0}};

    Chunk::Mesh& mesh = chunk.mesh;
    Chunk::Mesh::Idx idx_offset = 0;

    auto get_voxel = [&] (MapPos const &pos) -> VoxelId
    {
        //@TODO use current chunk to reduce to_chk_* calls, and chunks access
        return chunks[to_chk_pos(pos)].voxels[to_chk_idx(pos)];
    };

    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        MapPos map_pos = to_map_pos(pos, i);
        VoxelType vox_type = db_voxel_type(get_voxel(map_pos));
        if(db_voxel_type(chunk.voxels[i]).shape == VoxelType::EMPTY) continue;
        for(U32 j = 0; j < 4; ++j) {
            constexpr MapPos vert_offsets[4] =
                {{-1, -1, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 0, 0}};
            MapPos vert_pos = map_pos + quad[j];
            Vec3F col_avg(0.f);
            for(auto const &vert_offset : vert_offsets) {
                MapPos v_off_pos = vert_pos + vert_offset;
                LightLvl light_lvl =
                    get_chunk(to_chk_pos(v_off_pos)).light_lvls[to_chk_idx(v_off_pos)];
                col_avg += Vec3F((light_lvl & 0xF000) >> 12,
                                 (light_lvl & 0x0F00) >>  8,
                                 (light_lvl & 0x00F0) >>  4) * 17.f;
            }
            col_avg /= 4.f;
            Chunk::Mesh::GVert& g_vert = g_verts.emplace_back();
            g_vert.pos = vert_pos;
            g_vert.tex_pos = vox_type.tex_pos + tex_positions[j];
            Chunk::Mesh::LVert& l_vert = l_verts.emplace_back();
            l_vert.col = col_avg;
        }
        for(auto const &idx : {0, 1, 2, 2, 3, 0}) {
            idxs.emplace_back(idx + idx_offset);
        }
        idx_offset += 4;
    }
    glGenBuffers(1, &mesh.g_vbo);
    glGenBuffers(1, &mesh.l_vbo);
    glGenBuffers(1, &mesh.ebo);
#if LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);
#endif

    glBindBuffer(GL_ARRAY_BUFFER, mesh.g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Chunk::Mesh::GVert) * g_verts.size(),
                 g_verts.data(), GL_DYNAMIC_DRAW);
#if LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
    glVertexAttribPointer(shader_attribs.pos,
        2, GL_INT, GL_FALSE, sizeof(Chunk::Mesh::GVert),
        (void*)offsetof(Chunk::Mesh::GVert, pos));
    glVertexAttribPointer(shader_attribs.tex_pos,
        2, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(Chunk::Mesh::GVert),
        (void*)offsetof(Chunk::Mesh::GVert, tex_pos));
#endif

    glBindBuffer(GL_ARRAY_BUFFER, mesh.l_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Chunk::Mesh::LVert) * l_verts.size(),
                 l_verts.data(), GL_DYNAMIC_DRAW);
#if LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
    glVertexAttribPointer(shader_attribs.col,
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Chunk::Mesh::LVert),
        (void*)offsetof(Chunk::Mesh::LVert, col));
#endif

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Chunk::Mesh::Idx) * idxs.size(),
                 idxs.data(), GL_STATIC_DRAW);
    mesh.is_built = true;
    mesh.trig_count = idxs.size() / 3; //@TODO
}
