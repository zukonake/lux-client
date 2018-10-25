#include <config.hpp>
//
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
//
#include <db.hpp>
#include <rendering.hpp>
#include <client.hpp>
#include <ui.hpp>
#include "entity.hpp"

UiId ui_entity;
static EntityComps comps;
EntityComps& entity_comps = comps;
DynArr<EntityId> entities;

static GLuint program;
static GLuint tileset;

#pragma pack(push, 1)
struct Vert {
    typedef U32 Idx;
    static constexpr GLenum IDX_GL_TYPE = GL_UNSIGNED_INT;
    Vec2F    pos;
    Vec2<U8> tex_pos;
};
#pragma pack(pop)

static GLuint vbo;
static GLuint ebo;
#if defined(LUX_GL_3_3)
static GLuint vao;
#endif

struct {
    GLint pos;
    GLint tex_pos;
} static shader_attribs;

static void entity_render(void *, Vec2F const&, Vec2F const&);

void entity_init() {
    constexpr Vec2U tile_sz = {8, 8};
    char const* tileset_path = "entity_tileset.png";
    program = load_program("glsl/entity.vert", "glsl/entity.frag");

    glUseProgram(program);
    shader_attribs.pos     = glGetAttribLocation(program, "pos");
    shader_attribs.tex_pos = glGetAttribLocation(program, "tex_pos");
    Vec2U tileset_sz;
    tileset = load_texture(tileset_path, tileset_sz);
    Vec2F tex_scale = (Vec2F)tile_sz / (Vec2F)tileset_sz;
    set_uniform("tex_scale", program, glUniform2fv, 1,
                glm::value_ptr(tex_scale));

    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

#if defined(LUX_GL_3_3)
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(shader_attribs.pos);
    glEnableVertexAttribArray(shader_attribs.tex_pos);
    glVertexAttribPointer(shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(Vert),
        (void*)offsetof(Vert, pos));
    glVertexAttribPointer(shader_attribs.tex_pos,
        2, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(Vert),
        (void*)offsetof(Vert, tex_pos));
#endif
    ui_entity = new_ui(ui_world, 50);
    ui_elems[ui_entity].render = &entity_render;
}

static void entity_render(void *, Vec2F const& translation, Vec2F const& scale) {
    glUseProgram(program);
    set_uniform("scale", program, glUniform2fv,
                1, glm::value_ptr(scale));
    set_uniform("translation", program, glUniform2fv,
                1, glm::value_ptr(translation));

#if defined(LUX_GLES_2_0)
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(shader_attribs.pos);
    glEnableVertexAttribArray(shader_attribs.tex_pos);
    glVertexAttribPointer(shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(Vert),
        (void*)offsetof(Vert, pos));
    glVertexAttribPointer(shader_attribs.tex_pos,
        2, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(Vert),
        (void*)offsetof(Vert, tex_pos));
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
            Vec2<U8> constexpr tex_quad[] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
            EntitySprite const& sprite =
                db_entity_sprite(comps.visible.at(id).visible_id);
            for(Uns i = 0; i < 4; ++i) {
                verts.emplace_back(Vert
                    {(quad_sz / 2.f) * rotate(quad[i], angle) + pos,
                     tex_quad[i] * sprite.sz + sprite.pos});
            }
            //@TODO we need to remove when entity is removed
            if(comps.name.count(id) > 0) {
                auto const& name = comps.name.at(id);
                if(comps.text.count(id) == 0) {
                    DynStr name_str(name.cbegin(), name.cend());
                    if(id == ss_tick.player_id) {
                        name_str = "\\f2" + name_str;
                    } else {
                        name_str = "\\f7" + name_str;
                    }
                    comps.text[id] = {
                        create_text({0, 0}, {1.f, 1.f}, name_str.c_str(), ui_entity)};
                }
                UiText* text = ui_texts.at(comps.text.at(id).text);
                if(text == nullptr) {
                    comps.text.erase(id);
                } else {
                    //@TODO what if UI was deleted as well?
                    ui_elems[text->ui].pos = pos - Vec2F(name.size() / 2.f, 2.f);
                }
            }
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(Vert) * verts.size(), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(Vert::Idx) * idxs.size(), idxs.data(), GL_DYNAMIC_DRAW);

    glBindTexture(GL_TEXTURE_2D, tileset);
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
    for(auto const& container : net_comps.container) {
        comps.container[container.first].items = container.second.items;
    }
    for(auto const& orientation : net_comps.orientation) {
        comps.orientation[orientation.first] = {orientation.second.angle};
    }
    for(auto it = comps.text.begin(); it != comps.text.end();) {
        auto const& id = it->first;
        if(comps.name.count(id) == 0 ||
           comps.pos.count(id) == 0 ||
           comps.visible.count(id) == 0) {
            erase_text(it->second.text);
            it = comps.text.erase(it);
        } else ++it;
    }
}
