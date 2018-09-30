#pragma once

#include <config.hpp>
//
#include <GLFW/glfw3.h>
//
#include <lux_shared/common.hpp>

void console_init(Vec2U win_size);
void console_deinit();
void console_window_resize_cb(int win_w, int win_h);
void console_key_cb(int key, int code, int action, int mods);
void console_print(char const* str);
void console_render();
void console_clear();
bool console_is_active();
