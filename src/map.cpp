#include <config.hpp>
//
#include <cstring>
//
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/rotate_vector.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>
#include <lux_shared/util/packer.hpp>
#include <lux_shared/marching_cubes.hpp>
//
#include <imgui/imgui.h>
#include <rendering.hpp>
#include <db.hpp>
#include <client.hpp>
#include <ui.hpp>
#include "map.hpp"

UiId ui_map;

GLuint block_program;
GLuint tileset;

gl::VertFmt block_vert_fmt;

struct Mesh {
#pragma pack(push, 1)
    struct Vert {
        Vec3F pos;
        Vec2F layer_tex;
        Vec3F norm;
    };
#pragma pack(pop)
    gl::VertBuff    v_buff;
    gl::IdxBuff     i_buff;
    gl::VertContext context;
    U32             trigs_num;
    enum State {
        NOT_ALLOCATED,
        NEEDS_REBUILD,
        BUILT
    } state = NOT_ALLOCATED;
};

static Mesh debug_mesh_0;
static Mesh debug_mesh_1;

DynArr<Chunk>  chunks;
DynArr<Mesh>   meshes;
VecSet<ChkPos> chunk_requests;

static ChkCoord render_dist = 2;
static ChkPos   last_player_chk_pos = to_chk_pos(glm::floor(last_player_pos));

static void mesh_destroy(Mesh* mesh);
static bool try_guarantee_chunks_for_mesh(ChkPos const& pos);
static bool try_build_mesh(Mesh& mesh, ChkPos const& pos);
static void build_mesh(Mesh& mesh, ChkPos const& pos);

static void map_load_programs() {
    char const* tileset_path = "tileset.png";
    Vec2U const block_size = {8, 8};
    block_program  = load_program("glsl/block.vert" , "glsl/block.frag");
    Vec2U tileset_size;
    tileset = load_texture(tileset_path, tileset_size);
    Vec2F tex_scale = (Vec2F)block_size / (Vec2F)tileset_size;

    glUseProgram(block_program);
    set_uniform("tex_scale", block_program, glUniform2fv,
                1, glm::value_ptr(tex_scale));
    block_vert_fmt.init(block_program,
        {{"pos"    , 3, GL_FLOAT, false, false},
         {"tex_pos", 2, GL_FLOAT, false, false},
         {"norm"   , 3, GL_FLOAT, false, false}});
}

static void map_io_tick(  U32, Transform const&, IoContext&);

void map_init() {
    map_load_programs();

    ui_map = ui_create(ui_camera);
    ui_nodes[ui_map].io_tick = &map_io_tick;

    gl::VertContext::unbind_all();
    debug_mesh_0.v_buff.init();
    debug_mesh_0.i_buff.init();
    debug_mesh_0.context.init({debug_mesh_0.v_buff}, block_vert_fmt);

    auto const& dbg_block_0 = db_block_bp("dbg_block_0"_l);
    Arr<Mesh::Vert, 4> verts;
    Arr<U32, 12> idxs;
    for(Uns i = 0; i < 4; ++i) {
        verts[i].pos = Vec3F(u_quad<F32>[i] * (F32)CHK_SIZE, 0);
        verts[i].layer_tex = u_quad<F32>[i] + (Vec2F)dbg_block_0.tex_pos;;
        verts[i].norm = Vec3F(sqrt(3.f));
    }
    for(Uns i = 0; i < 6; ++i) {
        idxs[i] = quad_idxs<U32>[i];
    }
    for(Uns i = 0; i < 6; ++i) {
        idxs[6 + i] = quad_idxs<U32>[5 - i];
    }

    debug_mesh_0.v_buff.bind();
    debug_mesh_0.v_buff.write(4, verts, GL_DYNAMIC_DRAW);
    debug_mesh_0.i_buff.bind();
    debug_mesh_0.i_buff.write(12, idxs, GL_DYNAMIC_DRAW);
    debug_mesh_0.trigs_num = 4;
    debug_mesh_0.state = Mesh::BUILT;

    gl::VertContext::unbind_all();
    debug_mesh_1.v_buff.init();
    debug_mesh_1.i_buff.init();
    debug_mesh_1.context.init({debug_mesh_1.v_buff}, block_vert_fmt);

    auto const& dbg_block_1 = db_block_bp("dbg_block_1"_l);
    for(Uns i = 0; i < 4; ++i) {
        verts[i].layer_tex = u_quad<F32>[i] + (Vec2F)dbg_block_1.tex_pos;;
    }

    debug_mesh_1.v_buff.bind();
    debug_mesh_1.v_buff.write(4, verts, GL_DYNAMIC_DRAW);
    debug_mesh_1.i_buff.bind();
    debug_mesh_1.i_buff.write(12, idxs, GL_DYNAMIC_DRAW);
    debug_mesh_1.trigs_num = 4;
    debug_mesh_1.state = Mesh::BUILT;
}

