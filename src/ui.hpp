#pragma once

#include <lux_shared/common.hpp>
//
#include <ui.hpp>

struct UiNode;
extern SparseDynArr<UiNode, U16> ui_nodes;
typedef decltype(ui_nodes)::Id UiId;

struct UiText;
extern SparseDynArr<UiText, U16> ui_texts;
typedef decltype(ui_texts)::Id UiTextId;

struct UiPane;
extern SparseDynArr<UiPane, U16> ui_panes;
typedef decltype(ui_panes)::Id UiPaneId;

struct Transform {
    Vec2F pos;
    Vec2F scale = {1.f, 1.f};
};

struct UiNode {
    DynArr<UiId> children;
    //@TODO event loop and united functions
    void (*deinit)(U32) = nullptr;
    void (*render)(U32, Transform const&) = nullptr;
    bool (*mouse)(U32, Vec2F, int, int) = nullptr;
    bool (*scroll)(U32, Vec2F, F64) = nullptr;
    Transform tr;
    U32       ext_id;
    bool      fixed_aspect = false;
    U8        priority = 0;
};

extern UiId ui_screen;
extern UiId ui_world;
extern UiId ui_camera;
extern UiId ui_hud;

struct UiText {
    UiId ui;
    DynArr<char> buff;

    gl::VertBuff    v_buff;
    gl::IdxBuff     i_buff;
    gl::VertContext context;
};

struct UiPane {
    UiId ui;
    Vec4F bg_col;

    gl::VertBuff    v_buff;
    gl::IdxBuff     i_buff;
    gl::VertContext context;
};

UiId ui_create(UiId parent, U8 priority = 0);
void ui_erase(UiId handle);

UiTextId ui_text_create(UiId parent, Transform const& tr, const char* str);
UiPaneId ui_pane_create(UiId parent, Transform const& tr, Vec4F const& bg_col);

void ui_window_sz_cb(Vec2U const& old_window_sz, Vec2U const& window_sz);
void ui_init();
void ui_deinit();
void ui_render();
bool ui_mouse(Vec2F pos, int button, int action);
bool ui_scroll(Vec2F pos, F64 off);
