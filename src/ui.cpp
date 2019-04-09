#include <cstdint>
#include <cstring>
//
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
//
#include <lux_shared/common.hpp>
//
#include <rendering.hpp>
#include <ui.hpp>
#include <client.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>

static IoContext io_context;

UiId ui_screen;
UiId ui_world;
UiId ui_camera;
UiId ui_hud;
UiId ui_imgui;

SparseDynArr<UiNode, U16> ui_nodes;
SparseDynArr<UiText, U16> ui_texts;
SparseDynArr<UiPane, U16> ui_panes;

static void text_io_tick(U32, Transform const&, IoContext&);
static void text_deinit(U32);
static void pane_io_tick(U32, Transform const&, IoContext&);
static void pane_deinit(U32);
static void imgui_io_tick(U32, Transform const&, IoContext&);

UiId ui_create(UiId parent, U8 priority) {
    UiId id = ui_nodes.emplace();
    LUX_LOG_DBG("creating UI #%u", id);
    //@TODO priority might be deprecated along with the addition of Z-levels
    ui_nodes[id].priority = priority;
    LUX_ASSERT(ui_nodes.contains(parent));
    ui_nodes[parent].children.emplace(id);
    std::sort(ui_nodes[parent].children.begin(),
              ui_nodes[parent].children.end(),
              [](UiId const& a, UiId const& b) {
                  if(ui_nodes.contains(a) && ui_nodes.contains(b)) {
                      return ui_nodes[a].priority < ui_nodes[b].priority;
                  }
                  return false;
              });
    return id;
}

void ui_erase(UiId id) {
    LUX_LOG_DBG("erasing UI #%u", id);
    LUX_ASSERT(ui_nodes.contains(id));
    auto& ui = ui_nodes[id];
    for(UiId child : ui.children) {
        ui_erase(child);
    }
    if(ui.deinit != nullptr) {
        (*ui.deinit)(ui.ext_id);
    }
    ui_nodes.erase(id);
}

struct TextSystem {
#pragma pack(push, 1)
    struct Vert {
        Vec2F    pos;
        Vec2<U8> font_pos;
        Vec4<U8> fg_col;
        Vec4<U8> bg_col;
    };
#pragma pack(pop)

    GLuint      program;
    GLuint      font_texture;
    gl::VertFmt vert_fmt;
} static text_system;

UiTextId ui_text_create(UiId parent, Transform const& tr, Str const& str) {
    UiTextId id = ui_texts.emplace();
    auto& text  = ui_texts[id];
    text.ui     = ui_create(parent);
    text.v_buff.init();
    text.i_buff.init();
    text.context.init({text.v_buff}, text_system.vert_fmt);
    auto& ui  = ui_nodes[text.ui];
    ui.deinit = &text_deinit;
    ui.io_tick = &text_io_tick;
    ui.tr     = tr;
    ui.ext_id = id;
    ui.fixed_aspect = true;
    text.buff = str;
    return id;
}

void text_deinit(U32 id) {
    LUX_ASSERT(ui_texts.contains(id));
    auto& text = ui_texts[id];
    text.v_buff.deinit();
    text.i_buff.deinit();
    text.context.deinit();
    ui_texts.erase(id);
}

struct PaneSystem {
#pragma pack(push, 1)
    struct Vert {
        Vec2F pos;
        Vec4F bg_col;
    };
#pragma pack(pop)

    GLuint program;
    gl::VertFmt vert_fmt;
} static pane_system;

UiPaneId ui_pane_create(UiId parent, Transform const& tr, Vec4F const& bg_col) {
    UiPaneId id = ui_panes.emplace();
    auto& pane = ui_panes[id];
    pane.ui    = ui_create(parent);
    pane.v_buff.init();
    pane.i_buff.init();
    pane.context.init({pane.v_buff}, pane_system.vert_fmt);
    auto& ui  = ui_nodes[pane.ui];
    ui.deinit = &pane_deinit;
    ui.io_tick = &pane_io_tick;
    ui.tr     = tr;
    ui.ext_id = id;
    ui.fixed_aspect = true;
    pane.bg_col = bg_col;
    return id;
}

void pane_deinit(U32 id) {
    LUX_ASSERT(ui_panes.contains(id));
    auto& pane = ui_panes[id];
    pane.v_buff.deinit();
    pane.i_buff.deinit();
    pane.context.deinit();
    ui_panes.erase(id);
}

