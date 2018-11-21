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
UiId ui_roof;
UiId ui_fov;

GLuint light_program;
GLuint tile_program;
GLuint roof_program;
GLuint tileset;

gl::VertFmt light_vert_fmt;
gl::VertFmt tile_vert_fmt[3];
struct FovSystem {
#pragma pack(push, 1)
    struct Vert {
        Vec3F pos;
    };
#pragma pack(pop)
    GLuint program;
    gl::VertFmt  vert_fmt;
    gl::VertBuff v_buff;
    gl::IdxBuff  i_buff;
    gl::VertContext context;
} static fov_system;

struct LightMesh {
#pragma pack(push, 1)
    struct Vert {
        ///this refers to the middle of a tile
        Vec2<U8> pos;
        Vec3<U8> light;
    };
#pragma pack(pop)
    gl::VertBuff    v_buff;
    gl::IdxBuff     i_buff;
    gl::VertContext context;
};

struct TileMesh {
#pragma pack(push, 1)
    struct Vert {
        ///this is multiplied by 2, because we need access to half-tiles
        Vec2<U8> pos;
        Vec2F    layer_tex[3];
    };
#pragma pack(pop)
    gl::VertBuff    v_buff;
    gl::IdxBuff     i_buff;
    gl::VertContext context[3];
};

struct Mesh {
    LightMesh light;
    TileMesh  tile;
};

VecMap<ChkPos, Chunk> chunks;
VecMap<ChkPos, Mesh>  meshes;
VecSet<ChkPos>        chunk_requests;

static bool try_build_mesh(ChkPos const& pos);
static void build_tile_mesh(TileMesh& mesh, ChkPos const& chk_pos);
static void build_light_mesh(LightMesh& mesh, ChkPos const& chk_pos);

bool map_mouse(U32, Vec2F pos, int, int) {
    /*auto& action = cs_tick.actions.emplace_back();
    action.tag = NetCsTick::Action::BREAK;
    action.target.tag = NetCsTick::Action::Target::POINT;
    action.target.point = pos;*/
    glUseProgram(roof_program);
    set_uniform("cursor_pos", roof_program, glUniform2fv,
                1, glm::value_ptr(pos));
    return true;
}

bool map_scroll(U32, Vec2F, F64 off) {
    auto& world = ui_nodes[ui_world];
    F32 old_ratio = world.tr.scale.x / world.tr.scale.y;
    //@TODO this shouldn't be exponential/hyperbolic like it is now
    world.tr.scale.y += off * 0.01f;
    world.tr.scale.y = glm::clamp(world.tr.scale.y, 1.f / 50.f, 1.f);
    world.tr.scale.x = world.tr.scale.y * old_ratio;
    return true;
}

