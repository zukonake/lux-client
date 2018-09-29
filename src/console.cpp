#include <config.hpp>
//
#include <cstring>
//
#include <lua.hpp>
#include <include_opengl.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
//
#include <lux_shared/common.hpp>
//
#include <rendering.hpp>
#include <client.hpp>
#include "console.hpp"

#pragma pack(push, 1)
struct GridVert {
    Vec2F pos;
};

struct FontVert {
    Vec2F    font_pos;
    Vec3<U8> fg_col;
    Vec3<U8> bg_col;
};
#pragma pack(pop)

struct Console {
    static constexpr Uns IN_BUFF_WIDTH = 0x60;
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
    Arr<char, IN_BUFF_WIDTH> in_buff;
    DynArr<char>     out_buff;
    DynArr<FontVert> font_verts;

    Uns cursor_pos = 0;
    Uns cursor_scroll = 0;
    lua_State* lua_L;
} static console;

struct {
    GLint pos;
    GLint font_pos;
    GLint fg_col;
    GLint bg_col;
} static shader_attribs;

static void console_move_cursor(bool forward);
static void console_enter();
static void console_backspace();
static void console_delete();
static void console_input_char(char character);
static Uns  console_seek_last();

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
    shader_attribs.fg_col   = glGetAttribLocation(console.program, "fg_col");
    shader_attribs.bg_col   = glGetAttribLocation(console.program, "bg_col");

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
    glVertexAttribPointer(shader_attribs.fg_col,
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FontVert),
        (void*)offsetof(FontVert, fg_col));
    glVertexAttribPointer(shader_attribs.bg_col,
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FontVert),
        (void*)offsetof(FontVert, bg_col));
#endif

    console_window_resize_cb(win_size.x, win_size.y);
    console_clear();

    console.lua_L = luaL_newstate();
    luaL_openlibs(console.lua_L);
    luaL_loadstring(console.lua_L,
                    "package.path = package.path .. \";api/?.lua\"");
    lua_call(console.lua_L, 0, 0);
    switch(luaL_loadfile(console.lua_L, "api/lux-api.lua")) {
        case 0: break;
        case LUA_ERRSYNTAX:
            LUX_FATAL("lua syntax error: \n%s",
                      lua_tolstring(console.lua_L, -1, nullptr));
        case LUA_ERRMEM: LUX_FATAL("lua memory error");
        default: LUX_FATAL("lua unknown error");
    }
    switch(lua_pcall(console.lua_L, 0, 0, 0)) {
        case 0: break;
        case LUA_ERRRUN:
            LUX_FATAL("lua runtime error: \n%s",
                      lua_tolstring(console.lua_L, -1, nullptr));
        case LUA_ERRMEM: LUX_FATAL("lua memory error");
        case LUA_ERRERR: LUX_FATAL("lua error handler error");
        default: LUX_FATAL("lua unknown error");
    }
}

void console_deinit() {
    lua_close(console.lua_L);
}

void console_window_resize_cb(int win_w, int win_h) {
    console.grid_size.x = win_w / (console.char_size * console.scale);
    auto const& grid_size = console.grid_size;
    console.out_buff.resize(grid_size.x * (grid_size.y - 1));
    console.font_verts.resize(4 * grid_size.x * grid_size.y);
    //@CONSIDER static buffs
    DynArr<GridVert> grid_verts(4 * grid_size.x * grid_size.y);
    DynArr<U32>            idxs(6 * grid_size.x * grid_size.y);
    for(Uns i = 0; i < grid_size.x * grid_size.y; ++i) {
        Vec2F base_pos = {i % grid_size.x, i / grid_size.x};
        static const Vec2F quad[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        for(Uns j = 0; j < 4; ++j) {
            grid_verts[i * 4 + j].pos = base_pos + quad[j];
        }
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
                } else if(key == GLFW_KEY_DELETE) {
                    console_delete();
                } else if(key == GLFW_KEY_ENTER) {
                    console_enter();
                } else if(key == GLFW_KEY_LEFT) {
                    if(console.cursor_pos > 0) console_move_cursor(false);
                } else if(key == GLFW_KEY_RIGHT) {
                    if(console.cursor_pos < Console::IN_BUFF_WIDTH) {
                        console_move_cursor(true);
                    }
                }
            } break;
        }
    }
#undef CASE_CHAR
}

void console_clear() {
    std::memset(console.out_buff.data(), 0, console.out_buff.size());
    std::memset(console.in_buff, 0, Console::IN_BUFF_WIDTH);
}

static void blit_character(Uns grid_idx, char character,
                             Vec3<U8> const& fg_col = {0xFF, 0xFF, 0xFF},
                             Vec3<U8> const& bg_col = {0x00, 0x00, 0x00}) {
    Vec2F base_pos = {character % 16, character / 16};
    static const Vec2F quad[4] = {{0, 1}, {0, 0}, {1, 1}, {1, 0}};
    for(Uns i = 0; i < 4; ++i) {
        console.font_verts[grid_idx * 4 + i].font_pos = base_pos + quad[i];
        console.font_verts[grid_idx * 4 + i].fg_col = fg_col;
        console.font_verts[grid_idx * 4 + i].bg_col = bg_col;
    }
}

void console_print(char const* beg) {
    auto const& grid_size = console.grid_size;
    char const* end = beg;
    while(*end != '\0' && (std::uintptr_t)(end - beg) < grid_size.x) {
        ++end;
    }
    Uns len = end - beg;
    std::memmove(console.out_buff.data() + grid_size.x,
                 console.out_buff.data(), grid_size.x * (grid_size.y - 2));
    std::memcpy(console.out_buff.data(), beg, len);
    std::memset(console.out_buff.data() + len, 0, grid_size.x - len);
    if((std::uintptr_t)(end - beg) >= grid_size.x) {
        console_print(end);
    }
}

