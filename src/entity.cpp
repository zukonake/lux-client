#include <config.hpp>
//
#include <glm/gtc/type_ptr.hpp>
//
#include <rendering.hpp>
#include <client.hpp>
#include <viewport.hpp>
#include <ui.hpp>
#include "entity.hpp"

static GLuint program;

#pragma pack(push, 1)
struct Vert {
    typedef U32 Idx;
    static constexpr GLenum IDX_GL_TYPE = GL_UNSIGNED_INT;
    Vec2F pos;
};
#pragma pack(pop)

static GLuint vbo;
static GLuint ebo;
#if defined(LUX_GL_3_3)
static GLuint vao;
#endif

struct {
    GLint pos;
} static shader_attribs;

void entity_init() {
    program = load_program("glsl/entity.vert", "glsl/entity.frag");

    glUseProgram(program);
    shader_attribs.pos = glGetAttribLocation(program, "pos");

    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

#if defined(LUX_GL_3_3)
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(Vert),
        (void*)offsetof(Vert, pos));
    glEnableVertexAttribArray(shader_attribs.pos);
#endif
}

static HashTable<TextHandle, EntityHandle> entity_names;

void entity_render() {
    auto const& player_pos = last_player_pos;
    auto const& comps = ss_tick.comps;

    glUseProgram(program);
    set_uniform("scale", program, glUniform2fv,
                1, glm::value_ptr(world_viewport.scale));
    set_uniform("translation", program, glUniform2fv,
                1, glm::value_ptr(world_viewport.pos));

#if defined(LUX_GLES_2_0)
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(Vert),
        (void*)offsetof(Vert, pos));
    glEnableVertexAttribArray(shader_attribs.pos);
#elif defined(LUX_GL_3_3)
    glBindVertexArray(vao);
#endif

    static DynArr<Vert::Idx> idxs;
    static DynArr<Vert>     verts;

    idxs.resize(comps.pos.size() * 2 * 3);
    verts.resize(comps.pos.size() * 4);
    U32 off = 0;
    for(auto const& entity : comps.pos) {
        verts[off * 4 + 0] = {Vec2F(-1, -1) + Vec2F(entity.second)};
        verts[off * 4 + 1] = {Vec2F( 1, -1) + Vec2F(entity.second)};
        verts[off * 4 + 2] = {Vec2F(-1,  1) + Vec2F(entity.second)};
        verts[off * 4 + 3] = {Vec2F( 1,  1) + Vec2F(entity.second)};
        Vert::Idx constexpr idx_order[] = {0, 1, 2, 2, 3, 1};
        for(Uns j = 0; j < 6; ++j) {
            idxs[off * 6 + j] = idx_order[j] + off * 4;
        }
        ++off;
    }
    for(auto const& entity : comps.name) {
        if(comps.pos.count(entity.first) > 0) {
            EntityHandle id = entity.first;
            if(entity_names.count(id) == 0) {
                DynStr name(entity.second.cbegin(), entity.second.cend());
                entity_names[id] =
                    create_text({0, 0}, {0, 0}, name.c_str());
            }
            TextField& text = get_text_field(entity_names[id]);
            Vec2F world_pos = comps.pos.at(id);
            text.pos   = transform_point(world_pos, world_viewport);
            text.scale = world_viewport.scale * 0.5f;
            text.pos.x -= ((F32)comps.name.at(id).size() / 2.f) * text.scale.x;
            text.pos.y -= 3.f * text.scale.y;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(Vert) * verts.size(), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(Vert::Idx) * idxs.size(), idxs.data(), GL_DYNAMIC_DRAW);

    glDrawElements(GL_TRIANGLES, idxs.size(), Vert::IDX_GL_TYPE, 0);
#if defined(LUX_GLES_2_0)
    glDisableVertexAttribArray(shader_attribs.pos);
#endif
}