static void map_load_programs() {
    char const* tileset_path = "tileset.png";
    Vec2U const tile_size = {8, 8};
    tile_program  = load_program("glsl/tile.vert" , "glsl/tile.frag");
    roof_program  = load_program("glsl/roof.vert" , "glsl/roof.frag");
    light_program = load_program("glsl/light.vert", "glsl/light.frag");
    fov_system.program = load_program("glsl/fov.vert", "glsl/fov.frag");
    Vec2U tileset_size;
    tileset = load_texture(tileset_path, tileset_size);
    Vec2F tex_scale = (Vec2F)tile_size / (Vec2F)tileset_size;

    glUseProgram(tile_program);
    set_uniform("tex_scale", tile_program, glUniform2fv,
                1, glm::value_ptr(tex_scale));
    for(Uns i = 0; i < 2; i++) {
        tile_vert_fmt[i].init(tile_program,
            {{"pos"    , 2, GL_UNSIGNED_BYTE, false, false},
             {"tex_pos", 2, GL_FLOAT        , false, false}});
    }

    glUseProgram(roof_program);
    //@TODO calc
    set_uniform("reveal_rad", roof_program, glUniform1f, 8.f);
    set_uniform("tex_scale", roof_program, glUniform2fv,
                1, glm::value_ptr(tex_scale));
    tile_vert_fmt[2].init(roof_program,
        {{"pos"    , 2, GL_UNSIGNED_BYTE, false, false},
         {"tex_pos", 2, GL_FLOAT        , false, false}});

    for(Uns i = 0; i < 3; i++) {
        SizeT constexpr stride = sizeof(TileMesh::Vert);
        tile_vert_fmt[i].attribs[0].stride = stride;
        tile_vert_fmt[i].attribs[1].stride = stride;
        ///we offset the attribs for each tile type (floor, wall, roof)
        (SizeT&)tile_vert_fmt[i].attribs[1].off += i * sizeof(Vec2F);
    }

    glUseProgram(light_program);
    light_vert_fmt.init(light_program,
        {{"pos"  , 2, GL_UNSIGNED_BYTE, false, false},
         {"light", 3, GL_UNSIGNED_BYTE, true , false}});

    glUseProgram(fov_system.program);
    fov_system.vert_fmt.init(fov_system.program,
        {{"pos", 3, GL_FLOAT, false, false}});
    fov_system.v_buff.init();
    fov_system.i_buff.init();
    fov_system.context.init({fov_system.v_buff}, fov_system.vert_fmt);
}

static void map_render(U32, Transform const&);
static void light_render(U32, Transform const&);
static void roof_render(U32, Transform const&);
static void fov_render(U32, Transform const&);

void map_init() {
    map_load_programs();

    ui_map = ui_create(ui_camera);
    ui_nodes[ui_map].render = &map_render;
    ui_nodes[ui_map].mouse  = &map_mouse;
    ui_nodes[ui_map].scroll = &map_scroll;
    ui_light = ui_create(ui_camera, 0x80);
    ui_nodes[ui_light].render = &light_render;
    ui_roof = ui_create(ui_camera, 0x80);
    ui_nodes[ui_roof].render = &roof_render;
    ui_fov = ui_create(ui_camera, 0x90);
    ui_nodes[ui_fov].render = &fov_render;
}

static DynArr<ChkPos> render_list;

