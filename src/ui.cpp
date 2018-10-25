#include <cstdint>
#include <cstring>
//
#include <include_opengl.hpp>
#include <glm/gtc/type_ptr.hpp>
//
#include <lux_shared/common.hpp>
//
#include <rendering.hpp>
#include <ui.hpp>

UiId ui_screen;
UiId ui_world;
UiId ui_hud;

SparseDynArr<UiElement, U16> ui_elems;
SparseDynArr<UiText   , U16> ui_texts;
SparseDynArr<UiPane   , U16> ui_panes;

static void render_text(void*, Vec2F const&, Vec2F const&);
static void render_pane(void*, Vec2F const&, Vec2F const&);

UiId new_ui() {
    UiId id = ui_elems.emplace();
    return id;
}

UiId new_ui(UiId parent) {
    UiId id = ui_elems.emplace();
    LUX_ASSERT(ui_elems.contains(parent));
    ui_elems[parent].children.emplace_back(id);
    return id;
}

void erase_ui(UiId id) {
    LUX_ASSERT(ui_elems.contains(id));
    for(UiId child : ui_elems[id].children) {
        erase_ui(child);
    }
    ui_elems.erase(id);
}

TextId create_text(Vec2F pos, Vec2F scale, const char* str, UiId parent) {
    TextId id  = ui_texts.emplace();
    auto& text = ui_texts[id];
    text.ui    = new_ui(parent);
    auto& ui   = ui_elems[text.ui];
    ui.render  = &render_text;
    ui.pos     = pos;
    ui.scale   = scale;
    ui.ptr     = (void*)(std::uintptr_t)id;
    ui.fixed_aspect = true;
    SizeT str_sz = std::strlen(str);
    text.buff.resize(str_sz);
    std::memcpy(text.buff.data(), str, str_sz);
    return id;
}

void erase_text(TextId id) {
    LUX_ASSERT(ui_texts.contains(id));
    erase_ui(ui_texts[id].ui);
    ui_texts.erase(id);
}

PaneId create_pane(Vec2F pos, Vec2F scale, Vec2F size,
                       Vec4F const& bg_col, UiId parent) {
    PaneId id  = ui_panes.emplace();
    auto& pane = ui_panes[id];
    pane.ui    = new_ui(parent);
    auto& ui   = ui_elems[pane.ui];
    ui.render  = &render_pane;
    ui.pos     = pos;
    ui.scale   = scale;
    ui.ptr     = (void*)(std::uintptr_t)id;
    ui.fixed_aspect = true;
    pane.size   = size;
    pane.bg_col = bg_col;
    return id;
}

struct TextSystem {
#pragma pack(push, 1)
    struct Vert {
        Vec2F    pos;
        Vec2<U8> font_pos;
        Vec4<U8> fg_col;
        Vec4<U8> bg_col;
    };
#pragma pack(pop)

    GLuint program;
    GLuint vbo;
    GLuint ebo;
#if defined(LUX_GL_3_3)
    GLuint vao;
#endif

    GLuint font_texture;

    DynArr<Vert> verts;
    DynArr<U32>  idxs;
    struct {
        GLint pos;
        GLint font_pos;
        GLint fg_col;
        GLint bg_col;
    } shader_attribs;
} static text_system;

struct PaneSystem {
#pragma pack(push, 1)
    struct Vert {
        Vec2F pos;
        Vec4F bg_col;
    };
#pragma pack(pop)

    GLuint program;
    GLuint vbo;
    GLuint ebo;
#if defined(LUX_GL_3_3)
    GLuint vao;
#endif

    DynArr<Vert> verts;
    DynArr<U32>  idxs;
    struct {
        GLint pos;
        GLint bg_col;
    } shader_attribs;
} static pane_system;

static void update_aspect(UiId id, F32 w_to_h) {
    auto& ui = ui_elems[id];
    if(ui.fixed_aspect) {
        ui.scale.x = ui.scale.y * w_to_h;
    } else {
        for(auto& child : ui.children) {
            update_aspect(child, w_to_h);
        }
    }
}

void ui_window_sz_cb(Vec2U const& sz) {
    update_aspect(ui_screen, (F32)sz.y / (F32)sz.x);
}