void console_render() {
    if(console.is_active) {
        auto const& grid_size = console.grid_size;
        ///this might use uninitialized values in font verts if the input
        ///buffer size is smaller than grid size, but it doesn't seem to happen
        for(Uns i = 0; i < grid_size.x && i < Console::IN_BUFF_WIDTH; ++i) {
            if(i == console.cursor_pos - console.cursor_scroll) {
                blit_character(i, console.in_buff[i + console.cursor_scroll],
                    {0x40, 0x40, 0x40}, {0xFF, 0x00, 0x00});
            } else {
                blit_character(i, console.in_buff[i + console.cursor_scroll],
                    {0xFF, 0xFF, 0xFF}, {0x40, 0x40, 0x40});
            }
        }
        for(Uns i = grid_size.x; i < grid_size.x * grid_size.y; ++i) {
            blit_character(i, console.out_buff[i - grid_size.x],
                    {0xD0, 0xD0, 0xD0}, {0x00, 0x00, 0x00});
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
        glVertexAttribPointer(shader_attribs.fg_col,
            3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FontVert),
            (void*)offsetof(FontVert, fg_col));
        glVertexAttribPointer(shader_attribs.bg_col,
            3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FontVert),
            (void*)offsetof(FontVert, bg_col));
#else
        glBindVertexArray(console.vao);
#endif
        glEnableVertexAttribArray(shader_attribs.pos);
        glEnableVertexAttribArray(shader_attribs.font_pos);
        glEnableVertexAttribArray(shader_attribs.fg_col);
        glEnableVertexAttribArray(shader_attribs.bg_col);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, console.ebo);
        glDrawElements(GL_TRIANGLES, console.idxs_count, GL_UNSIGNED_INT, 0);
        glDisableVertexAttribArray(shader_attribs.pos);
        glDisableVertexAttribArray(shader_attribs.font_pos);
        glDisableVertexAttribArray(shader_attribs.fg_col);
        glDisableVertexAttribArray(shader_attribs.bg_col);
    }
}

bool console_is_active() {
    return console.is_active;
}

static void console_move_cursor(bool forward) {
    if(forward) {
        LUX_ASSERT(console.cursor_pos < Console::IN_BUFF_WIDTH);
        ++console.cursor_pos;
        if(console.cursor_pos - console.cursor_scroll >= console.grid_size.x) {
            console.cursor_scroll = console.cursor_pos - console.grid_size.x;
        }
    } else {
        LUX_ASSERT(console.cursor_pos != 0);
        if(console.cursor_pos == console.cursor_scroll) {
            --console.cursor_scroll;
        }
        --console.cursor_pos;
    }
}

static void console_input_char(char character) {
    if(console.cursor_pos < Console::IN_BUFF_WIDTH) {
        std::memmove(console.in_buff + console.cursor_pos + 1,
                     console.in_buff + console.cursor_pos,
                     console_seek_last() - console.cursor_pos);
        console.in_buff[console.cursor_pos] = character;
        console_move_cursor(true);
    }
}

static void console_backspace() {
    if(console.cursor_pos > 0) {
        console_move_cursor(false);
        std::memmove(console.in_buff + console.cursor_pos,
                     console.in_buff + console.cursor_pos + 1,
                     console_seek_last() - console.cursor_pos);
    }
}

static void console_delete() {
    if(console.cursor_pos > 0) {
        std::memmove(console.in_buff + console.cursor_pos,
                     console.in_buff + console.cursor_pos + 1,
                     console_seek_last() - console.cursor_pos);
    }
}

static void console_enter() {
    auto const& grid_size = console.grid_size;
    char const* beg = console.in_buff;
    char const* end = beg;
    while(*end != '\0' && (std::uintptr_t)(end - beg) < Console::IN_BUFF_WIDTH) {
        ++end;
    }
    String command(beg, end - beg);
    std::memset(console.in_buff, 0, Console::IN_BUFF_WIDTH);
    console.cursor_pos = 0;
    console.cursor_scroll = 0;
    if(command.size() > 2 && command[0] == '/') {
        command.erase(0, 1);
        if(command.size() > 2 && command[0] == 's') {
            command.erase(0, 1);
            if(send_command(command.c_str()) != LUX_OK) {
                console_print("failed to send server command");
                console_print(command.c_str());
            }
        } else {
            switch(luaL_loadstring(console.lua_L, command.c_str())) {
                case 0: break;
                case LUA_ERRSYNTAX: {
                    console_print(lua_tolstring(console.lua_L, -1, nullptr));
                    return;
                } break;
                case LUA_ERRMEM: LUX_FATAL("lua memory error");
                default: {
                    console_print("unknown lua error");
                    return;
                } break;
            }
            switch(lua_pcall(console.lua_L, 0, 0, 0)) {
                case 0: break;
                case LUA_ERRRUN: {
                    console_print(lua_tolstring(console.lua_L, -1, nullptr));
                } break;
                case LUA_ERRMEM: LUX_FATAL("lua memory error");
                case LUA_ERRERR: LUX_FATAL("lua error handler error");
                default: {
                    console_print("unknown lua error");
                } break;
            }
        }
    } else {
        console_print(command.c_str());
    }
}

static Uns console_seek_last() {
    Uns pos = 0;
    while(pos < Console::IN_BUFF_WIDTH && console.in_buff[pos] != 0) {
        ++pos;
    }
    return pos;
}