static void map_render(U32, Transform const& tr) {
    //@TODO this should happen elsewhere
    //@TODO the calculation seems off? (+2.f??)
    U32 render_dist =
        (glm::compMax(rcp(ui_nodes[ui_world].tr.scale) / (F32)CHK_SIZE)) + 2.f;

    Vec2F player_pos = -ui_nodes[ui_camera].tr.pos;
    render_list.clear();
    render_list.reserve(std::pow(2 * render_dist - 1, 2));

    ChkPos center = to_chk_pos(player_pos);
    ChkPos iter;
    for(iter.y  = center.y - render_dist;
        iter.y <= center.y + render_dist;
        iter.y++) {
        for(iter.x  = center.x - render_dist;
            iter.x <= center.x + render_dist;
            iter.x++) {
            if(meshes.count(iter) > 0 || try_build_mesh(iter)) {
                render_list.emplace_back(iter);
            }
        }
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(tile_program);
    set_uniform("scale", tile_program, glUniform2fv,
                1, glm::value_ptr(tr.scale));
    set_uniform("camera_pos", tile_program, glUniform2fv,
                1, glm::value_ptr(tr.pos));

    glBindTexture(GL_TEXTURE_2D, tileset);
    for(auto const& chk_pos : render_list) {
        Vec2F chk_translation = (Vec2F)(chk_pos * ChkPos(CHK_SIZE));
        set_uniform("chk_pos", tile_program, glUniform2fv, 1,
            glm::value_ptr(chk_translation));

        TileMesh& mesh = meshes.at(chk_pos).tile;
        ///render floors, then walls
        for(Uns i = 0; i < 2; i++) {
            mesh.context[i].bind();
            mesh.i_buff.bind();
            glDrawElements(GL_TRIANGLES, CHK_VOL * 6, GL_UNSIGNED_INT, 0);
            mesh.context[i].unbind();
        }
    }
    glDisable(GL_BLEND);
}

static void light_render(U32, Transform const& tr) {
    Vec3F ambient_light =
        glm::mix(Vec3F(1.f, 1.f, 1.f), Vec3F(0.1f, 0.1f, 0.6f),
                 std::cos(glfwGetTime() * 0.1f));
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    glUseProgram(light_program);
    set_uniform("scale", light_program, glUniform2fv,
                1, glm::value_ptr(tr.scale));
    set_uniform("camera_pos", light_program, glUniform2fv,
                1, glm::value_ptr(tr.pos));
    set_uniform("ambient_light", light_program, glUniform3fv,
                1, glm::value_ptr(ambient_light));
    for(auto const& chk_pos : render_list) {
        Vec2F chk_translation = (Vec2F)(chk_pos * ChkPos(CHK_SIZE));
        set_uniform("chk_pos", light_program, glUniform2fv, 1,
            glm::value_ptr(chk_translation));

        LightMesh const& mesh = meshes.at(chk_pos).light;
        mesh.context.bind();
        mesh.i_buff.bind();
        glDrawElements(GL_TRIANGLES, CHK_VOL * 6, GL_UNSIGNED_INT, 0);
        mesh.context.unbind();
    }
    glDisable(GL_BLEND);
}

static void fov_render(U32, Transform const& tr) {
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_DST_COLOR, GL_ZERO);
    glUseProgram(fov_system.program);
    set_uniform("scale", fov_system.program, glUniform2fv,
                1, glm::value_ptr(tr.scale));
    set_uniform("translation", fov_system.program, glUniform2fv,
                1, glm::value_ptr(tr.pos));
    static DynArr<FovSystem::Vert> verts;
    static DynArr<U32>             idxs;
    verts.clear();
    idxs.clear();
    F32 constexpr fov_range = 64.f;
    Vec2F camera_pos = -tr.pos;
    for(auto const& chk_pos : render_list) {
        Vec2F chk_off = chk_pos * ChkPos(CHK_SIZE);
        for(Uns i = 0; i < CHK_VOL; i++) {
            if(get_chunk(chk_pos).wall[i] != void_tile) {
                Vec2F idx_pos = to_idx_pos(i);
                Vec2F map_pos = to_map_pos(chk_pos, i);
                Vec2I dir = -glm::sign(map_pos - camera_pos);
                if(dir.x == 0) dir.x = 1;
                if(dir.y == 0) dir.y = 1;
                Uns edge_idx;
                if(dir.x == -1 && dir.y == -1) {
                    edge_idx = 0;
                } else if(dir.x == 1 && dir.y == -1) {
                    edge_idx = 1;
                } else if(dir.x == 1 && dir.y == 1) {
                    edge_idx = 2;
                } else if(dir.x == -1 && dir.y == 1) {
                    edge_idx = 3;
                } else LUX_UNREACHABLE();
                Vec2F edges[3];
                for(Int j = -1; j <= 1; j++) {
                    edges[j + 1] = u_quad<F32>[(edge_idx + j) % 4] + map_pos;
                }
                for(auto const& idx : {2, 1, 0, 2, 4, 3, 1, 4, 2}) {
                    idxs.push_back(verts.size() + idx);
                }
                if(edges[0] == camera_pos || edges[2] == camera_pos) continue;
                Vec2F rays[2] = {
                    glm::normalize(edges[0] - camera_pos) * fov_range,
                    glm::normalize(edges[2] - camera_pos) * fov_range};
                verts.push_back({{edges[0], 0.5f}});
                verts.push_back({{camera_pos + rays[0], 0.5f}});
                //@TODO modular arithmetic class?
                verts.push_back({{edges[1], 0.5f}});
                verts.push_back({{edges[2], 0.5f}});
                verts.push_back({{camera_pos + rays[1], 0.5f}});
            }
        }
    }
    fov_system.context.bind();
    fov_system.v_buff.bind();
    fov_system.v_buff.write(verts.size(), verts.data(), GL_DYNAMIC_DRAW);
    fov_system.i_buff.bind();
    fov_system.i_buff.write(idxs.size(), idxs.data(), GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, idxs.size(), GL_UNSIGNED_INT, 0);
    fov_system.context.unbind();
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}