void ui_init() {
    char const* font_path = "font.png";
    text_system.program = load_program("glsl/text.vert", "glsl/text.frag");
    Vec2U font_size;
    text_system.font_texture = load_texture(font_path, font_size);
    Vec2F font_pos_scale = Vec2F(8.f, 8.f) / (Vec2F)font_size;
    glUseProgram(text_system.program);
    set_uniform("font_pos_scale", text_system.program,
                glUniform2fv, 1, glm::value_ptr(font_pos_scale));

    text_system.shader_attribs.pos =
        glGetAttribLocation(text_system.program, "pos");
    text_system.shader_attribs.font_pos =
        glGetAttribLocation(text_system.program, "font_pos");
    text_system.shader_attribs.fg_col =
        glGetAttribLocation(text_system.program, "fg_col");
    text_system.shader_attribs.bg_col =
        glGetAttribLocation(text_system.program, "bg_col");

    glGenBuffers(1, &text_system.vbo);
    glGenBuffers(1, &text_system.ebo);

#if defined(LUX_GL_3_3)
    glGenVertexArrays(1, &text_system.vao);
    glBindVertexArray(text_system.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_system.ebo);

    glBindBuffer(GL_ARRAY_BUFFER, text_system.vbo);
    glVertexAttribPointer(text_system.shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, pos));
    glVertexAttribPointer(text_system.shader_attribs.font_pos,
        2, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, font_pos));
    glVertexAttribPointer(text_system.shader_attribs.fg_col,
        4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, fg_col));
    glVertexAttribPointer(text_system.shader_attribs.bg_col,
        4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, bg_col));
    glEnableVertexAttribArray(text_system.shader_attribs.pos);
    glEnableVertexAttribArray(text_system.shader_attribs.font_pos);
    glEnableVertexAttribArray(text_system.shader_attribs.fg_col);
    glEnableVertexAttribArray(text_system.shader_attribs.bg_col);
#endif

    pane_system.program = load_program("glsl/pane.vert", "glsl/pane.frag");
    glUseProgram(pane_system.program);

    pane_system.shader_attribs.pos =
        glGetAttribLocation(pane_system.program, "pos");
    pane_system.shader_attribs.bg_col =
        glGetAttribLocation(pane_system.program, "bg_col");

    glGenBuffers(1, &pane_system.vbo);
    glGenBuffers(1, &pane_system.ebo);

#if defined(LUX_GL_3_3)
    glGenVertexArrays(1, &pane_system.vao);
    glBindVertexArray(pane_system.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pane_system.ebo);

    glBindBuffer(GL_ARRAY_BUFFER, pane_system.vbo);
    glVertexAttribPointer(pane_system.shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(PaneSystem::Vert),
        (void*)offsetof(PaneSystem::Vert, pos));
    glVertexAttribPointer(pane_system.shader_attribs.bg_col,
        4, GL_FLOAT, GL_FALSE, sizeof(PaneSystem::Vert),
        (void*)offsetof(PaneSystem::Vert, bg_col));
    glEnableVertexAttribArray(pane_system.shader_attribs.pos);
    glEnableVertexAttribArray(pane_system.shader_attribs.bg_col);
#endif
    ui_screen = new_ui();
    ui_elems[ui_screen].scale = {1.f, -1.f};
    ui_world  = new_ui(ui_screen);
    //@TODO calculate
    ui_elems[ui_world].scale = {1.f / 10.f, 1.f / 10.f};
    ui_elems[ui_world].fixed_aspect = true;

    ui_hud = new_ui(ui_screen);
    ui_elems[ui_hud].scale = {0.01f, 0.01f};
}

void ui_deinit() {
    erase_ui(ui_screen);
}

