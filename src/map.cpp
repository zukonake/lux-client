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

struct BlockMesh {
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
};

struct Mesh {
    BlockMesh block;
    bool is_built = false;
};

static BlockMesh debug_mesh_0;
static BlockMesh debug_mesh_1;

DynArr<Chunk>  chunks;
DynArr<Mesh>   meshes;
VecSet<ChkPos> chunk_requests;

static ChkCoord render_dist = 4;
static ChkPos   last_player_chk_pos = to_chk_pos(glm::floor(last_player_pos));

static void mesh_destroy(Mesh* mesh);
static bool try_build_mesh(ChkPos const& pos);
static void build_block_mesh(BlockMesh& mesh, ChkPos const& chk_pos);

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
    Arr<BlockMesh::Vert, 4> verts;
    Arr<U32, 6> const&      idxs = quad_idxs<U32>;
    for(Uns i = 0; i < 4; ++i) {
        verts[i].pos = Vec3F(u_quad<F32>[i] * (F32)CHK_SIZE, 0);
        verts[i].layer_tex = u_quad<F32>[i] + (Vec2F)dbg_block_0.tex_pos;;
        verts[i].norm = Vec3F(sqrt(3.f));
    }

    debug_mesh_0.v_buff.bind();
    debug_mesh_0.v_buff.write(4, verts, GL_DYNAMIC_DRAW);
    debug_mesh_0.i_buff.bind();
    debug_mesh_0.i_buff.write(6, idxs, GL_DYNAMIC_DRAW);
    debug_mesh_0.trigs_num = 2;

    gl::VertContext::unbind_all();
    debug_mesh_1.v_buff.init();
    debug_mesh_1.i_buff.init();
    debug_mesh_1.context.init({debug_mesh_0.v_buff}, block_vert_fmt);

    auto const& dbg_block_1 = db_block_bp("dbg_block_1"_l);
    for(Uns i = 0; i < 4; ++i) {
        verts[i].layer_tex = u_quad<F32>[i] + (Vec2F)dbg_block_1.tex_pos;;
    }

    debug_mesh_1.v_buff.bind();
    debug_mesh_1.v_buff.write(4, verts, GL_DYNAMIC_DRAW);
    debug_mesh_1.i_buff.bind();
    debug_mesh_1.i_buff.write(6, idxs, GL_DYNAMIC_DRAW);
    debug_mesh_1.trigs_num = 2;
}