static void roof_render(U32, Transform const& tr) {
    Vec3F ambient_light =
        glm::mix(Vec3F(1.f, 1.f, 1.f), Vec3F(0.1f, 0.1f, 0.6f),
                 std::cos(glfwGetTime() * 0.1f));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(roof_program);
    set_uniform("scale", roof_program, glUniform2fv,
                1, glm::value_ptr(tr.scale));
    set_uniform("camera_pos", roof_program, glUniform2fv,
                1, glm::value_ptr(tr.pos));
    set_uniform("ambient_light", roof_program, glUniform3fv,
                1, glm::value_ptr(ambient_light));

    glBindTexture(GL_TEXTURE_2D, tileset);
    for(auto const& chk_pos : render_list) {
        Vec2F chk_translation = (Vec2F)(chk_pos * ChkPos(CHK_SIZE));
        set_uniform("chk_pos", roof_program, glUniform2fv, 1,
            glm::value_ptr(chk_translation));

        TileMesh& mesh = meshes.at(chk_pos).tile;
        ///render roofs
        mesh.context[2].bind();
        mesh.i_buff.bind();
        glDrawElements(GL_TRIANGLES, CHK_VOL * 6, GL_UNSIGNED_INT, 0);
        mesh.context[2].unbind();
    }
    glDisable(GL_BLEND);
}

void map_reload_program() {
    //@TODO we need to reload attrib locations here
    LUX_LOG("reloading map program");
    glDeleteTextures(1, &tileset);
    glDeleteProgram(tile_program);
    glDeleteProgram(roof_program);
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
    std::memcpy(chunk.floor, net_chunk.floor, sizeof(net_chunk.floor));
    std::memcpy(chunk.wall , net_chunk.wall , sizeof(net_chunk.wall));
    std::memcpy(chunk.roof , net_chunk.roof , sizeof(net_chunk.roof));
    for(auto const& offset : chebyshev<ChkCoord>) {
        ChkPos off_pos = pos + offset;
        if(is_chunk_loaded(off_pos) && meshes.count(off_pos) > 0) {
            build_tile_mesh(meshes.at(off_pos).tile, off_pos);
        }
    }
}

void light_update(ChkPos const& pos,
                  NetSsSgnl::Light::Chunk const& net_chunk) {
    if(!is_chunk_loaded(pos)) {
        //@TODO warn
        LUX_LOG("chunk is not loaded");
        return;
    }
    Chunk& chunk = chunks.at(pos);
    std::memcpy(chunk.light_lvl, net_chunk.light_lvl,
                CHK_VOL * sizeof(LightLvl));
    for(auto const& offset : chebyshev<ChkCoord>) {
        ChkPos off_pos = pos + offset;
        if(is_chunk_loaded(off_pos) && meshes.count(off_pos) > 0) {
            build_light_mesh(meshes.at(off_pos).light, off_pos);
        }
    }
}

Chunk const& get_chunk(ChkPos const& pos) {
    LUX_ASSERT(is_chunk_loaded(pos));
    return chunks.at(pos);
}

