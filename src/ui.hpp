#pragma once

#include <lux_shared/common.hpp>
#include <lux_shared/net/data.hpp>
//
#include <ui.hpp>

struct MouseEvent {
    Vec2F pos;
    int   button;
    int   action;
};

struct ScrollEvent {
    Vec2F pos;
    F64   off;
};

struct KeyEvent {
    int key;
    int action;
};

struct IoContext {
    DynArr<MouseEvent>  mouse_events;
    DynArr<ScrollEvent> scroll_events;
    DynArr<KeyEvent>    key_events;
    Vec2F               mouse_pos;
};

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
    Vec3F pos;
    Vec3F scale = {1.f, 1.f, 1.f};
};

struct UiNode {
    DynArr<UiId> children;
    //@TODO event loop and united functions
    void (*deinit)(U32) = nullptr;
    void (*io_tick)(U32, Transform const&, IoContext&) = nullptr;
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
    StrBuff buff;

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

UiTextId ui_text_create(UiId parent, Transform const& tr, Str const& str);
UiPaneId ui_pane_create(UiId parent, Transform const& tr, Vec4F const& bg_col);

LUX_MAY_FAIL ui_add_rasen_label(NetRasenLabel const& label);
bool ui_has_rasen_label(Str const& str_id);
void ui_do_action(Str const& str_id, Slice<U8> const& stack);
void ui_add_discrete_binding(Str const& str_id, int key, Slice<U8> const& stack);
void ui_add_continuous_binding(Str const& str_id, int key, Slice<U8> const& stack);
void ui_window_sz_cb(Vec2U const& old_window_sz, Vec2U const& window_sz);
void ui_init();
void ui_deinit();
void ui_io_tick();
void ui_mouse(Vec2F pos, int button, int action);
void ui_scroll(Vec2F pos, F64 off);
void ui_key(int key, int action);
