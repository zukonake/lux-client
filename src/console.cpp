#include <config.h>
//
#include <cstring>
//
#include <include_opengl.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
//
#include <lux_shared/common.hpp>
//
#include <rendering.hpp>
#include "console.hpp"

#pragma pack(push, 1)
struct GridVert {
    Vec2F pos;
};

struct FontVert {
    Vec2F    font_pos;
    Vec3<U8> col;
};
#pragma pack(pop)

struct {
    Vec2U grid_size = {0, 10};
    Uns scale     = 3;
    Uns char_size = 8;

    U32 idxs_count = 0;
    GLuint grid_vbo;
    GLuint font_vbo;
    GLuint ebo;
#if LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
    GLuint vao;
#endif

    GLuint font;
    GLuint program;

    bool is_active = false;
    DynArr<char>     font_buff;
    DynArr<FontVert> font_verts;

    U8 cursor_pos = 0;
} static console;

struct {
    GLint pos;
    GLint font_pos;
    GLint col;
} static shader_attribs;

static void console_enter();
static void console_backspace();
static void console_input_char(char character);

void console_init(Vec2U win_size) {
    char const* font_path = "font.png";
    console.program = load_program("glsl/console.vert", "glsl/console.frag");
    Vec2U font_size;
    console.font = load_texture(font_path, font_size);
    Vec2F font_pos_scale = Vec2F(console.char_size) / (Vec2F)font_size;
    glUseProgram(console.program);
    set_uniform("font_pos_scale", console.program,
                glUniform2fv, 1, glm::value_ptr(font_pos_scale));

    shader_attribs.pos      = glGetAttribLocation(console.program, "pos");
    shader_attribs.font_pos = glGetAttribLocation(console.program, "font_pos");
    shader_attribs.col      = glGetAttribLocation(console.program, "col");

    glGenBuffers(1, &console.grid_vbo);
    glGenBuffers(1, &console.font_vbo);
    glGenBuffers(1, &console.ebo);

#if LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
    glGenVertexArrays(1, &console.vao);
    glBindVertexArray(console.vao);

    glBindBuffer(GL_ARRAY_BUFFER, console.grid_vbo);
    glVertexAttribPointer(shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(GridVert),
        (void*)offsetof(GridVert, pos));

    glBindBuffer(GL_ARRAY_BUFFER, console.font_vbo);
    glVertexAttribPointer(shader_attribs.font_pos,
        2, GL_FLOAT, GL_FALSE, sizeof(FontVert),
        (void*)offsetof(FontVert, font_pos));
    glVertexAttribPointer(shader_attribs.col,
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FontVert),
        (void*)offsetof(FontVert, col));
#endif

    console_window_resize_cb(win_size.x, win_size.y);
    console_clear();
}

void console_window_resize_cb(int win_w, int win_h) {
    console.grid_size.x = win_w / (console.char_size * console.scale);
    auto const& grid_size = console.grid_size;
    console.font_buff.resize(grid_size.x * grid_size.y);
    console.font_verts.resize(4 * grid_size.x * grid_size.y);
    //@CONSIDER static buffs
    DynArr<GridVert> grid_verts(4 * grid_size.x * grid_size.y);
    DynArr<U32>            idxs(6 * grid_size.x * grid_size.y);
    for(Uns i = 0; i < grid_size.x * grid_size.y; ++i) {
        Vec2F base_pos = {i % grid_size.x, i / grid_size.x};
        grid_verts[i * 4 + 0].pos = (base_pos + Vec2F(0, 0));
        grid_verts[i * 4 + 1].pos = (base_pos + Vec2F(0, 1));
        grid_verts[i * 4 + 2].pos = (base_pos + Vec2F(1, 0));
        grid_verts[i * 4 + 3].pos = (base_pos + Vec2F(1, 1));
        constexpr U32 idx_order[6] = {0, 1, 2, 2, 3, 1};
        for(Uns j = 0; j < 6; ++j) {
            idxs[i * 6 + j] = i * 4 + idx_order[j];
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, console.grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GridVert) * grid_verts.size(),
                 grid_verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, console.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(U32) * idxs.size(),
                 idxs.data(), GL_STATIC_DRAW);
    console.idxs_count = idxs.size();

    glUseProgram(console.program);
    glm::mat4 transform(1.f);
    transform = glm::translate(transform, Vec3F(-1.f, -1.f, 0.f));
    Vec2F vec_scale = (Vec2F)(console.char_size * console.scale) / Vec2F(win_w, win_h);
    transform = glm::scale(transform, Vec3F(vec_scale * 2.f, 1.f));
    set_uniform("transform", console.program,
                glUniformMatrix4fv, 1, GL_FALSE, glm::value_ptr(transform));
}