static bool try_build_mesh(ChkPos const& pos) {
    bool can_build = true;
    for(auto const& offset : chebyshev<ChkCoord>) {
        if(!is_chunk_loaded(pos + offset)) {
            chunk_requests.emplace(pos + offset);
            can_build = false;
        }
    }
    if(!can_build) return false;

    LUX_ASSERT(meshes.count(pos) == 0);
    Mesh& mesh = meshes[pos];

    mesh.tile.v_buff.init();
    mesh.tile.i_buff.init();
    for(Uns i = 0; i < 3; i++) {
        mesh.tile.context[i].init({mesh.tile.v_buff}, tile_vert_fmt[i]);
    }
    build_tile_mesh(mesh.tile, pos);

    mesh.light.v_buff.init();
    mesh.light.i_buff.init();
    mesh.light.context.init({mesh.light.v_buff}, light_vert_fmt);
    build_light_mesh(mesh.light, pos);
    return true;
}

static void build_tile_mesh(TileMesh& mesh, ChkPos const& chk_pos) {
    Chunk const& chunk = get_chunk(chk_pos);
    Arr<TileMesh::Vert, CHK_VOL * 4> verts;
    Arr<U32           , CHK_VOL * 6> idxs;

    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        for(Uns j = 0; j < 6; j++) {
            idxs[i * 6 + j] = i * 4 + quad_idxs<U32>[j];
        }
        Vec2<U8> idx_pos = to_idx_pos(i);
        ///so that the idx coords fit in our vert coords
        static_assert((sizeof(decltype(verts[0].pos)::value_type) * 8) - 1 >=
                      CHK_SIZE_EXP);
        for(Uns j = 0; j < 4; j++) {
            verts[i * 4 + j].pos = (idx_pos + u_quad<U8>[j]) * (U8)2;
        }
        for(Uns j = 0; j < 3; j++) {
            TileBp const& bp = db_tile_bp(chunk.layer[j][i]);
            Uns random_off = 0;
            random_off = lux_rand(to_map_pos(chk_pos, i), j);
            constexpr F32 tx_edge = 0.001f;
            for(Uns k = 0; k < 4; ++k) {
                Uns tex_idx = (k + random_off) % 4;
                Vec2F tex_pos = u_quad<F32>[tex_idx] -
                    glm::sign(quad<F32>[tex_idx]) * tx_edge;
                verts[i * 4 + k].layer_tex[j] = (Vec2F)bp.tex_pos + tex_pos;
            }
        }
    }
    gl::VertContext::unbind_all();
    mesh.v_buff.bind();
    mesh.v_buff.write(CHK_VOL * 4, verts, GL_DYNAMIC_DRAW);
    mesh.i_buff.bind();
    mesh.i_buff.write(CHK_VOL * 6, idxs, GL_DYNAMIC_DRAW);
}

static void build_light_mesh(LightMesh& mesh, ChkPos const& chk_pos) {
    Chunk const& chunk = get_chunk(chk_pos);
    Arr<LightMesh::Vert, (CHK_SIZE + 1) * (CHK_SIZE + 1)> verts;
    Arr<U32, CHK_VOL * 6> idxs;
    for(ChkIdx i = 0; i < std::pow(CHK_SIZE + 1, 2); ++i) {
        Vec2<U16> rel_pos = {i % (CHK_SIZE + 1), i / (CHK_SIZE + 1)};
        verts[i].pos = rel_pos;
        Vec2U overflow = glm::equal(rel_pos, Vec2<U16>(CHK_SIZE));
        ChkPos off_pos = chk_pos + (ChkPos)overflow;
        ChkIdx chk_idx = to_chk_idx(rel_pos & Vec2<U16>(CHK_SIZE - 1));
        LightLvl light_lvl;
        if(chk_pos != off_pos) {
            light_lvl = get_chunk(off_pos).light_lvl[chk_idx];
        } else {
            light_lvl = chunk.light_lvl[chk_idx];
        }
        verts[i].light = (Vec3<U8>)glm::round(
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
            if(glm::length((Vec3F)verts[i + 1].light) +
               glm::length((Vec3F)verts[i + CHK_SIZE + 1].light) >
               glm::length((Vec3F)verts[i + 0].light) +
               glm::length((Vec3F)verts[i + CHK_SIZE + 2].light)) {
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
