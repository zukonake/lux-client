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

struct Console {
    static constexpr Uns IN_BUFF_WIDTH = 0x60;
    Vec2U grid_size = {0, 10};
    Uns scale     = 3;
    Uns char_size = 8;

    bool is_active = false;
    Arr<char, IN_BUFF_WIDTH> in_buff;
    DynArr<char>     out_buff;

    Uns cursor_pos = 0;
    Uns cursor_scroll = 0;
    lua_State* lua_L;
    HashTable<char, DynStr> key_bindings;
} static console;

static void console_move_cursor(bool forward);
static void console_enter();
static void console_backspace();
static void console_delete();
static void console_input_char(char character);
static void console_exec_command(char const* str);
static char parse_glfw_key(int key, int mods);
static Uns  console_seek_last();

void console_init() {
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

    ///bind some useful stuff
    LUX_ASSERT(console_bind_key('q', "/lux.quit()"));
    LUX_ASSERT(console_bind_key('r', "/lux.reload_program()"));
}

void console_deinit() {
    lua_close(console.lua_L);
}

void console_window_sz_cb(Vec2U const& window_sz) {
    console_clear();
}

void console_key_cb(int key, int code, int action, int mods) {
    (void)code;
    if(action != GLFW_PRESS) return;
    if(!console.is_active) {
        if(key == GLFW_KEY_T) {
            console.is_active = true;
        } else {
            char parsed_char = parse_glfw_key(key, mods);
            if(parsed_char != '\0' &&
               console.key_bindings.count(parsed_char) > 0) {
                console_exec_command(console.key_bindings.at(parsed_char).c_str());
            }
        }
    } else {
        switch(key) {
            case GLFW_KEY_ESCAPE:    { console.is_active = false; } break;
            case GLFW_KEY_BACKSPACE: { console_backspace();       } break;
            case GLFW_KEY_DELETE:    { console_delete();          } break;
            case GLFW_KEY_ENTER:     { console_enter();           } break;
            case GLFW_KEY_LEFT: {
                if(console.cursor_pos > 0) console_move_cursor(false);
            } break;
            case GLFW_KEY_RIGHT: {
                if(console.cursor_pos < Console::IN_BUFF_WIDTH) {
                    console_move_cursor(true);
                }
            } break;
            default: {
                char parsed_char = parse_glfw_key(key, mods);
                if(parsed_char != '\0') console_input_char(parsed_char);
            } break;
        }
    }
}

void console_clear() {
    std::memset(console.out_buff.data(), 0, console.out_buff.size());
    std::memset(console.in_buff, 0, Console::IN_BUFF_WIDTH);
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
}

bool console_is_active() {
    return console.is_active;
}

LUX_MAY_FAIL console_bind_key(char key, char const* input) {
    if(std::strlen(input) >= Console::IN_BUFF_WIDTH) {
        LUX_LOG("console binding too long");
        return LUX_FAIL;
    }
    console.key_bindings[key] = DynStr(input);
    return LUX_OK;
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
                     console_seek_last() + 1 - console.cursor_pos);
    }
}

static void console_delete() {
    if(console.cursor_pos > 0) {
        std::memmove(console.in_buff + console.cursor_pos,
                     console.in_buff + console.cursor_pos + 1,
                     console_seek_last() + 1 - console.cursor_pos);
    }
}

static void console_exec_command(char const* str) {
    char const* beg = str;
    char const* end = beg;
    while(*end != '\0' && (std::uintptr_t)(end - beg) < Console::IN_BUFF_WIDTH) {
        ++end;
    }
    DynStr command(beg, end - beg);
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

static void console_enter() {
    console_exec_command(console.in_buff);
    std::memset(console.in_buff, 0, Console::IN_BUFF_WIDTH);
    console.cursor_pos = 0;
    console.cursor_scroll = 0;
}

static Uns console_seek_last() {
    Uns pos = 0;
    while(pos < Console::IN_BUFF_WIDTH && console.in_buff[pos] != 0) {
        ++pos;
    }
    return pos;
}

static char parse_glfw_key(int key, int mods) {
#define CASE_CHAR(key, normal, shift) \
    case GLFW_KEY_##key: { \
        if(mods & GLFW_MOD_SHIFT) return shift; \
        else                      return normal; \
    } break

    switch(key) {
        CASE_CHAR(APOSTROPHE    , '\'', '"');
        CASE_CHAR(COMMA         , ',' , '<');
        CASE_CHAR(MINUS         , '-' , '_');
        CASE_CHAR(PERIOD        , '.' , '>');
        CASE_CHAR(SLASH         , '/' , '?');
        CASE_CHAR(SEMICOLON     , ';' , ':');
        CASE_CHAR(EQUAL         , '=' , '+');
        CASE_CHAR(LEFT_BRACKET  , '[' , '{');
        CASE_CHAR(BACKSLASH     , '\\', '|');
        CASE_CHAR(RIGHT_BRACKET , ']' , '}');
        CASE_CHAR(GRAVE_ACCENT  , '`' , '~');
        CASE_CHAR(0             , '0' , ')');
        CASE_CHAR(1             , '1' , '!');
        CASE_CHAR(2             , '2' , '@');
        CASE_CHAR(3             , '3' , '#');
        CASE_CHAR(4             , '4' , '$');
        CASE_CHAR(5             , '5' , '%');
        CASE_CHAR(6             , '6' , '^');
        CASE_CHAR(7             , '7' , '&');
        CASE_CHAR(8             , '8' , '*');
        CASE_CHAR(9             , '9' , '(');
        CASE_CHAR(SPACE         , ' ' , ' ');
        default: if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
            if(mods & GLFW_MOD_SHIFT) return key;
            else                      return key + 32;
        } else return '\0';
    }
#undef CASE_CHAR
}