void console_key_cb(int key, int code, int action, int mods) {
#define CASE_CHAR(key, normal, shift) \
    case GLFW_KEY_##key: { \
        if(mods & GLFW_MOD_SHIFT) console_input_char(shift); \
        else                      console_input_char(normal); \
    } break

    if(action != GLFW_PRESS) return;
    if(!console.is_active) {
        if(key == GLFW_KEY_T) {
            console.is_active = true;
        }
    } else {
        if(key == GLFW_KEY_ESCAPE) {
            console.is_active = false;
        } else switch(key) {
            CASE_CHAR(APOSTROPHE   , '\'', '"');
            CASE_CHAR(COMMA        , ',' , '<');
            CASE_CHAR(MINUS        , '-' , '_');
            CASE_CHAR(PERIOD       , '.' , '>');
            CASE_CHAR(SLASH        , '/' , '?');
            CASE_CHAR(SEMICOLON    , ';' , ':');
            CASE_CHAR(EQUAL        , '=' , '+');
            CASE_CHAR(LEFT_BRACKET , '[' , '{');
            CASE_CHAR(BACKSLASH    , '\\', '|');
            CASE_CHAR(RIGHT_BRACKET, ']' , '}');
            CASE_CHAR(GRAVE_ACCENT , '`' , '~');
            default: {
                if(key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
                    if(mods & GLFW_MOD_SHIFT) {
                        switch(key) {
                            case '0': { console_input_char(')'); } break;
                            case '1': { console_input_char('!'); } break;
                            case '2': { console_input_char('@'); } break;
                            case '3': { console_input_char('#'); } break;
                            case '4': { console_input_char('$'); } break;
                            case '5': { console_input_char('%'); } break;
                            case '6': { console_input_char('^'); } break;
                            case '7': { console_input_char('&'); } break;
                            case '8': { console_input_char('*'); } break;
                            case '9': { console_input_char('('); } break;
                            default: LUX_UNREACHABLE();
                        }
                    } else console_input_char(key);
                } else if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                    if(mods & GLFW_MOD_SHIFT) console_input_char(key);
                    else                      console_input_char(key + 32);
                } else if(key == GLFW_KEY_SPACE) {
                    console_input_char(' ');
                } else if(key == GLFW_KEY_BACKSPACE) {
                    console_backspace();
                } else if(key == GLFW_KEY_ENTER) {
                    console_enter();
                }
            } break;
        }
    }
#undef CASE_CHAR
}

void console_clear() {
    std::memset(console.font_buff.data(), 0, console.font_buff.size());
}

void console_render() {
    if(console.is_active) {
        for(Uns i = 0; i < console.font_buff.size(); ++i) {
            Vec2F base_pos = {console.font_buff[i] % 16,
                              console.font_buff[i] / 16};
            if(i < console.grid_size.x && i == console.cursor_pos) {
                base_pos = {'|' % 16, '|' / 16};
            }
            console.font_verts[i * 4 + 0].font_pos = base_pos + Vec2F(0, 1);
            console.font_verts[i * 4 + 1].font_pos = base_pos + Vec2F(0, 0);
            console.font_verts[i * 4 + 2].font_pos = base_pos + Vec2F(1, 1);
            console.font_verts[i * 4 + 3].font_pos = base_pos + Vec2F(1, 0);
            console.font_verts[i * 4 + 0].col = Vec3<U8>(0xFF, 0xFF, 0xFF);
            console.font_verts[i * 4 + 1].col = Vec3<U8>(0xFF, 0xFF, 0xFF);
            console.font_verts[i * 4 + 2].col = Vec3<U8>(0xFF, 0xFF, 0xFF);
            console.font_verts[i * 4 + 3].col = Vec3<U8>(0xFF, 0xFF, 0xFF);
        }
        glBindBuffer(GL_ARRAY_BUFFER, console.font_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(FontVert) * console.font_verts.size(),
                     console.font_verts.data(), GL_STREAM_DRAW);

        glUseProgram(console.program);
        glBindTexture(GL_TEXTURE_2D, console.font);
#if LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
        glBindBuffer(GL_ARRAY_BUFFER, console.grid_vbo);
        glVertexAttribPointer(shader_attribs.pos,
            2, GL_FLOAT, GL_FALSE, sizeof(GridVert),
            (void*)offsetof(GridVert, pos));

        glBindBuffer(GL_ARRAY_BUFFER, console.font_vbo);
        glVertexAttribPointer(shader_attribs.font_pos,
            2, GL_FLOAT, GL_FALSE, sizeof(FontVert),
            (void*)offsetof(FontVert, font_pos));
        glVertexAttribPointer(shader_attribs.col,
            3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FontVert),
            (void*)offsetof(FontVert, col));
#else
        glBindVertexArray(console.vao);
#endif
        glEnableVertexAttribArray(shader_attribs.pos);
        glEnableVertexAttribArray(shader_attribs.font_pos);
        glEnableVertexAttribArray(shader_attribs.col);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, console.ebo);
        glDrawElements(GL_TRIANGLES, console.idxs_count, GL_UNSIGNED_INT, 0);
        glDisableVertexAttribArray(shader_attribs.pos);
        glDisableVertexAttribArray(shader_attribs.font_pos);
        glDisableVertexAttribArray(shader_attribs.col);
    }
}

bool console_is_active() {
    return console.is_active;
}

static void console_input_char(char character) {
    if(console.cursor_pos < console.grid_size.x) {
        console.font_buff[console.cursor_pos] = character;
        ++console.cursor_pos;
    }
}

static void console_backspace() {
    console.font_buff[console.cursor_pos] = 0;
    if(console.cursor_pos > 0) --console.cursor_pos;
}

static void console_enter() {
    auto const& grid_size = console.grid_size;
    std::memmove(console.font_buff.data() + grid_size.x,
                 console.font_buff.data(), grid_size.x * (grid_size.y - 1));
    std::memset(console.font_buff.data(), 0, grid_size.x);
    console.cursor_pos = 0;
}