static void map_io_tick(U32, Transform const& tr, IoContext& context) {
    auto& world = ui_nodes[ui_world];
    F32 old_ratio = world.tr.scale.x / world.tr.scale.y;
    static F32 bob = 1.4f;
    for(auto const& event : context.key_events) {
        if(event.key == GLFW_KEY_1 && event.action == GLFW_PRESS) {
            render_dist++;
        } else if(event.key == GLFW_KEY_2 && event.action == GLFW_PRESS &&
                  render_dist > 1) {
            render_dist--;
        }
    }
    for(auto const& event : context.scroll_events) {
        //@TODO this shouldn't be exponential/hyperbolic like it is now
        //world.tr.scale.y += event.off * 0.01f;
        bob -= event.off * 1.f;
        //world.tr.scale.y = glm::clamp(world.tr.scale.y, 1.f / 50.f, 1.f);
        //world.tr.scale.x = world.tr.scale.y * old_ratio;
    }
    context.scroll_events.clear();
    for(Uns i = 0; i < context.mouse_events.len; ++i) {
        //@TODO erase event
        auto const& event = context.mouse_events[i];
        if(event.button == GLFW_MOUSE_BUTTON_LEFT &&
           event.action == GLFW_PRESS) {
            ui_do_action("entity_break_block"_l, {nullptr, 0});
        } else if(event.button == GLFW_MOUSE_BUTTON_RIGHT &&
           event.action == GLFW_PRESS) {
            ui_do_action("entity_place_block"_l, {nullptr, 0});
        }
    }

    EntityVec camera_pos = -ui_nodes[ui_camera].tr.pos;
    camera_pos.z += bob;

    ChkCoord load_size      = 2 * (render_dist + 1) + 1;
    ChkCoord mesh_load_size = 2 * render_dist + 1;

    ChkPos chk_pos = to_chk_pos(glm::floor(camera_pos));

    static ChkCoord last_render_dist = 0;

    ChkCoord last_load_size      = 2 * (last_render_dist + 1) + 1;
    ChkCoord last_mesh_load_size = 2 * last_render_dist + 1;

    ChkPos chk_diff           = chk_pos - last_player_chk_pos;
    ChkCoord render_dist_diff = render_dist - last_render_dist;
    chk_diff -= render_dist_diff;
    if(chk_diff != ChkPos(0)) {
        //@TODO toroidal addresing
        SizeT chunks_num = glm::pow(load_size, 3);
        SizeT meshes_num = glm::pow(mesh_load_size, 3);
        DynArr<Chunk> new_chunks(chunks_num);
        auto get_idx = [&](ChkPos pos, ChkCoord size) {
            return pos.x +
                   pos.y * size +
                   pos.z * size * size;
        };
        for(ChkCoord z =  chk_diff.z > 0 ? chk_diff.z : 0;
                     z < (chk_diff.z < 0 ? chk_diff.z : 0) + last_load_size;
                   ++z) {
            for(ChkCoord y =  chk_diff.y > 0 ? chk_diff.y : 0;
                         y < (chk_diff.y < 0 ? chk_diff.y : 0) + last_load_size;
                       ++y) {
                for(ChkCoord x =  chk_diff.x > 0 ? chk_diff.x : 0;
                             x < (chk_diff.x < 0 ? chk_diff.x : 0) + last_load_size;
                           ++x) {
                    ChkPos src(x, y, z);
                    SizeT src_idx = get_idx(src, last_load_size);
                    if(src_idx >= chunks.len || !chunks[src_idx].loaded) {
                        continue;
                    }
                    ChkPos dst = src - chk_diff;
                    SizeT dst_idx = get_idx(dst, load_size);
                    if(dst_idx >= new_chunks.len) {
                        continue;
                    }
                    new_chunks[dst_idx] = std::move(chunks[src_idx]);
                    chunks[src_idx].loaded = false;
                }
            }
        }
        //@TODO dtor (needed for exit)
        //@TODO unload chunks 
        //@TODO swap instead (we would have 2 buffers)
        chunks = std::move(new_chunks);
        //@TODO unite loops?
        //we could do 8 loops for each outer edge, and then do the interior
        //in a single loop for both meshes and chunks
        DynArr<Mesh> new_meshes(meshes_num);
        for(ChkCoord z =  chk_diff.z > 0 ? chk_diff.z : 0;
                     z < (chk_diff.z < 0 ? chk_diff.z : 0) + last_mesh_load_size;
                   ++z) {
            for(ChkCoord y =  chk_diff.y > 0 ? chk_diff.y : 0;
                         y < (chk_diff.y < 0 ? chk_diff.y : 0) + last_mesh_load_size;
                       ++y) {
                for(ChkCoord x =  chk_diff.x > 0 ? chk_diff.x : 0;
                             x < (chk_diff.x < 0 ? chk_diff.x : 0) + last_mesh_load_size;
                           ++x) {
                    ChkPos src(x, y, z);
                    SizeT src_idx = get_idx(src, last_mesh_load_size);
                    if(src_idx >= meshes.len ||
                       meshes[src_idx].state == Mesh::NOT_ALLOCATED) {
                        continue;
                    }
                    ChkPos dst = src - chk_diff;
                    SizeT dst_idx = get_idx(dst, mesh_load_size);
                    if(dst_idx >= new_meshes.len) {
                        continue;
                    }
                    new_meshes[dst_idx] = std::move(meshes[src_idx]);
                    meshes[src_idx].state = Mesh::NOT_ALLOCATED;
                }
            }
        }
        for(Uns i = 0; i < meshes.len; ++i) {
            if(meshes[i].state != Mesh::NOT_ALLOCATED) {
                mesh_destroy(&meshes[i]);
            }
        }
        //@TODO swap instead (we would have 2 buffers)
        meshes = std::move(new_meshes);
    }

    last_player_chk_pos = chk_pos;
    last_render_dist    = render_dist;

    glUseProgram(block_program);
    set_uniform("scale", block_program, glUniform3fv,
                1, glm::value_ptr(tr.scale));
    set_uniform("camera_pos", block_program, glUniform3fv,
                1, glm::value_ptr(camera_pos));
    //@TODO
    static Vec2F last_mouse_pos = {0, 0};
    static Vec2F prot = {0, 0};
    Vec2F diff = (last_mouse_pos - context.mouse_pos) * 0.005f;
    last_mouse_pos = context.mouse_pos;
    prot += diff;
    prot.x = s_mod_clamp(prot.x, tau / 2.f);
    prot.y = clamp(prot.y, -tau / 4.f + .001f, tau / 4.f - .001f);
    Vec2F s_prot = {s_mod_clamp((prot.x + tau / 4.f) / (-tau / 2.f), 1.f),
                    prot.y / (tau / 2.f)};
    U8 stack[2];
    stack[1] = (I8)clamp(s_prot.x * 128.f, -128.f, 127.f);
    stack[0] = (I8)clamp(s_prot.y * 128.f, -128.f, 127.f);
    ui_do_action("entity_rotate"_l, stack);

    glm::mat4 mvp =
        {1, 0, 0, 0,
         0, 0, 1, 0,
         0, 1, 0, 0,
         0, 0, 0, 1};
    Vec3F direction;
    direction.x = glm::cos(prot.y) * glm::cos(-prot.x);
    direction.y = glm::sin(prot.y);
    direction.z = glm::cos(prot.y) * glm::sin(-prot.x);
    direction = glm::normalize(direction);

    F32 temp = camera_pos.z;
    camera_pos.z = camera_pos.y;
    camera_pos.y = temp;
    //@TODO Z_FAR
    mvp = glm::perspective(glm::radians(90.f), 16.f/9.f, 0.1f, 1024.f) *
          glm::lookAt(camera_pos, camera_pos + direction, Vec3F(0, 0.1, 0)) * mvp;

    Vec3F ambient_light =
        glm::mix(Vec3F(0.01f, 0.015f, 0.02f), Vec3F(1.f, 1.f, 0.9f),
                 (ss_tick.day_cycle + 1.f) / 2.f);
    set_uniform("ambient_light", block_program, glUniform3fv,
                1, glm::value_ptr(ambient_light));
    set_uniform("mvp", block_program, glUniformMatrix4fv,
                1, GL_FALSE, glm::value_ptr(mvp));

    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);

    static bool wireframe = false;
    if(wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_2D, tileset);
    struct {
        U64 chunks_num = 0;
        U64 real_chunks_num = 0;
        U64 trigs_num  = 0;
    } status;
    U32 constexpr max_mesh_builds_per_frame = 16;
    U32 meshes_built = 0;
    for(Uns i = 0; i < meshes.len; ++i) {
        Mesh* mesh = &meshes[i];
        ChkPos pos = { i % mesh_load_size,
                      (i / mesh_load_size) % mesh_load_size,
                       i / (mesh_load_size * mesh_load_size)};
        pos -= render_dist;
        pos += chk_pos;
        if(glm::distance((Vec3F)chk_pos, (Vec3F)pos) > render_dist) {
            continue;
        }
        Vec3F center = (Vec3F)pos * (Vec3F)CHK_SIZE + (Vec3F)(CHK_SIZE / 2);
        Vec4F c_center = mvp * Vec4F(center, 1.f);
        c_center.x /= c_center.w;
        c_center.y /= c_center.w;
        //@TODO in rare cases this is too small
        F32 constexpr chk_rad = glm::sqrt(CHK_SIZE * CHK_SIZE * 3) / 2.f;
        if(c_center.z < -chk_rad) continue;
        F32 c_chk_rad = chk_rad / glm::abs(c_center.w);
        if(glm::abs(c_center.x) > 1.f + c_chk_rad ||
           glm::abs(c_center.y) > 1.f + c_chk_rad) continue;
        if(mesh->state != Mesh::BUILT) {
            if(meshes_built < max_mesh_builds_per_frame &&
               try_build_mesh(*mesh, pos)) {
                meshes_built++;
            } else if(is_chunk_loaded(pos)) {
                mesh = &debug_mesh_1;
            } else mesh = &debug_mesh_0;
        }
        if(mesh->trigs_num <= 0) continue;

        if(mesh != &debug_mesh_0 && mesh != &debug_mesh_1) {
            status.real_chunks_num++;
        }
        status.chunks_num++;
        status.trigs_num += mesh->trigs_num;
        Vec3F chk_translation = (Vec3F)(pos * ChkPos(CHK_SIZE));
        set_uniform("chk_pos", block_program, glUniform3fv, 1,
            glm::value_ptr(chk_translation));

        mesh->context.bind();
        mesh->i_buff.bind();
        glDrawElements(GL_TRIANGLES, mesh->trigs_num * 3, GL_UNSIGNED_INT, 0);
    }
    glDisable(GL_DEPTH_TEST);
    if(wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    static F64 avg = 0.0;
    static U32 denom = 0;
    static F64 timer = glfwGetTime();
    F64 now = glfwGetTime();
    F64 delta = now - timer;
    timer = now;

    SizeT constexpr plot_sz = 512;
    static F32 last_delta[plot_sz] = {0.0};
    for(Uns i = 0; i < plot_sz - 1; ++i) {
        last_delta[i] = last_delta[i + 1];
    }
    last_delta[plot_sz - 1] = (F32)(delta * 1000.0);

    avg += delta;
    denom++;
    static F64 last_avg = 0.0;
    if(denom >= 8) {
        last_avg = avg / (F64)denom;
        avg = 0.0;
        denom = 0;
    }
    F64 fps = 1.0 / last_avg;

    ImGui::Begin("render status");
    ImGui::Text("delta: %.2F", last_avg * 1000.0);
    ImGui::PlotHistogram("", last_delta, plot_sz, 0,
                         nullptr, 0.f, FLT_MAX, {200, 80});
    if(ImGui::Button("wireframe mode")) {
        wireframe = !wireframe;
    }
    ImGui::Text("fps: %d", (int)fps);
    ImGui::Text("render dist: %d", render_dist);
    ImGui::Text("pending chunks num: %zu", chunk_requests.size());
    ImGui::Text("chunks num: %zu", status.chunks_num);
    ImGui::Text("real chunks num: %zu", status.real_chunks_num);
    ImGui::Text("trigs num: %zu", status.trigs_num);
    ImGui::End();
}

