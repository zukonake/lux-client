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

UiHandle ui_screen;
UiHandle ui_world;
UiHandle ui_hud;

SparseDynArr<UiElement, U16> ui_elems;
SparseDynArr<UiText   , U16> ui_texts;

static void render_text(void*, Vec2F const&, Vec2F const&);

UiHandle new_ui() {
    UiHandle handle = ui_elems.emplace();
    return handle;
}

UiHandle new_ui(UiHandle parent) {
    UiHandle handle = ui_elems.emplace();
    LUX_ASSERT(parent.is_valid());
    parent->children.emplace_back(handle);
    return handle;
}

void erase_ui(UiHandle handle) {
    LUX_ASSERT(handle.is_valid());
    for(UiHandle child : handle->children) {
        erase_ui(child);
    }
    handle.erase();
}

TextHandle create_text(Vec2F pos, Vec2F scale, const char* str, UiHandle parent) {
    TextHandle text  = ui_texts.emplace();
    text->ui         = new_ui(parent);
    text->ui->render = &render_text;
    text->ui->pos    = pos;
    text->ui->scale  = scale;
    text->ui->ptr    = (void*)(std::uintptr_t)text.id;
    text->ui->fixed_aspect = true;
    SizeT str_sz     = std::strlen(str);
    text->buff.resize(str_sz);
    std::memcpy(text->buff.data(), str, str_sz);
    return text;
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

static void update_aspect(UiHandle ui, F32 w_to_h) {
    if(ui->fixed_aspect) {
        ui->scale.x = (ui->scale.y < 0 ? -1.f : 1.f) * ui->scale.y * w_to_h;
    } else {
        for(auto& child : ui->children) {
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
    ui_screen = new_ui();
    ui_world  = new_ui(ui_screen);
    //@TODO calculate
    ui_world->scale = {0.06f, -0.06f};
    ui_world->fixed_aspect = true;

    ui_hud = new_ui(ui_screen);
    ui_hud->scale = {0.01f, -0.01f};
}

void ui_deinit() {
    erase_ui(ui_screen);
}

static void render_text(void* ptr, Vec2F const& pos, Vec2F const& scale) {
    TextHandle text = {&ui_texts, (decltype(ui_texts)::Id)(std::uintptr_t)ptr};
    Vec4<U8> fg_col = {0xFF, 0xFF, 0xFF, 0xFF};
    Vec4<U8> bg_col = {0x00, 0x00, 0x00, 0x00};
    bool fg = false;
    bool bg = false;
    Vec2F off = {0, 0};
    bool special = false;
    for(auto character : text->buff) {
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
                if(character == 0x7) col = {0xc0, 0xc0, 0xc0, 0xFF};
                else if(character == 0x8) col = {0x80, 0x80, 0x80, 0xFF};
                else {
                    col = Vec4F( character & 0b0001,
                                (character & 0b0010) >> 1,
                                (character & 0b0100) >> 2,
                                (character & 0b1000) >> 3 ? 1.f : 0.25f) * 255.f;
                }
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
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, fg_col));
    glVertexAttribPointer(text_system.shader_attribs.bg_col,
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
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

static void ui_render(UiHandle const& ui, Vec2F const& pos, Vec2F const& scale) {
    Vec2F total_scale = scale * ui->scale;
    if(ui->render != nullptr) {
        (*ui->render)(ui->ptr, ui->pos + pos, total_scale);
    }
    for(auto child : ui->children) {
        ui_render(child, ui->pos * total_scale + pos, total_scale);
    }
}

void ui_render() {
    ui_render(ui_screen, {0.f, 0.f}, {1.f, 1.f});
    text_system_render();
}
