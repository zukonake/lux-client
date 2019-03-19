#include <config.hpp>
//
#include <cstring>
//
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/fast_square_root.hpp>
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

static UiId        ui_map;
static gl::VertFmt vert_fmt;
static GLuint      program;
static GLuint      tileset;

struct {
    GLuint      program;
    gl::VertFmt vert_fmt;
} norm_renderer;

struct Mesh {
#pragma pack(push, 1)
    struct Vert {
        Vec3F pos;
        Vec3F norm;
        Vec3<U8> tex_idx; //indices for textures of 3 vertices of that triangle
    };
#pragma pack(pop)

    gl::VertBuff v_buff;
    gl::VertContext context;
    DynArr<Vert> verts;
    bool is_allocated = false;

    void alloc() {
        LUX_ASSERT(not is_allocated);
        v_buff.init();
        context.init({v_buff}, vert_fmt);
        is_allocated = true;
    }

    void dealloc() {
        LUX_ASSERT(is_allocated);
        context.deinit();
        v_buff.deinit();
        is_allocated = false;
    }

    void operator=(Mesh&& that) {
        v_buff  = move(that.v_buff);
        context = move(that.context);
        verts   = move(that.verts);
        is_allocated = move(that.is_allocated);
        that.is_allocated = false;
    }
    Mesh() = default;
    Mesh(Mesh&& that) { *this = move(that); }
};

static Mesh debug_mesh_0;
static Mesh debug_mesh_1;

static DynArr<Mesh>   meshes;
VecSet<ChkPos>        chunk_requests;

static ChkCoord render_dist = 2;
static ChkPos   last_player_chk_pos = to_chk_pos(glm::floor(last_player_pos));

static void map_load_programs() {
    char const* tileset_path = "tileset.png";
    Vec2U const block_size = {8, 8};
    program  = load_program("glsl/block.vert" , "glsl/block.frag");
    Vec2U tileset_size;
    tileset = load_texture(tileset_path, tileset_size);
    Vec2F tex_scale = (Vec2F)block_size / (Vec2F)tileset_size;

    glUseProgram(program);
    set_uniform("tex_scale", program, glUniform2fv,
                1, glm::value_ptr(tex_scale));
    vert_fmt.init(
        {{3, GL_FLOAT, false, false},
         {3, GL_FLOAT, false, false},
         {3, GL_UNSIGNED_BYTE, false, false}});

    norm_renderer.program = load_program("glsl/block_norm.vert" ,
        "glsl/block_norm.frag", "glsl/block_norm.geom");
}

static void map_io_tick(  U32, Transform const&, IoContext&);

void map_init() {
    map_load_programs();

    ui_map = ui_create(ui_camera);
    ui_nodes[ui_map].io_tick = &map_io_tick;

    gl::VertContext::unbind_all();
    debug_mesh_0.alloc();
    debug_mesh_0.context.init({debug_mesh_0.v_buff}, vert_fmt);

    auto const& dbg_block_0 = db_block_bp("dbg_block_0"_l);
    debug_mesh_0.verts.resize(4);
    for(Uns i = 0; i < 4; ++i) {
        debug_mesh_0.verts[i].pos = Vec3F(u_quad<F32>[i] * (F32)CHK_SIZE, 0);
        debug_mesh_0.verts[i].norm = Vec3F(sqrt(3.f));
        //debug_mesh_0.verts[i].tex_i = u_quad<F32>[i] + (Vec2F)dbg_block_0.tex_pos;;
    }

    gl::VertContext::unbind_all();
    debug_mesh_0.v_buff.bind();
    debug_mesh_0.v_buff.write(4, debug_mesh_0.verts.beg, GL_STATIC_DRAW);
}