void map_reload_program() {
    //@TODO we need to reload attrib locations here
    LUX_LOG("reloading map program");
    glDeleteTextures(1, &tileset);
    glDeleteProgram(block_program);
    map_load_programs();
}

///returns -1 if out of bounds
static Int get_fov_idx(ChkPos const& pos, ChkCoord dist) {
    ChkPos idx_pos = (pos - last_player_chk_pos) + dist;
    ChkCoord size = dist * 2 + 1;
    for(Uns i = 0; i < 3; ++i) {
        if(idx_pos[i] < 0 || idx_pos[i] >= size) {
            return -1;
        }
    }
    Int idx = idx_pos.x + idx_pos.y * size + idx_pos.z * size * size;
    return idx;
}

bool is_chunk_loaded(ChkPos const& pos) {
    Int idx = get_fov_idx(pos, render_dist + 1);
    if(idx < 0) return false;
    return chunks[idx].loaded;
}

Chunk& get_chunk(ChkPos const& pos) {
    Int idx = get_fov_idx(pos, render_dist + 1);
    LUX_ASSERT(idx >= 0);
    return chunks[idx];
}

static Mesh& get_mesh(ChkPos const& pos) {
    Int idx = get_fov_idx(pos, render_dist);
    LUX_ASSERT(idx >= 0);
    return meshes[idx];
}

