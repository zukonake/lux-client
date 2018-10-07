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
#include "map.hpp"

GLuint program;
GLuint vbo;
GLuint vao;
GLuint tex;

VecMap<ChkPos, Chunk>     chunks;
VecSet<ChkPos>            chunk_requests;

Vec3F screen[800 * 600];

static void map_load_programs() {
    program = load_program("glsl/map.vert", "glsl/map.frag");
    glGenTextures(1, &tex);
}

void map_init() {
    map_load_programs();

    Vec2F verts[6][2] =
        {{{-1.f, -1.f}, {0.f, 0.f}},
         {{ 1.f, -1.f}, {1.f, 0.f}},
         {{-1.f,  1.f}, {0.f, 1.f}},
         {{-1.f,  1.f}, {0.f, 1.f}},
         {{ 1.f,  1.f}, {1.f, 1.f}},
         {{ 1.f, -1.f}, {1.f, 0.f}}};

    glGenBuffers(1, &vbo);

    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vec2F) * 12, verts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
        sizeof(Vec2F) * 2, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
        sizeof(Vec2F) * 2, (void*)sizeof(Vec2F));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
}

static void cast_ray(Vec2I src, Vec2I dst, Vec2I off, Int z) {
    Vec2I ray = dst - src;
    F32 steps = glm::compMax(glm::abs(ray));
    Vec2F step = (Vec2F)ray / steps;
    Vec2F it = (Vec2F)(src - off);
    bool hit = false;
    Uns c = 0;
    Vec3F col(0.f);
    bool map_stat[(800 / 64) * (600 / 64)] = {false};
    while(c < steps) {
        Vec2I t_it = (Vec2I)it;
        Vec2I h_map_pos = (t_it - off % 64l) / 64l;
        if(!hit && !map_stat[h_map_pos.x + h_map_pos.y * (800 / 64)]) {
            MapPos map_pos = MapPos((t_it - off) / 64l, z);
            ChkPos chk_pos = to_chk_pos(map_pos);
            if(is_chunk_loaded(chk_pos)) {
                if(db_voxel_type(get_chunk(chk_pos).voxels[to_chk_idx(map_pos)]).shape == VoxelType::BLOCK) {
                    col = Vec3F(0.f);
                    hit = true;
                } else {
                    col = Vec3F(1.f);
                }
            }
            else chunk_requests.emplace(chk_pos);
            map_stat[h_map_pos.x + h_map_pos.y * (800 / 64)] = true;
        }
        screen[t_it.x + t_it.y * 800] = col;
        it += step;
        ++c;
    }
}

void map_render(EntityVec const& player_pos) {
    Vec2I off = (Vec2I)(player_pos * 64.f) * Vec2I(1, -1);
    Vec2I src = Vec2I(400, 300) + off;
    Vec2I dst = off;
    for(Uns i = 0; i < 800; ++i) {
        ++dst.x;
        cast_ray(src, dst, off, player_pos.z);
    }
    dst = off;
    for(Uns i = 0; i < 600; ++i) {
        ++dst.y;
        cast_ray(src, dst, off, player_pos.z);
    }

    glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB, GL_FLOAT, screen);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void map_reload_program() {
    LUX_LOG("reloading map program");
    glDeleteTextures(1, &tex);
    glDeleteProgram(program);
    map_load_programs();
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
}

Chunk const& get_chunk(ChkPos const& pos) {
    LUX_ASSERT(is_chunk_loaded(pos));
    return chunks.at(pos);
}
