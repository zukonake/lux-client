#pragma once

#include <lux_shared/common.hpp>
//
#include <ui.hpp>

struct UiElement;
extern SparseDynArr<UiElement, U16> ui_elems;
typedef decltype(ui_elems)::Id UiId;

struct UiText;
extern SparseDynArr<UiText, U16> ui_texts;
typedef decltype(ui_texts)::Id TextId;

struct UiPane;
extern SparseDynArr<UiPane, U16> ui_panes;
typedef decltype(ui_panes)::Id PaneId;

struct UiElement {
    DynArr<UiId> children;
    void (*render)(void* ptr, Vec2F const&, Vec2F const&) = nullptr;
    Vec2F pos   = {0.f, 0.f};
    Vec2F scale = {1.f, 1.f};
    void* ptr;
    bool fixed_aspect = false;
};

///those get initialized during ui_init
extern UiId ui_screen;
extern UiId ui_world;
extern UiId ui_hud;

//Vec2F transform_point(Vec2F const& point, UiElement const& ui_elem);

struct UiText {
    UiId ui;
    DynArr<char> buff;
};

struct UiPane {
    UiId ui;
    Vec2F size;
    Vec4F bg_col;
};

UiId new_ui();
UiId new_ui(UiId parent);
void erase_ui(UiId handle);

TextId create_text(Vec2F pos, Vec2F scale, const char* str, UiId parent);
PaneId create_pane(Vec2F pos, Vec2F scale, Vec2F size,
                       Vec4F const& bg_col, UiId parent);
void erase_text(TextId handle);

void ui_window_sz_cb(Vec2U const& window_sz);
void ui_init();
void ui_deinit();
void ui_render();