void map_deinit() {
    for(auto& mesh : meshes) {
        if(mesh.is_allocated) {
            mesh.dealloc();
        }
    }
    meshes.dealloc_all();
    debug_mesh_0.dealloc();
    debug_mesh_1.dealloc();
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
        SizeT meshes_num = glm::pow(mesh_load_size, 3);
        auto get_idx = [&](ChkPos pos, ChkCoord size) {
            return pos.x +
                   pos.y * size +
                   pos.z * size * size;
        };
        //@TODO send unload signal to server
        static DynArr<Mesh> new_meshes;
        for(auto& mesh : new_meshes) { //@TODO temp workaround
            if(mesh.is_allocated) mesh.dealloc();
        }
        new_meshes.resize(meshes_num);
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
                    ChkPos dst = src - chk_diff;
                    SizeT dst_idx = get_idx(dst, mesh_load_size);
                    if(src_idx >= meshes.len ||
                       not meshes[src_idx].is_allocated) {
                        if(dst_idx < new_meshes.len &&
                           new_meshes[dst_idx].is_allocated) {
                            new_meshes[dst_idx].dealloc();
                        }
                        continue;
                    }
                    if(dst_idx >= new_meshes.len) {
                        if(meshes[src_idx].is_allocated) {
                            meshes[src_idx].dealloc();
                        }
                        continue;
                    }
                    new_meshes[dst_idx] = move(meshes[src_idx]);
                }
            }
        }
        swap(meshes, new_meshes);
    }

    last_player_chk_pos = chk_pos;
    last_render_dist    = render_dist;

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
    F32 z_far = (F32)render_dist * (F32)CHK_SIZE;
    //@TODO apect ratio
    mvp = glm::perspective(glm::radians(90.f), 16.f/9.f, 0.1f, z_far) *
          glm::lookAt(camera_pos, camera_pos + direction, Vec3F(0, 1, 0)) * mvp;

    struct DrawData {
        U32 mesh_idx;
        Vec3F pos;
    };
    static DynArr<DrawData> draw_queue;
    draw_queue.clear();
    for(Uns i = 0; i < meshes.len; ++i) {
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
        Mesh* mesh = &meshes[i];
        if(mesh->is_allocated && mesh->verts.len <= 0) continue;

        draw_queue.push({i, pos});
    }
    {
        Vec3F f_pos = chk_pos;
        std::sort(draw_queue.begin(), draw_queue.end(),
            [&](DrawData const& a, DrawData const& b) {
                return glm::fastDistance(a.pos, f_pos) <
                       glm::fastDistance(b.pos, f_pos);
            });
    }
    static bool wireframe = false;
    static bool render_normals = false;
    if(wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    struct {
        U64 chunks_num = 0;
        U64 real_chunks_num = 0;
        U64 trigs_num  = 0;
    } status;
    Vec3F ambient_light =
        glm::mix(Vec3F(0.01f, 0.015f, 0.02f), Vec3F(1.f, 1.f, 0.9f),
                 (ss_tick.day_cycle + 1.f) / 2.f);

    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);
    glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D, tileset);
    set_uniform("time", program, glUniform1f, glfwGetTime());
    set_uniform("ambient_light", program, glUniform3fv,
                1, glm::value_ptr(ambient_light));
    set_uniform("mvp", program, glUniformMatrix4fv,
                1, GL_FALSE, glm::value_ptr(mvp));
    glUseProgram(norm_renderer.program);
    set_uniform("mvp", norm_renderer.program, glUniformMatrix4fv,
                1, GL_FALSE, glm::value_ptr(mvp));

    for(auto const& draw_data : draw_queue) {
        Mesh* mesh = &meshes[draw_data.mesh_idx];
        auto const& pos = draw_data.pos;
        if(not mesh->is_allocated) {
            chunk_requests.emplace(pos);
            mesh = &debug_mesh_0;
        }
        if(mesh != &debug_mesh_0 && mesh != &debug_mesh_1) {
            status.real_chunks_num++;
        }
        status.chunks_num++;
        status.trigs_num += mesh->verts.len / 3;
        Vec3F chk_translation = pos * (F32)CHK_SIZE;

        mesh->context.bind();
        glUseProgram(program);
        set_uniform("chk_pos", program, glUniform3fv, 1,
            glm::value_ptr(chk_translation));
        //@TODO multi draw
        glDrawArrays(GL_TRIANGLES, 0, mesh->verts.len);
        if(render_normals) {
            glUseProgram(norm_renderer.program);
            set_uniform("chk_pos", norm_renderer.program, glUniform3fv, 1,
                glm::value_ptr(chk_translation));
            glDrawArrays(GL_TRIANGLES, 0, mesh->verts.len);
        }
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
    if(ImGui::Button("toggle normals")) {
        render_normals = !render_normals;
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
    glDeleteProgram(program);
    map_load_programs();
}

///returns -1 if out of bounds
static Int get_fov_idx(ChkPos const& pos, ChkCoord dist) {
    ChkPos idx_pos = (pos - last_player_chk_pos) + dist;
    ChkCoord size = dist * 2 + 1;
    if(clamp(idx_pos, ChkPos(0), ChkPos(size)) != idx_pos) return -1;
    Int idx = idx_pos.x + idx_pos.y * size + idx_pos.z * size * size;
    return idx;
}

void map_load_chunks(NetSsSgnl::ChunkLoad const& net_chunks) {
    for(auto const& pair : net_chunks.chunks) {
        ChkPos chk_pos = pair.first;
        auto const& net_chunk = pair.second;
        LUX_LOG("loading chunk {%zd, %zd, %zd}", chk_pos.x, chk_pos.y, chk_pos.z);
        Int idx = get_fov_idx(chk_pos, render_dist);
        if(idx < 0) {
            LUX_LOG_WARN("received chunk {%zd, %zd, %zd} out of load range",
                chk_pos.x, chk_pos.y, chk_pos.z);
            continue;
        }
        if(meshes[idx].is_allocated) {
            LUX_LOG_WARN("received chunk {%zd, %zd, %zd} alread loaded",
                chk_pos.x, chk_pos.y, chk_pos.z);
            continue;
        }
        Mesh& mesh = meshes[idx];
        //if(net_chunk.idxs.len <= 0) continue;
        mesh.verts.resize(net_chunk.idxs.len);
        static DynArr<U8> ids;
        ids.resize(mesh.verts.len);
        for(Uns i = 0; i < net_chunk.idxs.len; ++i) {
            auto idx = net_chunk.idxs[i];
            auto const& n_vert = net_chunk.verts[idx];
            auto& vert = mesh.verts[i];
            vert.pos  = fixed_to_float<4, U16, 3>(n_vert.pos);
            vert.norm = (Vec3F)(n_vert.norm & (U8)0x7f) / Vec3F(0x7f);
            for(Uns j = 0; j < 3; ++j) {
                if(n_vert.norm[j] & (U8)0x80) {
                    vert.norm[j] = -vert.norm[j];
                }
            }
            //@TODO make sure normals are normalized
            ids[i] = n_vert.id;
        }
        for(Uns i = 0; i < mesh.verts.len; i += 3) {
            auto idxs = Vec3<U8>(ids[i + 0], ids[i + 1], ids[i + 2]);
            for(Uns j = 0; j < 3; ++j) {
                mesh.verts[i + j].tex_idx = idxs;
            }
        }
        mesh.alloc();
        mesh.v_buff.bind();
        mesh.v_buff.write(mesh.verts.len, mesh.verts.beg, GL_DYNAMIC_DRAW);
        chunk_requests.erase(chk_pos);
    }
    LUX_UNIMPLEMENTED();
}

/*void map_update_chunks(NetSsSgnl::ChunkUpdate const& net_chunks) {
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
}*/