static void map_io_tick(U32, Transform const& tr, IoContext& context) {
    ImGui::Begin("settings");
    static int new_render_dist = render_dist;
    ImGui::InputInt("render_dist", &new_render_dist);
    new_render_dist = glm::max(0, new_render_dist);
    render_dist = new_render_dist;
    ImGui::End();
    auto& world = ui_nodes[ui_world];
    F32 old_ratio = world.tr.scale.x / world.tr.scale.y;
    static F32 bob = 3.6f;
    for(auto const& event : context.scroll_events) {
        //@TODO this shouldn't be exponential/hyperbolic like it is now
        //world.tr.scale.y += event.off * 0.01f;
        bob -= event.off * 0.1f;
        //world.tr.scale.y = glm::clamp(world.tr.scale.y, 1.f / 50.f, 1.f);
        //world.tr.scale.x = world.tr.scale.y * old_ratio;
    }
    context.scroll_events.clear();

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
        SizeT chunks_num = glm::pow(load_size, 3);
        SizeT meshes_num = glm::pow(mesh_load_size, 3);
        DynArr<Chunk> new_chunks(chunks_num);
        //@TODO this crashes sometimes
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
                    new_chunks[get_idx(dst, load_size)] =
                        std::move(chunks[src_idx]);
                    chunks[src_idx].loaded = false;
                }
            }
        }
        //@TODO swap instead (we would have 2 buffers)
        chunks = std::move(new_chunks);
        //@TODO unite loops?
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
                    if(src_idx >= meshes.len || !meshes[src_idx].is_built) {
                        continue;
                    }
                    ChkPos dst = src - chk_diff;
                    new_meshes[get_idx(dst, mesh_load_size)] =
                        std::move(meshes[src_idx]);
                    meshes[src_idx].is_built = false;
                }
            }
        }
        for(Uns i = 0; i < meshes.len; ++i) {
            if(meshes[i].is_built) {
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
    prot += (last_mouse_pos - context.mouse_pos) * 0.005f;
    last_mouse_pos = context.mouse_pos;
    prot.y = clamp(prot.y, -tau / 4.f + .001f, tau / 4.f - .001f);

    glm::mat4 bobo =
        {1, 0, 0, 0,
         0, 0, 1, 0,
         0, 1, 0, 0,
         0, 0, 0, 1};
    Vec3F direction;
    direction.x = glm::cos(prot.y) * glm::cos(-prot.x);
    direction.y = glm::sin(prot.y);
    direction.z = glm::cos(prot.y) * glm::sin(-prot.x);
    direction = glm::normalize(direction);

    bobo = glm::perspective(glm::radians(120.f), 16.f/9.f, 0.1f, 1024.f) *
           glm::lookAt(Vec3F(0.f), direction, Vec3F(0, 1, 0)) * bobo;
    bobo *= 0.1f;


    Vec3F ambient_light =
        glm::mix(Vec3F(0.01f, 0.015f, 0.02f), Vec3F(1.f, 1.f, 0.9f),
                 (ss_tick.day_cycle + 1.f) / 2.f);
    set_uniform("ambient_light", block_program, glUniform3fv,
                1, glm::value_ptr(ambient_light));
    set_uniform("bobo", block_program, glUniformMatrix4fv,
                1, GL_FALSE, glm::value_ptr(bobo));

    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_2D, tileset);
    for(Uns i = 0; i < meshes.len; ++i) {
        Mesh const& mesh        = meshes[i];
        BlockMesh const* b_mesh = &mesh.block;
        ChkPos pos = { i % mesh_load_size,
                      (i / mesh_load_size) % mesh_load_size,
                       i / (mesh_load_size * mesh_load_size)};
        pos -= render_dist;
        pos += chk_pos;
        if(glm::distance((Vec3F)chk_pos, (Vec3F)pos) > render_dist) {
            continue;
        }
        if(!mesh.is_built && !try_build_mesh(pos)) {
            if(!is_chunk_loaded(pos)) b_mesh = &debug_mesh_0;
            else                      b_mesh = &debug_mesh_1;
        }
        if(b_mesh->trigs_num == 0) continue;

        Vec3F chk_translation = (Vec3F)(pos * ChkPos(CHK_SIZE));
        set_uniform("chk_pos", block_program, glUniform3fv, 1,
            glm::value_ptr(chk_translation));

        b_mesh->context.bind();
        b_mesh->i_buff.bind();
        glDrawElements(GL_TRIANGLES, b_mesh->trigs_num * 3, GL_UNSIGNED_INT, 0);
    }
    glDisable(GL_DEPTH_TEST);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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
            ChkIdx idx = block_update.idx;
            chunk.blocks[idx] = block_update.id;
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
            ChkPos off_pos = pos + r_pos;
            Int mesh_idx = get_fov_idx(off_pos, render_dist);
            if(mesh_idx >= 0 && meshes[mesh_idx].is_built &&
               is_chunk_loaded(off_pos)) {
                build_block_mesh(meshes[mesh_idx].block, off_pos);
            }
        };
        //@TODO consider doing this in a loop
        update_mesh({0,  0,  0});
        if(updated_sides != 0) {
            if(updated_sides & 0b000001) {
                update_mesh({-1,  0,  0});
                if(updated_sides & 0b000100) {
                    update_mesh({-1, -1, 0});
                    update_mesh({ 0, -1, 0});
                    if(updated_sides & 0b010000) {
                        update_mesh({-1, -1, -1});
                        update_mesh({ 0, -1, -1});
                        update_mesh({ 0,  0, -1});
                        update_mesh({-1,  0, -1});
                    } else if(updated_sides & 0b100000) {
                        update_mesh({-1, -1,  1});
                        update_mesh({ 0, -1,  1});
                        update_mesh({ 0,  0,  1});
                        update_mesh({-1,  0,  1});
                    }
                }
                else if(updated_sides & 0b001000) {
                    update_mesh({-1,  1, 0});
                    update_mesh({ 0,  1, 0});
                    if(updated_sides & 0b010000) {
                        update_mesh({-1,  1, -1});
                        update_mesh({ 0,  1, -1});
                        update_mesh({ 0,  0, -1});
                        update_mesh({-1,  0, -1});
                    } else if(updated_sides & 0b100000) {
                        update_mesh({-1,  1,  1});
                        update_mesh({ 0,  1,  1});
                        update_mesh({ 0,  0,  1});
                        update_mesh({-1,  0,  1});
                    }
                } else if(updated_sides & 0b010000) {
                    update_mesh({-1,  0, -1});
                    update_mesh({ 0,  0, -1});
                } else if(updated_sides & 0b100000) {
                    update_mesh({-1,  0,  1});
                    update_mesh({ 0,  0,  1});
                }
            } else if(updated_sides & 0b000010) {
                update_mesh({ 1,  0,  0});
                if(updated_sides & 0b000100) {
                    update_mesh({ 1, -1, 0});
                    update_mesh({ 0, -1, 0});
                    if(updated_sides & 0b010000) {
                        update_mesh({ 1, -1, -1});
                        update_mesh({ 0, -1, -1});
                        update_mesh({ 0,  0, -1});
                        update_mesh({ 1,  0, -1});
                    } else if(updated_sides & 0b100000) {
                        update_mesh({ 1, -1,  1});
                        update_mesh({ 0, -1,  1});
                        update_mesh({ 0,  0,  1});
                        update_mesh({ 1,  0,  1});
                    }
                }
                else if(updated_sides & 0b001000) {
                    update_mesh({ 1,  1, 0});
                    update_mesh({ 0,  1, 0});
                    if(updated_sides & 0b010000) {
                        update_mesh({ 1,  1, -1});
                        update_mesh({ 0,  1, -1});
                        update_mesh({ 0,  0, -1});
                        update_mesh({ 1,  0, -1});
                    } else if(updated_sides & 0b100000) {
                        update_mesh({ 1,  1,  1});
                        update_mesh({ 0,  1,  1});
                        update_mesh({ 0,  0,  1});
                        update_mesh({ 1,  0,  1});
                    }
                } else if(updated_sides & 0b010000) {
                    update_mesh({ 1,  0, -1});
                    update_mesh({ 0,  0, -1});
                } else if(updated_sides & 0b100000) {
                    update_mesh({ 1,  0,  1});
                    update_mesh({ 0,  0,  1});
                }
            } else if(updated_sides & 0b000100) {
                update_mesh({ 0, -1, 0});
                if(updated_sides & 0b010000) {
                    update_mesh({ 0, -1, -1});
                    update_mesh({ 0,  0, -1});
                } else if(updated_sides & 0b100000) {
                    update_mesh({ 0, -1,  1});
                    update_mesh({ 0,  0,  1});
                }
            } else if(updated_sides & 0b001000) {
                update_mesh({ 0,  1, 0});
                if(updated_sides & 0b010000) {
                    update_mesh({ 0,  1, -1});
                    update_mesh({ 0,  0, -1});
                } else if(updated_sides & 0b100000) {
                    update_mesh({ 0,  1,  1});
                    update_mesh({ 0,  0,  1});
                }
            } else if(updated_sides & 0b010000) {
                update_mesh({ 0,  0, -1});
            } else if(updated_sides & 0b100000) {
                update_mesh({ 0,  0,  1});
            }
        }
    }
}