void map_load_chunks(NetSsSgnl::ChunkLoad const& net_chunks) {
    for(auto const& pair : net_chunks.chunks) {
        ChkPos pos = pair.first;
        auto const& net_chunk = pair.second;
        LUX_LOG("loading chunk {%zd, %zd, %zd}", pos.x, pos.y, pos.z);
        Int idx = get_fov_idx(pos, render_dist + 1);
        if(idx < 0) {
            LUX_LOG_WARN("received chunk {%zd, %zd, %zd} out of load range",
                pos.x, pos.y, pos.z);
            continue;
        }
        Chunk& chunk = chunks[idx];
        std::memcpy(chunk.blocks, net_chunk.blocks, sizeof(chunk.blocks));
        chunk.loaded = true;
        chunk_requests.erase(pos);
    }
}

void map_update_chunks(NetSsSgnl::ChunkUpdate const& net_chunks) {
    for(auto const& pair : net_chunks.chunks) {
        U8 updated_sides = 0b000000;
        ChkPos pos = pair.first;
        auto const& net_chunk = pair.second;
        LUX_LOG("updating chunk {%zd, %zd, %zd}", pos.x, pos.y, pos.z);
        Int idx = get_fov_idx(pos, render_dist + 1);
        if(idx < 0 || !chunks[idx].loaded) {
            LUX_LOG_WARN("received chunk update for unloaded chunk"
                " {%zd, %zd, %zd}", pos.x, pos.y, pos.z);
            continue;
        }
        Chunk& chunk = chunks[idx];
        for(auto const& block_update : net_chunk.blocks) {
            //@TODO bound check?
            ChkIdx idx = block_update.first;
            chunk.blocks[idx].id  = block_update.second.id;
            chunk.blocks[idx].lvl = block_update.second.lvl;
            IdxPos idx_pos = to_idx_pos(idx);
            for(Uns a = 0; a < 3; ++a) {
                if(idx_pos[a] == 0) {
                    updated_sides |= (0b000001 << (a * 2));
                } else if(idx_pos[a] == CHK_SIZE - 1) {
                    updated_sides |= (0b000010 << (a * 2));
                }
            }
        }
        auto update_mesh = [&](ChkPos r_pos) {
            Int mesh_idx = get_fov_idx(r_pos + pos, render_dist);
            if(mesh_idx >= 0 && meshes[mesh_idx].state == Mesh::BUILT) {
                meshes[mesh_idx].state = Mesh::NEEDS_REBUILD;
            }
        };
        update_chunks_around(update_mesh, updated_sides);
    }
}

