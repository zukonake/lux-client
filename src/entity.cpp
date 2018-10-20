#include <config.hpp>
//
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
//
#include <rendering.hpp>
#include <client.hpp>
#include <viewport.hpp>
#include <ui.hpp>
#include "entity.hpp"

DynArr<EntityHandle> entities;

struct EntityComps {
    typedef EntityVec Pos;
    typedef DynArr<char> Name;
    struct Visible {
        U32   visible_id;
        Vec2F quad_sz;
    };
    struct Orientation {
        F32 angle; ///in radians
    };
    struct Text {
        TextHandle id;
    };

    HashTable<EntityHandle, Pos>         pos;
    HashTable<EntityHandle, Name>        name;
    HashTable<EntityHandle, Visible>     visible;
    HashTable<EntityHandle, Orientation> orientation;
    HashTable<EntityHandle, Text>        text;
} static comps;

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

void entity_render() {
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
    idxs.clear();
    verts.clear();

    for(auto const& id : entities) {
        if(comps.pos.count(id) > 0 &&
           comps.visible.count(id) > 0) {
            Vec2F const& pos = Vec2F(comps.pos.at(id));
            Vec2F const& quad_sz = comps.visible.at(id).quad_sz;
            F32 angle = 0;
            if(comps.orientation.count(id) > 0) {
                angle = comps.orientation.at(id).angle;
            }
            for(Vert::Idx const& idx : {0, 1, 2, 2, 3, 1}) {
                idxs.emplace_back(verts.size() + idx);
            }
            Vec2F constexpr quad[] =
                {{-1.f, -1.f}, {1.f, -1.f}, {-1.f, 1.f}, {1.f, 1.f}};
            for(auto const& vert : quad) {
                verts.emplace_back(Vert{(quad_sz / 2.f) * rotate(vert, angle) +
                                        pos});
            }
            if(comps.name.count(id) > 0) {
                auto const& name = comps.name.at(id);
                if(comps.text.count(id) == 0) {
                    DynStr name_str(name.cbegin(), name.cend());
                    name_str = "\\f2" + name_str;
                    comps.text[id] = {
                        create_text({0, 0}, {0, 0}, name_str.c_str())};
                }
                TextField& text = get_text_field(comps.text.at(id).id);
                text.pos   = transform_point(pos, world_viewport);
                text.scale = world_viewport.scale * 0.5f;
                text.pos.x -= ((F32)name.size() / 2.f) * text.scale.x;
                //@TODO we should calculate the offset here
                text.pos.y -= 3.f * text.scale.y;
            }
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

void set_net_entity_comps(NetSsTick::EntityComps const& net_comps) {
    comps.pos.clear();
    comps.name.clear();
    comps.visible.clear();
    comps.orientation.clear();
    for(auto const& pos : net_comps.pos) {
        comps.pos[pos.first] = pos.second;
    }
    for(auto const& name : net_comps.name) {
        comps.name[name.first] = name.second;
    }
    for(auto const& visible : net_comps.visible) {
        comps.visible[visible.first] =
            {visible.second.visible_id, visible.second.quad_sz};
    }
    for(auto const& orientation : net_comps.orientation) {
        comps.orientation[orientation.first] = {orientation.second.angle};
    }
}