static void mesh_destroy(Mesh* mesh) {
    mesh->block.v_buff.deinit();
    mesh->block.i_buff.deinit();
    mesh->block.context.deinit();
    mesh->is_built = true;
}

static bool try_build_mesh(ChkPos const& pos) {
    bool can_build = true;
    for(auto const& offset : chebyshev<ChkCoord>) {
        ChkPos off_pos = pos + offset;
        if(!is_chunk_loaded(off_pos)) {
            chunk_requests.emplace(off_pos);
            ///we don't return here, because we want to request all the chunks
            can_build = false;
        }
    }
    if(!can_build) return false;

    Int idx = get_fov_idx(pos, render_dist);
    LUX_ASSERT(idx >= 0);
    Mesh& mesh = meshes[idx];
    LUX_ASSERT(!mesh.is_built);

    mesh.block.v_buff.init();
    mesh.block.i_buff.init();
    mesh.block.context.init({mesh.block.v_buff}, block_vert_fmt);
    build_block_mesh(mesh.block, pos);

    mesh.is_built = true;
    return true;
}

//@TODO cleanup
typedef struct {
   Vec3F p[3];
} Triangle;

typedef struct {
   Vec3F p[8];
   F32 val[8];
} GridCell;

/*
   Linearly interpolate the position where an isosurface cuts
   an edge between two vertices, each with their own scalar value
*/
static Vec3F VertexInterp(F32 isolevel, Vec3F p1, Vec3F p2, F32 valp1, F32 valp2)
{
   F32 mu;
   Vec3F p;

   if(abs(isolevel-valp1) < 0.00001)
      return(p1);
   if(abs(isolevel-valp2) < 0.00001)
      return(p2);
   if(abs(valp1-valp2) < 0.00001)
      return(p1);
   mu = (isolevel - valp1) / (valp2 - valp1);
   p = p1 + mu * (p2 - p1);

   return p;
}

