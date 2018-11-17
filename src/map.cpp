#include <config.hpp>
//
#include <cstring>
//
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/component_wise.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>
#include <lux_shared/util/packer.hpp>
//
#include <rendering.hpp>
#include <db.hpp>
#include <client.hpp>
#include <ui.hpp>
#include "map.hpp"

UiId ui_map;
UiId ui_light;

GLuint tile_program;
GLuint tileset;
GLuint light_program;

struct GeometryMesh {
#pragma pack(push, 1)
    struct Vert {
        Vec2<U16> pos;
    };
#pragma pack(pop)
    gl::VertBuff v_buff;
    gl::IdxBuff  i_buff;
};

struct MatMesh {
#pragma pack(push, 1)
    struct Vert {
        Vec2F tex_pos;
    };
#pragma pack(pop)
    gl::VertBuff    v_buff;
    gl::VertContext context;
};

struct LightMesh {
#pragma pack(push, 1)
    struct Vert {
        //@TODO separate vbo?
        Vec2<U16> pos;
        Vec3<U8>  col;
    };
#pragma pack(pop)
    gl::VertBuff    v_buff;
    gl::IdxBuff     i_buff;
    gl::VertContext context;
};

gl::VertFmt tile_vert_fmt;
gl::VertFmt light_vert_fmt;

struct ChunkMesh {
    MatMesh   mat;
    LightMesh light;
};

static GeometryMesh       geometry_mesh;
VecMap<ChkPos, Chunk>     chunks;
VecMap<ChkPos, ChunkMesh> meshes;
VecSet<ChkPos>            chunk_requests;

static void try_build_mesh(ChkPos const& pos);
static void build_mat_mesh(MatMesh& mesh, Chunk const& chunk, ChkPos const& chk_pos);
static void build_light_mesh(LightMesh& mesh, Chunk const& chunk, ChkPos const& chk_pos);

bool map_mouse(U32, Vec2F pos, int, int) {
    auto& action = cs_tick.actions.emplace_back();
    action.tag = NetCsTick::Action::BREAK;
    action.target.tag = NetCsTick::Action::Target::POINT;
    action.target.point = pos;
    return true;
}

static void map_load_programs() {
    char const* tileset_path = "tileset.png";
    Vec2U const tile_size = {8, 8};
    tile_program = load_program("glsl/tile.vert", "glsl/tile.frag");
    Vec2U tileset_size;
    tileset = load_texture(tileset_path, tileset_size);
    Vec2F tex_scale = (Vec2F)tile_size / (Vec2F)tileset_size;

    glUseProgram(tile_program);
    tile_vert_fmt.init(tile_program,
        {{"pos"    , 2, GL_UNSIGNED_SHORT, false, false},
         {"tex_pos", 2, GL_FLOAT         , false, true}});

    set_uniform("tex_scale", tile_program, glUniform2fv,
                1, glm::value_ptr(tex_scale));

    light_program = load_program("glsl/light.vert", "glsl/light.frag");
    glUseProgram(light_program);
    light_vert_fmt.init(light_program,
        {{"pos", 2, GL_UNSIGNED_SHORT, false, false},
         {"col", 3, GL_UNSIGNED_BYTE , true , false}});
}

static void map_render(U32, Transform const&);
static void light_render(U32, Transform const&);

void map_init() {
    map_load_programs();

    Arr<GeometryMesh::Vert, CHK_VOL * 4> verts;
    Arr<U32               , CHK_VOL * 6> idxs;

    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        IdxPos idx_pos = to_idx_pos(i);
        for(Uns j = 0; j < 4; ++j) {
            constexpr Vec2<U16> quad[4] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
            verts[i * 4 + j].pos = (Vec2<U16>)idx_pos + quad[j];
        }
        U32 constexpr idx_order[6] = {0, 1, 2, 2, 3, 1};
        for(Uns j = 0; j < 6; ++j) {
            idxs[i * 6 + j] = i * 4 + idx_order[j];
        }
    }
    gl::VertContext::unbind_all();
    geometry_mesh.v_buff.init();
    geometry_mesh.v_buff.bind();
    geometry_mesh.v_buff.write(CHK_VOL * 4, verts, GL_STATIC_DRAW);

    geometry_mesh.i_buff.init();
    geometry_mesh.i_buff.bind();
    geometry_mesh.i_buff.write(CHK_VOL * 6, idxs, GL_STATIC_DRAW);

    ui_map = ui_create(ui_camera);
    ui_nodes[ui_map].render = &map_render;
    ui_nodes[ui_map].mouse = &map_mouse;
    ui_light = ui_create(ui_camera, 128);
    ui_nodes[ui_light].render = &light_render;
}

