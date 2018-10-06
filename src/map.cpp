#include <config.hpp>
//
#include <cstring>
//
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

struct GeometryMesh {
    typedef U32 Idx;
    static constexpr GLenum IDX_GL_TYPE = GL_UNSIGNED_INT;
#pragma pack(push, 1)
    struct Vert {
        Vec2<U16> pos;
    };
#pragma pack(pop)
    GLuint vbo;
    GLuint ebo;
};

struct MaterialMesh {
#pragma pack(push, 1)
    struct Vert {
        Vec2<U16> tex_pos;
    };
#pragma pack(pop)
    GLuint vbo;
#if defined(LUX_GL_3_3)
    GLuint vao;
#endif
};

struct {
    GLint pos;
    GLint tex_pos;
} shader_attribs;

static GeometryMesh   geometry_mesh;
//@CONSIDER putting mesh into chunk?
VecMap<ChkPos, Chunk>        chunks;
VecMap<ChkPos, MaterialMesh> meshes;
VecSet<ChkPos> chunk_requests;

static void build_material_mesh(MaterialMesh& mesh, Chunk const& chunk);

static void map_load_program() {
    char const* tileset_path = "tileset.png";
    Vec2U const tile_size = {16, 16};
    program = load_program("glsl/map.vert", "glsl/map.frag");
    Vec2U tileset_size;
    tileset = load_texture(tileset_path, tileset_size);
    Vec2F tex_scale = (Vec2F)tile_size / (Vec2F)tileset_size;
    glUseProgram(program);

    shader_attribs.pos     = glGetAttribLocation(program, "pos");
    shader_attribs.tex_pos = glGetAttribLocation(program, "tex_pos");

    set_uniform("tex_scale", program, glUniform2fv,
                1, glm::value_ptr(tex_scale));
}

void map_init() {
    map_load_program();

    Arr<GeometryMesh::Vert, CHK_VOL * 4> verts;
    Arr<GeometryMesh::Idx,  CHK_VOL * 2 * 3>  idxs;

    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        IdxPos idx_pos = to_idx_pos(i);
        for(Uns j = 0; j < 4; ++j) {
            constexpr Vec2<U16> quad[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
            verts[i * 4 + j].pos = (Vec2<U16>)idx_pos + quad[j];
        }
        GeometryMesh::Idx constexpr idx_order[6] = {0, 1, 2, 2, 3, 0};
        for(Uns j = 0; j < 6; ++j) {
            idxs[i * 6 + j] = i * 4 + idx_order[j];
        }
    }
    glGenBuffers(1, &geometry_mesh.vbo);
    glGenBuffers(1, &geometry_mesh.ebo);

    glBindBuffer(GL_ARRAY_BUFFER, geometry_mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GeometryMesh::Vert) *
        CHK_VOL * 4, verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry_mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GeometryMesh::Idx) *
        CHK_VOL * 2 * 3, idxs, GL_STATIC_DRAW);
}

void map_render(EntityVec const& player_pos) {
    glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D, tileset);
    {   Vec2U window_size = get_window_size();
        F32 ratio = (F32)window_size.x / (F32)window_size.y;
        F32 constexpr BASE_SCALE = 0.03f;
        Vec2F scale       = Vec2F(BASE_SCALE, -BASE_SCALE * ratio);
        set_uniform("scale", program, glUniform2fv,
                    1, glm::value_ptr(scale));
    }
#if defined(LUX_GLES_2_0)
    glBindBuffer(GL_ARRAY_BUFFER, geometry_mesh.vbo);
    glVertexAttribPointer(shader_attribs.pos,
        2, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(GeometryMesh::Vert),
        (void*)offsetof(GeometryMesh::Vert, pos));
    glEnableVertexAttribArray(shader_attribs.pos);
#endif

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
                Vec2F translation = Vec2F(-player_pos) +
                    (Vec2F)(iter * ChkPos(CHK_SIZE));
                set_uniform("translation", program, glUniform2fv,
                            1, glm::value_ptr(translation));

                MaterialMesh const& mesh = meshes.at(iter);
#if defined(LUX_GLES_2_0)
                glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
                glVertexAttribPointer(shader_attribs.tex_pos,
                    2, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(MaterialMesh::Vert),
                    (void*)offsetof(MaterialMesh::Vert, tex_pos));
                glEnableVertexAttribArray(shader_attribs.tex_pos);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry_mesh.ebo);
#elif defined(LUX_GL_3_3)
                glBindVertexArray(mesh.vao);
#endif
                glDrawElements(GL_TRIANGLES, CHK_VOL * 2 * 3,
                    GeometryMesh::IDX_GL_TYPE, 0);
#if defined(LUX_GLES_2_0)
                glDisableVertexAttribArray(shader_attribs.tex_pos);
#endif
            } else {
                chunk_requests.emplace(iter);
            }
        }
    }
#if defined(LUX_GLES_2_0)
    glDisableVertexAttribArray(shader_attribs.pos);
#endif
}

void map_reload_program() {
    LUX_LOG("reloading map program");
    glDeleteTextures(1, &tileset);
    glDeleteProgram(program);
    map_load_program();
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
    ///insert new mesh
    MaterialMesh& mesh = meshes[pos];
    glGenBuffers(1, &mesh.vbo);
    build_material_mesh(mesh, chunk);
#if defined(LUX_GL_3_3)
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, geometry_mesh.vbo);
    glVertexAttribPointer(shader_attribs.pos,
        2, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(GeometryMesh::Vert),
        (void*)offsetof(GeometryMesh::Vert, pos));
    glEnableVertexAttribArray(shader_attribs.pos);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glVertexAttribPointer(shader_attribs.tex_pos,
        2, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(MaterialMesh::Vert),
        (void*)offsetof(MaterialMesh::Vert, tex_pos));
    glEnableVertexAttribArray(shader_attribs.tex_pos);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry_mesh.ebo);
#endif
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
    LUX_UNIMPLEMENTED();
    //@TODO mesh
}

Chunk const& get_chunk(ChkPos const& pos) {
    LUX_ASSERT(is_chunk_loaded(pos));
    return chunks.at(pos);
}

static void build_material_mesh(MaterialMesh& mesh, Chunk const& chunk) {
    Arr<MaterialMesh::Vert, CHK_VOL * 4> verts;

    constexpr Vec2<U16> tex_quad[4] =
        {{0, 0}, {0, 1}, {1, 1}, {1, 0}};

    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        VoxelType const& vox_type = db_voxel_type(chunk.voxels[i]);
        for(U32 j = 0; j < 4; ++j) {
            verts[i * 4 + j].tex_pos = (Vec2<U16>)vox_type.tex_pos + tex_quad[j];
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(MaterialMesh::Vert) *
        CHK_VOL * 4, verts, GL_DYNAMIC_DRAW);
}