/*
   Given a grid cell and an isolevel, calculate the triangular
   facets required to represent the isosurface through the cell.
   Return the number of triangular facets, the array "triangles"
   will be loaded up with the vertices at most 5 triangular facets.
	0 will be returned if the grid cell is either totally above
   of totally below the isolevel.
*/
int polygonise(GridCell grid, F32 isolevel, Triangle *triangles)
{
   int i, ntriang;
   int cubeindex;
   Vec3F vertlist[12];

    constexpr U16 edgeTable[256]={
    0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0   };
    constexpr I8 triTable[256][16] =
    {{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
    {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
    {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
    {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
    {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
    {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
    {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
    {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
    {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
    {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
    {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
    {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
    {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
    {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
    {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
    {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
    {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
    {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
    {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
    {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
    {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
    {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
    {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
    {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
    {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
    {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
    {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
    {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
    {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
    {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
    {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
    {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
    {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
    {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
    {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
    {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
    {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
    {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
    {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
    {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
    {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
    {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
    {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
    {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
    {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
    {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
    {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
    {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
    {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
    {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
    {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
    {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
    {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
    {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
    {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
    {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
    {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
    {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
    {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
    {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
    {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
    {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
    {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
    {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
    {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
    {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
    {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
    {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
    {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
    {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
    {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
    {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
    {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
    {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
    {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
    {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
    {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
    {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
    {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
    {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
    {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
    {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
    {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
    {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
    {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
    {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
    {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
    {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
    {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
    {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
    {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
    {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
    {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
    {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
    {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
    {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
    {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
    {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
    {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
    {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
    {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
    {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
    {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
    {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
    {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
    {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
    {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
    {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
    {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
    {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
    {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
    {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
    {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
    {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
    {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
    {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
    {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
    {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
    {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
    {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
    {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
    {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
    {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
    {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
    {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};

   /*
      Determine the index into the edge table which
      tells us which vertices are inside of the surface
   */
   cubeindex = 0;
   if (grid.val[0] < isolevel) cubeindex |= 1;
   if (grid.val[1] < isolevel) cubeindex |= 2;
   if (grid.val[2] < isolevel) cubeindex |= 4;
   if (grid.val[3] < isolevel) cubeindex |= 8;
   if (grid.val[4] < isolevel) cubeindex |= 16;
   if (grid.val[5] < isolevel) cubeindex |= 32;
   if (grid.val[6] < isolevel) cubeindex |= 64;
   if (grid.val[7] < isolevel) cubeindex |= 128;

   /* Cube is entirely in/out of the surface */
   if (edgeTable[cubeindex] == 0)
      return(0);

   /* Find the vertices where the surface intersects the cube */
   if (edgeTable[cubeindex] & 1)
      vertlist[0] =
         VertexInterp(isolevel,grid.p[0],grid.p[1],grid.val[0],grid.val[1]);
   if (edgeTable[cubeindex] & 2)
      vertlist[1] =
         VertexInterp(isolevel,grid.p[1],grid.p[2],grid.val[1],grid.val[2]);
   if (edgeTable[cubeindex] & 4)
      vertlist[2] =
         VertexInterp(isolevel,grid.p[2],grid.p[3],grid.val[2],grid.val[3]);
   if (edgeTable[cubeindex] & 8)
      vertlist[3] =
         VertexInterp(isolevel,grid.p[3],grid.p[0],grid.val[3],grid.val[0]);
   if (edgeTable[cubeindex] & 16)
      vertlist[4] =
         VertexInterp(isolevel,grid.p[4],grid.p[5],grid.val[4],grid.val[5]);
   if (edgeTable[cubeindex] & 32)
      vertlist[5] =
         VertexInterp(isolevel,grid.p[5],grid.p[6],grid.val[5],grid.val[6]);
   if (edgeTable[cubeindex] & 64)
      vertlist[6] =
         VertexInterp(isolevel,grid.p[6],grid.p[7],grid.val[6],grid.val[7]);
   if (edgeTable[cubeindex] & 128)
      vertlist[7] =
         VertexInterp(isolevel,grid.p[7],grid.p[4],grid.val[7],grid.val[4]);
   if (edgeTable[cubeindex] & 256)
      vertlist[8] =
         VertexInterp(isolevel,grid.p[0],grid.p[4],grid.val[0],grid.val[4]);
   if (edgeTable[cubeindex] & 512)
      vertlist[9] =
         VertexInterp(isolevel,grid.p[1],grid.p[5],grid.val[1],grid.val[5]);
   if (edgeTable[cubeindex] & 1024)
      vertlist[10] =
         VertexInterp(isolevel,grid.p[2],grid.p[6],grid.val[2],grid.val[6]);
   if (edgeTable[cubeindex] & 2048)
      vertlist[11] =
         VertexInterp(isolevel,grid.p[3],grid.p[7],grid.val[3],grid.val[7]);

   /* Create the triangle */
   ntriang = 0;
   for (i=0;triTable[cubeindex][i]!=-1;i+=3) {
      triangles[ntriang].p[0] = vertlist[triTable[cubeindex][i  ]];
      triangles[ntriang].p[1] = vertlist[triTable[cubeindex][i+1]];
      triangles[ntriang].p[2] = vertlist[triTable[cubeindex][i+2]];
      ntriang++;
   }

   return(ntriang);
}

static void build_block_mesh(BlockMesh& mesh, ChkPos const& chk_pos) {
    LUX_LOG("building mesh {%zd, %zd, %zd}", chk_pos.x, chk_pos.y, chk_pos.z);
    static DynArr<BlockMesh::Vert> verts(CHK_VOL * 5 * 3);
    static DynArr<U32>             idxs( CHK_VOL * 5 * 3);

    U32 trigs_num = 0;
    /*BitArr<CHK_VOL> face_map;
    MapPos constexpr axis_off[6] =
        {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        BlockId current = chunk.blocks[i];
        MapPos map_pos = to_map_pos(chk_pos, i);
        face_map[i] = false;
        for(MapPos const& off : chebyshev<MapCoord>) {
            face_map[i] = face_map[i] |
               ((get_block(map_pos + off) == void_block) !=
                (current == void_block));
        }
    }*/
    for(ChkIdx i = 0; i < CHK_VOL; ++i) {
        //if(!face_map[i]) continue;
        GridCell grid_cell;
        MapPos map_pos = to_map_pos(chk_pos, i);
        IdxPos idx_pos = to_idx_pos(i);
        constexpr MapPos cell_verts[8] = {
            {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
            {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
        BlockBp const* bp = &db_block_bp("void"_l);
        for(Uns j = 0; j < 8; ++j) {
            MapPos abs_pos = map_pos + cell_verts[j];
            Vec3F rel_pos = (Vec3F)idx_pos + (Vec3F)cell_verts[j];
            grid_cell.p[j] = (Vec3F)rel_pos;
            grid_cell.val[j] = get_block(abs_pos) != void_block;
            if(grid_cell.val[j] > 0.5f) bp = &get_block_bp(abs_pos);
        }
        Triangle cell_trigs[5];
        int cell_trigs_num = polygonise(grid_cell, 0.5f, cell_trigs);
        constexpr Vec2F bob[3] = {{0.001,0.001}, {0.001,0.999}, {0.999,0.001}};
        for(Uns j = 0; j < cell_trigs_num; ++j) {
            for(Uns k = 0; k < 3; ++k) {
                idxs[ trigs_num * 3 + k] = trigs_num * 3 + k;
                verts[trigs_num * 3 + k].pos = cell_trigs[j].p[k] + Vec3F(0.5f);
                verts[trigs_num * 3 + k].layer_tex = (Vec2F)bp->tex_pos + bob[k];
            }
            auto const& v0 = verts[trigs_num * 3 + 0].pos;
            auto const& v1 = verts[trigs_num * 3 + 1].pos;
            auto const& v2 = verts[trigs_num * 3 + 2].pos;
            for(Uns k = 0; k < 3; ++k) {
                verts[trigs_num * 3 + k].norm = glm::cross(v1 - v0, v2 - v0);
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

BlockId get_block(MapPos const& pos) {
    static ChkPos cached_chk_pos = to_chk_pos(pos);
    static Chunk *cached_chunk = &get_chunk(cached_chk_pos);
    ChkPos current_chk_pos = to_chk_pos(pos);
    if(cached_chk_pos != current_chk_pos) {
        cached_chunk   = &get_chunk(current_chk_pos);
        cached_chk_pos = current_chk_pos;
    }
    return cached_chunk->blocks[to_chk_idx(pos)] & 0xFF;
}

BlockBp const& get_block_bp(MapPos const& pos) {
    return db_block_bp(get_block(pos));
}
