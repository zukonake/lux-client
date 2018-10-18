#pragma once

#include <config.hpp>
//
#include <GLFW/glfw3.h>
//
#include <lux_shared/common.hpp>

void console_init();
void console_deinit();
void console_window_sz_cb(Vec2U const& window_sz);
void console_key_cb(int key, int code, int action, int mods);
void console_print(char const* str);
void console_render();
void console_clear();
LUX_MAY_FAIL console_bind_key(char key, char const* input);
bool console_is_active();