static bool try_build_mesh(Mesh& mesh, ChkPos const& pos) {
    LUX_ASSERT(mesh.state != Mesh::BUILT);
    if(mesh.state == Mesh::NOT_ALLOCATED) {
        if(!try_guarantee_chunks_for_mesh(pos)) {
            return false;
        }
        mesh.v_buff.init();
        mesh.i_buff.init();
        mesh.context.init({mesh.v_buff}, block_vert_fmt);
    }
    build_mesh(mesh, pos);
    mesh.state = Mesh::BUILT;
    return true;
}

static void mesh_destroy(Mesh* mesh) {
    mesh->v_buff.deinit();
    mesh->i_buff.deinit();
    mesh->context.deinit();
    mesh->state = Mesh::NOT_ALLOCATED;
}

static bool try_guarantee_chunks_for_mesh(ChkPos const& pos) {
    bool can_build = true;
    for(auto const& offset : chebyshev<ChkCoord>) {
        ChkPos off_pos = pos + offset;
        if(!is_chunk_loaded(off_pos)) {
            chunk_requests.emplace(off_pos);
            ///we don't return here, because we want to request all the chunks
            can_build = false;
        }
    }
    return can_build;
}

static void build_mesh(Mesh& mesh, ChkPos const& chk_pos) {
    LUX_LOG("building mesh {%zd, %zd, %zd}", chk_pos.x, chk_pos.y, chk_pos.z);
    static DynArr<Mesh::Vert> verts(CHK_VOL * 5 * 3);
    static DynArr<U32>        idxs( CHK_VOL * 5 * 3);

    U32 trigs_num = 0;
    MapPos constexpr axis_off[6] =
        {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    Chunk const& chunk = get_chunk(chk_pos);
    static BlockBp const& void_bp = db_block_bp("void"_l);

    constexpr MapPos cell_verts[8] = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
    GridCell grid_cell;
    for(Uns i = 0; i < 8; ++i) {
        grid_cell.p[i] = (Vec3F)cell_verts[i];
    }
    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        MapPos map_pos = to_map_pos(chk_pos, i);
        Vec3F rel_pos = (Vec3F)to_idx_pos(i) + 0.5f;
        BlockBp const* bp = &void_bp;
        int sign = s_norm((F32)get_block(map_pos).lvl / 15.f);
        bool has_face = false;
        F32 max_lvl = -1.f;
        for(Uns j = 0; j < 8; ++j) {
            MapPos abs_pos = map_pos + cell_verts[j];
            BlockLvl lvl = get_block(abs_pos).lvl;
            grid_cell.val[j] = s_norm((F32)lvl / 15.f);
            if(grid_cell.val[j] >= max_lvl) {
                bp = &get_block_bp(abs_pos);
                max_lvl = grid_cell.val[j];
            }
            if((grid_cell.val[j] >= 0.f) != sign) {
                has_face = true;
            }
        }
        if(!has_face) continue;
        Triangle cell_trigs[5];
        int cell_trigs_num = polygonise(grid_cell, 0.f, cell_trigs);
        for(Uns j = 0; j < cell_trigs_num; ++j) {
            for(Uns k = 0; k < 3; ++k) {
                idxs[ trigs_num * 3 + k] = trigs_num * 3 + k;
                verts[trigs_num * 3 + k].pos =
                    cell_trigs[j].p[k] + rel_pos; 
            }
            auto const& v0 = verts[trigs_num * 3 + 0].pos;
            auto const& v1 = verts[trigs_num * 3 + 1].pos;
            auto const& v2 = verts[trigs_num * 3 + 2].pos;
            for(Uns k = 0; k < 3; ++k) {
                Vec3F norm = glm::cross(v1 - v0, v2 - v0);
                verts[trigs_num * 3 + k].norm = norm;
                norm = glm::abs(norm);
                F32 max = glm::compMax(norm);
                if(max == norm.x) {
                    verts[trigs_num * 3 + k].layer_tex =
                        (Vec2F)bp->tex_pos + Vec2F(cell_trigs[j].p[k].y,
                                                   cell_trigs[j].p[k].z);
                } else if(max == norm.y) {
                    verts[trigs_num * 3 + k].layer_tex =
                        (Vec2F)bp->tex_pos + Vec2F(cell_trigs[j].p[k].x,
                                                   cell_trigs[j].p[k].z);
                } else if(max == norm.z) {
                    verts[trigs_num * 3 + k].layer_tex =
                        (Vec2F)bp->tex_pos + Vec2F(cell_trigs[j].p[k].x,
                                                   cell_trigs[j].p[k].y);
                }
            }
            ++trigs_num;
        }
    }
    if(trigs_num != 0) {
        gl::VertContext::unbind_all();
        mesh.v_buff.bind();
        mesh.v_buff.write(trigs_num * 3, verts.beg, GL_DYNAMIC_DRAW);
        mesh.i_buff.bind();
        mesh.i_buff.write(trigs_num * 3, idxs.beg, GL_DYNAMIC_DRAW);
        mesh.trigs_num = trigs_num;
    }
}

Block get_block(MapPos const& pos) {
    return get_chunk(to_chk_pos(pos)).blocks[to_chk_idx(pos)];
}

BlockBp const& get_block_bp(MapPos const& pos) {
    return db_block_bp(get_block(pos).id);
}
