#include <config.hpp>
//
#include <glm/gtc/type_ptr.hpp>
//
#include <rendering.hpp>
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

void entity_render(EntityVec const& player_pos, EntityComps const& comps) {

    Vec2F scale = {0, 0};
    {   Vec2U window_size = get_window_size();
        F32 aspect_ratio = (F32)window_size.x / (F32)window_size.y;
        F32 constexpr BASE_SCALE = 0.04f;
        scale = Vec2F(BASE_SCALE, -BASE_SCALE * aspect_ratio);
    }

    glUseProgram(program);
    set_uniform("scale", program, glUniform2fv, 1, glm::value_ptr(scale));

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
        verts[off * 4 + 0] = {Vec2F(0, 0) + Vec2F(entity.second)};
        verts[off * 4 + 1] = {Vec2F(1, 0) + Vec2F(entity.second)};
        verts[off * 4 + 2] = {Vec2F(0, 1) + Vec2F(entity.second)};
        verts[off * 4 + 3] = {Vec2F(1, 1) + Vec2F(entity.second)};
        Vert::Idx constexpr idx_order[] = {0, 1, 2, 2, 3, 1};
        for(Uns j = 0; j < 6; ++j) {
            idxs[off * 6 + j] = idx_order[j] + off * 4;
        }
        off += 4;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(Vert) * verts.size(), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(Vert::Idx) * idxs.size(), idxs.data(), GL_DYNAMIC_DRAW);

    Vec2F translation = Vec2F(-player_pos);
    set_uniform("translation", program, glUniform2fv,
                1, glm::value_ptr(translation));

    glDrawElements(GL_TRIANGLES, idxs.size(), Vert::IDX_GL_TYPE, 0);
#if defined(LUX_GLES_2_0)
    glDisableVertexAttribArray(shader_attribs.pos);
#endif
}