static void update_aspect(UiId id, F32 old_ratio, F32 ratio) {
    auto& ui = ui_nodes[id];
    if(ui.fixed_aspect) {
        ui.tr.scale.x /= old_ratio;
        ui.tr.scale.x *= ratio;
    } else {
        for(auto& child : ui.children) {
            update_aspect(child, old_ratio, ratio);
        }
    }
}

void ui_window_sz_cb(Vec2U const& old_sz, Vec2U const& sz) {
    update_aspect(ui_screen, (F32)old_sz.y / (F32)old_sz.x,
                             (F32)sz.y / (F32)sz.x);
}

void ui_init() {
    char const* font_path = "font.png";
    text_system.program = load_program("glsl/text.vert", "glsl/text.frag");
    Vec2U font_size;
    text_system.font_texture = load_texture(font_path, font_size);
    Vec2F font_pos_scale = Vec2F(8.f, 8.f) / (Vec2F)font_size;
    glUseProgram(text_system.program);
    set_uniform("font_pos_scale", text_system.program,
                glUniform2fv, 1, glm::value_ptr(font_pos_scale));

    text_system.vert_fmt.init({
        {2, GL_FLOAT        , false, false},
        {2, GL_UNSIGNED_BYTE, false, false},
        {4, GL_UNSIGNED_BYTE, true , false},
        {4, GL_UNSIGNED_BYTE, true , false}});

    pane_system.program = load_program("glsl/pane.vert", "glsl/pane.frag");

    pane_system.vert_fmt.init({
        {2, GL_FLOAT, false, false},
        {4, GL_FLOAT, true , false}});

    ui_screen = ui_nodes.emplace();
    ui_nodes[ui_screen].tr.scale = {1.f, -1.f, 1.f};
    ui_imgui  = ui_create(ui_screen, 0xff);
    ui_nodes[ui_imgui].io_tick = &imgui_io_tick;
    //@TODO calculate (config?)
    ui_world  = ui_create(ui_screen);
    ui_nodes[ui_world].tr.scale = {1.f / 15.f, 1.f / 15.f, -1.f / 16.f};
    ui_nodes[ui_world].fixed_aspect = true;
    ui_camera = ui_create(ui_world);

    ui_hud = ui_create(ui_screen);
    ui_nodes[ui_hud].tr.scale = {1.f, 1.f, 1.f};

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(glfw_window, false);
    ImGui_ImplOpenGL3_Init(nullptr);
    ImGui::StyleColorsClassic();
}

void ui_deinit() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ui_erase(ui_screen);
    LUX_ASSERT(ui_nodes.size() == 0);
    LUX_ASSERT(ui_texts.size() == 0);
    LUX_ASSERT(ui_panes.size() == 0);
}

static void text_io_tick(U32 id, Transform const& tr, IoContext&) {
    auto& text = ui_texts[id];
    static DynArr<TextSystem::Vert> verts;
    static DynArr<U32>               idxs;
    idxs.resize(text.buff.len * 6);
    verts.resize(text.buff.len * 4);
    Vec4<U8> fg_col = {0xFF, 0xFF, 0xFF, 0xFF};
    Vec4<U8> bg_col = {0x00, 0x00, 0x00, 0x00};
    bool fg = false;
    bool bg = false;
    Vec2F off = {0, 0};
    bool special = false;
    U32 quad_len = 0;
    for(auto character : text.buff) {
        if(character == '\\') {
            special = true;
            continue;
        }
        if(special == true) {
            if(fg || bg) {
                Vec4<U8> col;
                if(character >= '0' && character <= '9') {
                    character -= '0';
                } else if(character >= 'a' && character <= 'f') {
                    character -= 'a';
                    character += 0xa;
                } else {
                    fg = false;
                    bg = false;
                }
                col = Vec4F( character & 0b0001,
                            (character & 0b0010) >> 1,
                            (character & 0b0100) >> 2,
                            (character & 0b1000) >> 3 ? 1.f : 0.25f) * 255.f;
                if(fg) {
                    fg_col = col;
                    fg = false;
                    special = false;
                }
                if(bg) {
                    bg_col = col;
                    bg = false;
                    special = false;
                }
            }
            else switch(character) {
                case '\n':
                off.y += 1.f;
                off.x  = 0.f;
                special = false;
                break;

                case 'f': fg = true; break;
                case 'b': bg = true; break;
                default:
                fg = false;
                bg = false;
                special = false;
                break;
            }
            continue;
        }
        for(Uns i = 0; i < 4; ++i) {
            verts[quad_len * 4 + i] = {
                ((Vec2F)tr.pos + u_quad<F32>[i] + off) * (Vec2F)tr.scale,
                Vec2<U8>(character % 16, character / 16) + u_quad<U8>[i],
                fg_col, bg_col};
        }
        for(Uns i = 0; i < 6; ++i) {
            idxs[quad_len * 6 + i] = quad_len * 4 + quad_idxs<U32>[i];
        }
        ++quad_len;
        off.x += 1.f;
    }
    text.context.bind();
    text.v_buff.bind();
    text.v_buff.write(quad_len * 4, verts.beg, GL_DYNAMIC_DRAW);
    text.i_buff.bind();
    text.i_buff.write(quad_len * 6, idxs.beg, GL_DYNAMIC_DRAW);

    glUseProgram(text_system.program);
    glBindTexture(GL_TEXTURE_2D, text_system.font_texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_TRIANGLES, quad_len * 6, GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);
}