static void map_render(U32, Transform const& tr) {
    U32 RENDER_DIST =
        (glm::compMax(rcp(ui_nodes[ui_world].tr.scale) / (F32)CHK_SIZE)) + 2.f;

    Vec2F player_pos = -ui_nodes[ui_camera].tr.pos;
    static DynArr<ChkPos> render_list;
    render_list.reserve(std::pow(2 * RENDER_DIST - 1, 2));

    ChkPos center = to_chk_pos(player_pos);
    ChkPos iter;
    for(iter.y  = center.y - RENDER_DIST;
        iter.y <= center.y + RENDER_DIST;
        iter.y++) {
        for(iter.x  = center.x - RENDER_DIST;
            iter.x <= center.x + RENDER_DIST;
            iter.x++) {
            if(meshes.count(iter) > 0) render_list.emplace_back(iter);
            else try_build_mesh(iter);
        }
    }

    glUseProgram(tile_program);
    set_uniform("scale", tile_program, glUniform2fv,
                1, glm::value_ptr(tr.scale));
    glBindTexture(GL_TEXTURE_2D, tileset);

    for(auto const& chk_pos : render_list) {
        Vec2F translation = tr.pos + (Vec2F)(chk_pos * ChkPos(CHK_SIZE));
        set_uniform("translation", tile_program, glUniform2fv,
                    1, glm::value_ptr(translation));

        MatMesh const& mesh = meshes.at(chk_pos).mat;
        mesh.context.bind();
        geometry_mesh.i_buff.bind();
        glDrawElements(GL_TRIANGLES, CHK_VOL * 6, GL_UNSIGNED_INT, 0);
        mesh.context.unbind();
    }

    render_list.clear();
}

