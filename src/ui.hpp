#pragma once

#include <lux_shared/common.hpp>
//
#include <ui.hpp>

typedef U32 TextHandle;

void ui_window_sz_cb(Vec2U const& window_sz);
TextHandle create_text(Vec2I pos, F32 scale, const char* str);
void delete_text(TextHandle handle);
void ui_init();