static void pane_io_tick(U32 id, Transform const& tr, IoContext&) {
    Arr<PaneSystem::Vert, 4> verts;
    Arr<U32             , 6> idxs;
    auto& pane = ui_panes[id];
    for(Uns i = 0; i < 4; ++i) {
        verts[i] =
            {((Vec2F)tr.pos + u_quad<F32>[i]) * (Vec2F)tr.scale, pane.bg_col};
    }
    for(Uns i = 0; i < 6; ++i) {
        idxs[i] = quad_idxs<U32>[i];
    }
    pane.context.bind();
    pane.v_buff.bind();
    pane.v_buff.write(4, verts, GL_DYNAMIC_DRAW);
    pane.i_buff.bind();
    pane.i_buff.write(6, idxs, GL_DYNAMIC_DRAW);
    glUseProgram(pane_system.program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);
}

static void imgui_io_tick(U32, Transform const&, IoContext&) {
    {   ImGui::Begin("debug info");
        ImGui::Text("pos: {%.2f, %.2f, %.2f}",
            last_player_pos.x, last_player_pos.y, last_player_pos.z);
        ChkPos chk_pos = to_chk_pos(floor(last_player_pos));
        ImGui::Text("chk_pos: {%zd, %zd, %zd}",
            chk_pos.x, chk_pos.y, chk_pos.z);
        IdxPos idx_pos = to_idx_pos(floor(last_player_pos));
        ImGui::Text("idx_pos: {%u, %u, %u}",
            idx_pos.x, idx_pos.y, idx_pos.z);
        ChkIdx chk_idx = to_chk_idx(idx_pos);
        ImGui::Text("chk_idx: {%u}", chk_idx);
        ImGui::End();
    }
}

void ui_mouse(Vec2F pos, int button, int action) {
    io_context.mouse_events.push({pos, button, action});
}

void ui_scroll(Vec2F pos, F64 off) {
    io_context.scroll_events.push({pos, off});
}

void ui_key(int key, int action) {
    io_context.key_events.push({key, action});
}

static void ui_io_tick(UiId id, Transform const& tr) {
    UiNode* ui = ui_nodes.at(id);
    Vec3F total_scale = tr.scale * ui->tr.scale;
    if(ui->io_tick != nullptr) {
        //@TODO mouse needs a negative translation in transform??
        //@TODO mouse gets untransformed coords btw...
        ui->io_tick(ui->ext_id,
            {(ui->tr.pos + tr.pos) / ui->tr.scale, total_scale}, io_context);
    }
    for(auto it = ui->children.begin(); it != ui->children.end(); ++it) {
        ui_io_tick(*it, {(ui->tr.pos + tr.pos) / ui->tr.scale, total_scale});
    }
}

void ui_io_tick() {
    static bool cursor_disabled = true;
    //@TODO placeholder
    if(glfwGetKey(glfw_window, GLFW_KEY_TAB)) {
        if(cursor_disabled) {
            glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        cursor_disabled = !cursor_disabled;
    }
    Vec2<F64> mouse_pos;
    glfwGetCursorPos(glfw_window, &mouse_pos.x, &mouse_pos.y);
    io_context.mouse_pos = (Vec2F)mouse_pos;
    io_context.win = glfw_window;
    ui_io_tick(ui_screen, {{0, 0, 0}, {1, 1, 1}});
    ui_nodes.free_slots();
    ui_texts.free_slots();
    ui_panes.free_slots();
    io_context.mouse_events.clear();
    io_context.scroll_events.clear();
    io_context.key_events.clear();
}