static void light_render(U32, Transform const& tr) {
    //@TODO merge with tile render
    //@TODO no caps
    //@TODO the calculation seems off? (+2.f??)
    U32 RENDER_DIST =
        (glm::compMax(rcp(ui_nodes[ui_world].tr.scale) / (F32)CHK_SIZE)) + 2.f;

    static DynArr<ChkPos> render_list;
    render_list.reserve(std::pow(2 * RENDER_DIST - 1, 2));

    Vec2F player_pos = -ui_nodes[ui_camera].tr.pos;
    ChkPos center = to_chk_pos(player_pos);
    ChkPos iter;
    for(iter.y  = center.y - RENDER_DIST;
        iter.y <= center.y + RENDER_DIST;
        iter.y++) {
        for(iter.x  = center.x - RENDER_DIST;
            iter.x <= center.x + RENDER_DIST;
            iter.x++) {
            if(meshes.count(iter) > 0) render_list.emplace_back(iter);
            else try_build_mesh(iter);
        }
    }
    glUseProgram(light_program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    set_uniform("scale", light_program, glUniform2fv,
                1, glm::value_ptr(tr.scale));
    for(auto const& chk_pos : render_list) {
        Vec2F translation = tr.pos + (Vec2F)(chk_pos * ChkPos(CHK_SIZE));
        set_uniform("translation", light_program, glUniform2fv,
                    1, glm::value_ptr(translation));

        LightMesh const& mesh = meshes.at(chk_pos).light;
        mesh.context.bind();
        mesh.i_buff.bind();
        glDrawElements(GL_TRIANGLES, CHK_VOL * 6, GL_UNSIGNED_INT, 0);
        mesh.context.unbind();
    }
    glDisable(GL_BLEND);

    render_list.clear();
}

void map_reload_program() {
    LUX_LOG("reloading map program");
    glDeleteTextures(1, &tileset);
    glDeleteProgram(tile_program);
    glDeleteProgram(light_program);
    map_load_programs();
}

bool is_chunk_loaded(ChkPos const& pos) {
    return chunks.count(pos) > 0;
}

static void guarantee_chunk(ChkPos const& pos) {
    if(!is_chunk_loaded(pos)) {
        LUX_LOG("loading chunk");
        LUX_LOG("    pos: {%zd, %zd}", pos.x, pos.y);
        chunks[pos];
    }
}

void tiles_update(ChkPos const& pos, NetSsSgnl::Tiles::Chunk const& net_chunk) {
    guarantee_chunk(pos);
    Chunk& chunk = chunks.at(pos);
    std::memcpy(chunk.id, net_chunk.id, sizeof(net_chunk.id));
    std::memcpy(chunk.wall.raw_data, net_chunk.wall.raw_data,
                sizeof(net_chunk.wall));
    std::memset(chunk.light_lvl, 0, sizeof(chunk.light_lvl));
    if(meshes.count(pos) > 0) {
        build_mat_mesh(meshes.at(pos).mat, chunk, pos);
    }
}

void light_update(ChkPos const& pos,
                  NetSsSgnl::Light::Chunk const& net_chunk) {
    if(!is_chunk_loaded(pos)) {
        LUX_LOG("chunk is not loaded");
        return;
    }
    Chunk& chunk = chunks.at(pos);
    std::memcpy(chunk.light_lvl, net_chunk.light_lvl,
                CHK_VOL * sizeof(LightLvl));
    if(meshes.count(pos) > 0) {
        build_light_mesh(meshes.at(pos).light, chunk, pos);
    }
}

Chunk const& get_chunk(ChkPos const& pos) {
    LUX_ASSERT(is_chunk_loaded(pos));
    return chunks.at(pos);
}

static void try_build_mesh(ChkPos const& pos) {
    constexpr ChkPos offsets[9] =
        {{-1, -1}, { 0, -1}, { 1, -1},
         {-1,  0}, { 0,  0}, { 1,  0},
         {-1,  1}, { 0,  1}, { 1,  1}};
    bool can_build = true;
    for(auto const& offset : offsets) {
        if(!is_chunk_loaded(pos + offset)) {
            chunk_requests.emplace(pos + offset);
            can_build = false;
        }
    }
    if(!can_build) return;

    MatMesh& mat_mesh = meshes[pos].mat;
    mat_mesh.v_buff.init();
    mat_mesh.context.init({geometry_mesh.v_buff, mat_mesh.v_buff},
                          tile_vert_fmt);

    build_mat_mesh(mat_mesh, get_chunk(pos), pos);

    LightMesh& light_mesh = meshes[pos].light;
    light_mesh.v_buff.init();
    light_mesh.i_buff.init();

    light_mesh.context.init({light_mesh.v_buff}, light_vert_fmt);
    build_light_mesh(light_mesh, get_chunk(pos), pos);
}

static void build_mat_mesh(MatMesh& mesh, Chunk const& chunk, ChkPos const& chk_pos) {
    Arr<MatMesh::Vert, CHK_VOL * 4> verts;

    constexpr F32 tx_0 = 0.001f;
    constexpr F32 tx_1 = 0.999f;
    constexpr Vec2F tex_quad[4] =
        {{tx_0, tx_0}, {tx_1, tx_0}, {tx_0, tx_1}, {tx_1, tx_1}};

    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        TileBp const& tile_bp = db_tile_bp(chunk.id[i]);
        constexpr MapPos neighbor_offsets[4] =
            {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        U8 neighbors = 0;
        Vec2F offset = {0, 0};
        if(tile_bp.connected_tex) {
            for(Uns n = 0; n < 4; ++n) {
                MapPos map_pos = to_map_pos(chk_pos, i) + neighbor_offsets[n];
                if(get_chunk(to_chk_pos(map_pos)).id[to_chk_idx(map_pos)] ==
                   chunk.id[i]) {
                    neighbors |= 1 << n;
                }
            }
            switch(neighbors) {
                case 0b0000: offset = {0, 0}; break;
                case 0b1111: offset = {1, 0}; break;
                case 0b0101: offset = {2, 0}; break;
                case 0b1010: offset = {3, 0}; break;
                case 0b0111: offset = {0, 1}; break;
                case 0b1110: offset = {1, 1}; break;
                case 0b1101: offset = {2, 1}; break;
                case 0b1011: offset = {3, 1}; break;
                case 0b0011: offset = {0, 2}; break;
                case 0b0110: offset = {1, 2}; break;
                case 0b1100: offset = {2, 2}; break;
                case 0b1001: offset = {3, 2}; break;
                case 0b0001: offset = {0, 3}; break;
                case 0b0010: offset = {1, 3}; break;
                case 0b0100: offset = {2, 3}; break;
                case 0b1000: offset = {3, 3}; break;
            }
        }
        for(Uns j = 0; j < 4; ++j) {
            verts[i * 4 + j].tex_pos =
                (Vec2F)tile_bp.tex_pos + offset + tex_quad[j];
        }
    }

    gl::VertContext::unbind_all();
    mesh.v_buff.bind();
    mesh.v_buff.write(CHK_VOL * 4, verts, GL_DYNAMIC_DRAW);
}

static void build_light_mesh(LightMesh& mesh, Chunk const& chunk, ChkPos const& pos) {
    Arr<LightMesh::Vert, (CHK_SIZE + 1) * (CHK_SIZE + 1)> verts;
    Arr<U32,  CHK_VOL * 6> idxs;

    for(ChkIdx i = 0; i < std::pow(CHK_SIZE + 1, 2); ++i) {
        Vec2<U16> rel_pos = {i % (CHK_SIZE + 1), i / (CHK_SIZE + 1)};
        verts[i].pos = rel_pos;
        Vec2U overflow = glm::equal(rel_pos, Vec2<U16>(CHK_SIZE));
        ChkPos chk_pos = pos + (ChkPos)overflow;
        ChkIdx chk_idx = to_chk_idx(rel_pos & Vec2<U16>(CHK_SIZE - 1));
        LightLvl light_lvl;
        if(chk_pos != pos) {
            light_lvl = get_chunk(chk_pos).light_lvl[chk_idx];
        } else {
            light_lvl = chunk.light_lvl[chk_idx];
        }
        verts[i].col = (Vec3<U8>)glm::round(
            Vec3F((light_lvl >> 11) & 0x1F,
                  (light_lvl >>  6) & 0x1F,
                  (light_lvl >>  1) & 0x1F) * (255.f / 31.f));
    }
    Uns idx_off = 0;
    for(ChkIdx i = 0; i < std::pow(CHK_SIZE + 1, 2); ++i) {
        if(i % (CHK_SIZE + 1) != CHK_SIZE &&
           i / (CHK_SIZE + 1) != CHK_SIZE) {
            U32 constexpr idx_order[6] =
                {0, 1, CHK_SIZE + 1, CHK_SIZE + 1, CHK_SIZE + 2, 1};
            U32 constexpr flipped_idx_order[6] =
                {0, CHK_SIZE + 1, CHK_SIZE + 2, CHK_SIZE + 2, 1, 0};
            if(glm::length((Vec3F)verts[i + 1].col) +
               glm::length((Vec3F)verts[i + CHK_SIZE + 1].col) >
               glm::length((Vec3F)verts[i + 0].col) +
               glm::length((Vec3F)verts[i + CHK_SIZE + 2].col)) {
                for(Uns j = 0; j < 6; ++j) {
                    idxs[idx_off * 6 + j] = i + idx_order[j];
                }
            } else {
                for(Uns j = 0; j < 6; ++j) {
                    idxs[idx_off * 6 + j] = i + flipped_idx_order[j];
                }
            }
            ++idx_off;
        }
    }
    gl::VertContext::unbind_all();
    mesh.v_buff.bind();
    mesh.v_buff.write(std::pow(CHK_SIZE + 1, 2), verts, GL_DYNAMIC_DRAW);

    mesh.i_buff.bind();
    mesh.i_buff.write(CHK_VOL * 6, idxs, GL_DYNAMIC_DRAW);
}
