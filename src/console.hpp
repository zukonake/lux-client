#pragma once

#include <include_glfw.hpp>

void console_init(Vec2U win_size);
void console_window_resize_cb(int win_w, int win_h);
void console_key_cb(int key, int code, int action, int mods);
void console_render();
void console_clear();
bool console_is_active();