static void render_text(void* ptr, Vec2F const& pos, Vec2F const& scale) {
    TextId text_id = (TextId)(std::uintptr_t)ptr;
    auto& text = ui_texts[text_id];
    Vec4<U8> fg_col = {0xFF, 0xFF, 0xFF, 0xFF};
    Vec4<U8> bg_col = {0x00, 0x00, 0x00, 0x00};
    bool fg = false;
    bool bg = false;
    Vec2F off = {0, 0};
    bool special = false;
    for(auto character : text.buff) {
        if(character == '\\') {
            special = true;
            continue;
        }
        if(special == true) {
            if(fg || bg) {
                Vec4<U8> col;
                if(character >= '0' && character <= '9') {
                    character -= '0';
                } else if(character >= 'a' && character <= 'f') {
                    character -= 'a';
                    character += 0xa;
                } else {
                    fg = false;
                    bg = false;
                }
                col = Vec4F( character & 0b0001,
                            (character & 0b0010) >> 1,
                            (character & 0b0100) >> 2,
                            (character & 0b1000) >> 3 ? 1.f : 0.25f) * 255.f;
                if(fg) {
                    fg_col = col;
                    fg = false;
                    special = false;
                }
                if(bg) {
                    bg_col = col;
                    bg = false;
                    special = false;
                }
            }
            else switch(character) {
                case '\n':
                off.y += 1.f;
                off.x  = 0.f;
                special = false;
                break;

                case 'f': fg = true; break;
                case 'b': bg = true; break;
                default:
                fg = false;
                bg = false;
                special = false;
                break;
            }
            continue;
        }
        for(Uns i = 0; i < 4; ++i) {
            constexpr Vec2I quad[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
            text_system.verts.push_back({
                pos + ((Vec2F)quad[i] + off) * scale,
                Vec2<U8>(character % 16, character / 16) + (Vec2<U8>)quad[i],
                fg_col, bg_col});
        }
        for(Uns i = 0; i < 6; ++i) {
            constexpr U32 idxs[6] = {0, 1, 2, 2, 3, 1};
            text_system.idxs.emplace_back(text_system.verts.size() - 4 + idxs[i]);
        }
        off.x += 1.f;
    }
}

static void render_pane(void* ptr, Vec2F const& pos, Vec2F const& scale) {
    PaneId pane_id = (PaneId)(std::uintptr_t)ptr;
    auto& pane = ui_panes[pane_id];
    for(Uns i = 0; i < 4; ++i) {
        constexpr Vec2F quad[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        pane_system.verts.push_back({
            pos + quad[i] * scale * pane.size, pane.bg_col});
    }
    for(Uns i = 0; i < 6; ++i) {
        constexpr U32 idxs[6] = {0, 1, 2, 2, 3, 1};
        pane_system.idxs.emplace_back(pane_system.verts.size() - 4 + idxs[i]);
    }
}

static void text_system_render() {
    if(text_system.verts.size() == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, text_system.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TextSystem::Vert) *
        text_system.verts.size(), text_system.verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_system.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(U32) *
        text_system.idxs.size(), text_system.idxs.data(), GL_DYNAMIC_DRAW);

    glUseProgram(text_system.program);
    glBindTexture(GL_TEXTURE_2D, text_system.font_texture);
#if defined(LUX_GLES_2_0)
    glBindBuffer(GL_ARRAY_BUFFER, text_system.vbo);
    glVertexAttribPointer(text_system.shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, pos));
    glVertexAttribPointer(text_system.shader_attribs.font_pos,
        2, GL_FLOAT, GL_FALSE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, font_pos));
    glVertexAttribPointer(text_system.shader_attribs.fg_col,
        4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, fg_col));
    glVertexAttribPointer(text_system.shader_attribs.bg_col,
        4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, bg_col));
    glEnableVertexAttribArray(text_system.shader_attribs.pos);
    glEnableVertexAttribArray(text_system.shader_attribs.font_pos);
    glEnableVertexAttribArray(text_system.shader_attribs.fg_col);
    glEnableVertexAttribArray(text_system.shader_attribs.bg_col);
#elif defined(LUX_GL_3_3)
    glBindVertexArray(text_system.vao);
#endif
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_TRIANGLES, text_system.idxs.size(), GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);
#if defined(LUX_GLES_2_0)
    glDisableVertexAttribArray(text_system.shader_attribs.pos);
    glDisableVertexAttribArray(text_system.shader_attribs.font_pos);
    glDisableVertexAttribArray(text_system.shader_attribs.fg_col);
    glDisableVertexAttribArray(text_system.shader_attribs.bg_col);
#endif
    text_system.verts.clear();
    text_system.idxs.clear();
}

static void pane_system_render() {
    if(pane_system.verts.size() == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, pane_system.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(PaneSystem::Vert) *
        pane_system.verts.size(), pane_system.verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pane_system.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(U32) *
        pane_system.idxs.size(), pane_system.idxs.data(), GL_DYNAMIC_DRAW);

    glUseProgram(pane_system.program);
#if defined(LUX_GLES_2_0)
    glBindBuffer(GL_ARRAY_BUFFER, pane_system.vbo);
    glVertexAttribPointer(pane_system.shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, pos));
    glVertexAttribPointer(pane_system.shader_attribs.bg_col,
        4, GL_FLOAT, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, bg_col));
    glEnableVertexAttribArray(pane_system.shader_attribs.pos);
    glEnableVertexAttribArray(pane_system.shader_attribs.bg_col);
#elif defined(LUX_GL_3_3)
    glBindVertexArray(pane_system.vao);
#endif
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_TRIANGLES, pane_system.idxs.size(), GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);
#if defined(LUX_GLES_2_0)
    glDisableVertexAttribArray(pane_system.shader_attribs.pos);
    glDisableVertexAttribArray(pane_system.shader_attribs.bg_col);
#endif
    pane_system.verts.clear();
    pane_system.idxs.clear();
}

static void ui_render(UiId id, Vec2F const& pos, Vec2F const& scale) {
    UiElement* ui = ui_elems.at(id);
    Vec2F total_scale = scale * ui->scale;
    if(ui->render != nullptr) {
        (*ui->render)(ui->ptr, ui->pos * scale + pos, total_scale);
        ui = ui_elems.at(id);
        if(ui == nullptr) return;
    }
    for(auto it = ui->children.begin(); it != ui->children.end();) {
        if(!ui_elems.contains(*it)) {
            it = ui->children.erase(it);
        } else {
            ui_render(*it, ui->pos * scale + pos, total_scale);
            ui = ui_elems.at(id);
            if(ui == nullptr) return;
            ++it;
        }
    }
}

void ui_render() {
    ui_render(ui_screen, {0.f, 0.f}, {1.f, 1.f});
    pane_system_render();
    text_system_render();
    ui_elems.free_slots();
    ui_texts.free_slots();
    ui_panes.free_slots();
}
