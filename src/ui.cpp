#include <config.hpp>
#include <cstdint>
#include <cstring>
#include <string.h>
//
#include <include_opengl.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
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

static HashMap<StrBuff, U16>   rasen_labels_id;
static HashMap<int, NetAction> rasen_tick_bindings;
static HashMap<int, NetAction> rasen_sgnl_bindings;

F32 constexpr DBG_POINT_SIZE = 5.f;
F32 constexpr DBG_LINE_WIDTH = 2.f;
F32 constexpr DBG_ARROW_HEAD_LEN = 0.2f;

UiId ui_screen;
UiId ui_world;
UiId ui_camera;
UiId ui_dbg_shapes;
UiId ui_hud;
UiId ui_imgui;

SparseDynArr<UiNode, U16> ui_nodes;
SparseDynArr<UiText, U16> ui_texts;
SparseDynArr<UiPane, U16> ui_panes;

static void text_io_tick(U32, Transform const&, IoContext&);
static void text_deinit(U32);
static void pane_io_tick(U32, Transform const&, IoContext&);
static void pane_deinit(U32);
static void dbg_shapes_io_tick(U32, Transform const&, IoContext&);
static void imgui_io_tick(U32, Transform const&, IoContext&);

UiId ui_create(UiId parent, U8 priority) {
    UiId id = ui_nodes.emplace();
    LUX_LOG_DBG("creating UI #%u", id);
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

struct DbgShapesSystem {
#pragma pack(push, 1)
    struct Vert {
        Vec2F pos;
        Vec4F col;
    };
#pragma pack(pop)

    GLuint program;
    gl::VertBuff    v_buff;
    gl::IdxBuff     i_buff;
    gl::VertContext context;
    gl::VertFmt     vert_fmt;

    DynArr<Vert> verts;
    DynArr<U32>  idxs;
} static dbg_shapes_system;

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

    text_system.vert_fmt.init(text_system.program, {
        {"pos"     , 2, GL_FLOAT        , false, false},
        {"font_pos", 2, GL_UNSIGNED_BYTE, false, false},
        {"fg_col"  , 4, GL_UNSIGNED_BYTE, true , false},
        {"bg_col"  , 4, GL_UNSIGNED_BYTE, true , false}});

    pane_system.program = load_program("glsl/pane.vert", "glsl/pane.frag");
    glUseProgram(pane_system.program);

    pane_system.vert_fmt.init(pane_system.program, {
        {"pos"     , 2, GL_FLOAT, false, false},
        {"bg_col"  , 4, GL_FLOAT, true , false}});

    dbg_shapes_system.program = load_program("glsl/color_shape.vert",
                                             "glsl/color_shape.frag");
    glUseProgram(dbg_shapes_system.program);
    dbg_shapes_system.vert_fmt.init(dbg_shapes_system.program, {
        {"pos", 2, GL_FLOAT, false, false},
        {"col", 4, GL_FLOAT, true , false}});
    dbg_shapes_system.v_buff.init();
    dbg_shapes_system.i_buff.init();
    dbg_shapes_system.context.init({dbg_shapes_system.v_buff},
                                   dbg_shapes_system.vert_fmt);

    ui_screen = ui_nodes.emplace();
    ui_nodes[ui_screen].tr.scale = {1.f, -1.f};
    ui_imgui  = ui_create(ui_screen, 0xff);
    ui_nodes[ui_imgui].io_tick = &imgui_io_tick;
    //@TODO calculate (config?)
    ui_world  = ui_create(ui_screen);
    ui_nodes[ui_world].tr.scale = {1.f / 5.f, 1.f / 5.f};
    ui_nodes[ui_world].fixed_aspect = true;
    ui_camera = ui_create(ui_world);
    ui_dbg_shapes = ui_create(ui_camera, 0x80);
    ui_nodes[ui_dbg_shapes].io_tick = &dbg_shapes_io_tick;

    ui_hud = ui_create(ui_screen);
    ui_nodes[ui_hud].tr.scale = {1.f, 1.f};

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

static void text_io_tick(U32 id, Transform const& tr, IoContext& context) {
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
                (tr.pos + u_quad<F32>[i] + off) * tr.scale,
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

static void dbg_shapes_io_tick(U32, Transform const& tr, IoContext& context) {
    //@IMPROVE we could optimize by sharing verts between borders and fills
    //@CONSIDER static buffs as in text rendering
    auto& verts = dbg_shapes_system.verts;
    auto& idxs  = dbg_shapes_system.idxs;
    verts.clear();
    idxs.clear();
    Uns lines_start;
    Uns triangles_start;
    //@TODO replace this?
#define ADD_VERT(off) \
        verts.push({off, shape.col});
    for(auto const& shape : ss_tick.dbg_inf.shapes) {
        switch(shape.tag) {
            case NetSsTick::DbgInf::Shape::POINT: {
                idxs.emplace(verts.len);
                ADD_VERT(shape.point.pos);
                break;
            }
            default: break;
        }
    }
    lines_start = idxs.len;
    for(auto const& shape : ss_tick.dbg_inf.shapes) {
        switch(shape.tag) {
            case NetSsTick::DbgInf::Shape::LINE: {
                constexpr U32 order[] = {0, 1};
                for(auto const& idx : order) {
                    idxs.emplace(idx + verts.len);
                }
                ADD_VERT(shape.line.beg);
                ADD_VERT(shape.line.end);
                break;
            }
            case NetSsTick::DbgInf::Shape::ARROW: {
                if(shape.arrow.beg == shape.arrow.end) break;
                constexpr U32 order[] = {0, 1, 1, 2, 1, 3};
                for(auto const& idx : order) {
                    idxs.emplace(idx + verts.len);
                }
                ADD_VERT(shape.arrow.beg);
                ADD_VERT(shape.arrow.end);
                Vec2F d = glm::normalize(shape.arrow.beg - shape.arrow.end);
                ADD_VERT(shape.arrow.end +
                    glm::rotate(d, tau / -8.f) * DBG_ARROW_HEAD_LEN);
                ADD_VERT(shape.arrow.end +
                    glm::rotate(d, tau /  8.f) * DBG_ARROW_HEAD_LEN);
                break;
            }
            case NetSsTick::DbgInf::Shape::CROSS: {
                constexpr U32 order[] = {0, 1, 2, 3};
                for(auto const& idx : order) {
                    idxs.emplace(idx + verts.len);
                }
                constexpr Vec2F cross[4] =
                    {{-1.f, -1.f}, {1.f, 1.f}, {1.f, -1.f}, {-1.f, 1.f}};
                for(auto const& vert : cross) {
                    ADD_VERT(shape.cross.pos + vert * shape.cross.sz);
                }
                break;
            }
            case NetSsTick::DbgInf::Shape::SPHERE: {
                if(!shape.border) break;
                constexpr U32 order[] = {0, 1, 1, 2, 2, 3, 3, 4,
                                         4, 5, 5, 6, 6, 7, 7, 0};
                for(auto const& idx : order) {
                    idxs.emplace(idx + verts.len);
                }
                constexpr F32 diag = std::sqrt(2.f) / 2.f;
                constexpr Vec2F octagon[] =
                   {{0.f, -1.f}, {diag, -diag}, { 1.f, 0.f}, { diag,  diag},
                    {0.f,  1.f}, {-diag, diag}, {-1.f, 0.f}, {-diag, -diag}};
                for(auto const& vert : octagon) {
                    ADD_VERT(shape.sphere.pos + vert * shape.sphere.rad);
                }
                break;
            }
            case NetSsTick::DbgInf::Shape::TRIANGLE: {
                if(!shape.border) break;
                constexpr U32 order[] = {0, 1, 1, 2, 2, 0};
                for(auto const& idx : order) {
                    idxs.emplace(idx + verts.len);
                }
                for(Uns i = 0; i < 3; ++i) {
                    ADD_VERT(shape.triangle.verts[i]);
                }
                break;
            }
            case NetSsTick::DbgInf::Shape::RECT: {
                if(!shape.border) break;
                constexpr U32 order[] = {0, 1, 1, 2, 2, 3, 3, 0};
                for(auto const& idx : order) {
                    idxs.emplace(idx + verts.len);
                }
                //@TODO rect_points from server
                for(auto const& vert : quad<F32>) {
                    ADD_VERT(shape.rect.pos +
                        glm::rotate(vert, shape.rect.angle) * shape.rect.sz);
                }
                break;
            }
            default: break;
        }
    }
    triangles_start = idxs.len;
    for(auto const& shape : ss_tick.dbg_inf.shapes) {
        switch(shape.tag) {
            case NetSsTick::DbgInf::Shape::SPHERE: {
                constexpr U32 order[] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5,
                                         0, 5, 6, 0, 6, 7, 0, 7, 8, 0, 8, 1};
                for(auto const& idx : order) {
                    idxs.emplace(idx + verts.len);
                }
                constexpr F32 diag = std::sqrt(2.f) / 2.f;
                constexpr Vec2F octagon[] = {{0.f, 0.f},
                    {0.f, -1.f}, {diag, -diag}, { 1.f, 0.f}, { diag,  diag},
                    {0.f,  1.f}, {-diag, diag}, {-1.f, 0.f}, {-diag, -diag}};
                for(auto const& vert : octagon) {
                    ADD_VERT(shape.sphere.pos + vert * shape.sphere.rad);
                }
                break;
            }
            case NetSsTick::DbgInf::Shape::TRIANGLE: {
                if(!shape.border) break;
                constexpr U32 order[] = {0, 1, 2};
                for(auto const& idx : order) {
                    idxs.emplace(idx + verts.len);
                }
                for(Uns i = 0; i < 3; ++i) {
                    ADD_VERT(shape.triangle.verts[i]);
                }
                break;
            }
            case NetSsTick::DbgInf::Shape::RECT: {
                constexpr U32 order[] = {0, 1, 2, 2, 3, 0};
                for(auto const& idx : order) {
                    idxs.emplace(idx + verts.len);
                }
                for(auto const& vert : quad<F32>) {
                    ADD_VERT(shape.rect.pos +
                        glm::rotate(vert, shape.rect.angle) * shape.rect.sz);
                }
                break;
            }
            default: break;
        }
    }
#undef ADD_VERT
    dbg_shapes_system.context.bind();
    dbg_shapes_system.v_buff.bind();
    dbg_shapes_system.v_buff.write(verts.len, verts.beg, GL_DYNAMIC_DRAW);
    dbg_shapes_system.i_buff.bind();
    dbg_shapes_system.i_buff.write(idxs.len, idxs.beg, GL_DYNAMIC_DRAW);

    glUseProgram(dbg_shapes_system.program);
    set_uniform("scale", dbg_shapes_system.program,
                glUniform2fv, 1, glm::value_ptr(tr.scale));
    set_uniform("translation", dbg_shapes_system.program,
                glUniform2fv, 1, glm::value_ptr(tr.pos));
    glEnable(GL_BLEND);
#if defined(LUX_GL_3_3)
    //@TODO perhaps we should render those as quads?
    glPointSize(DBG_POINT_SIZE);
#endif
    glLineWidth(DBG_LINE_WIDTH);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    SizeT triangles_sz = idxs.len - triangles_start;
    if(triangles_sz != 0) {
        glDrawElements(GL_TRIANGLES, triangles_sz, GL_UNSIGNED_INT,
                       (void*)(sizeof(U32) * triangles_start));
    }
    //@TODO this number might be too high, check it
    SizeT lines_sz = triangles_start;
    if(lines_sz != 0) {
        glDrawElements(GL_LINES, lines_sz, GL_UNSIGNED_INT,
                       (void*)(sizeof(U32) * lines_start));
    }
    SizeT points_sz = lines_start;
    if(points_sz != 0) {
        glDrawElements(GL_POINTS, points_sz, GL_UNSIGNED_INT, 0);
    }
    glDisable(GL_BLEND);
    glLineWidth(1.f);
#if defined(LUX_GL_3_3)
    glPointSize(1.f);
#endif
}

static void pane_io_tick(U32 id, Transform const& tr, IoContext& context) {
    Arr<PaneSystem::Vert, 4> verts;
    Arr<U32             , 6> idxs;
    auto& pane = ui_panes[id];
    for(Uns i = 0; i < 4; ++i) {
        verts[i] ={(tr.pos + u_quad<F32>[i]) * tr.scale, pane.bg_col};
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

static void imgui_io_tick(U32, Transform const&, IoContext& context) {
    for(Uns i = 0; i < context.key_events.len;) {
        auto const& event = context.key_events[i];
        if(event.action == GLFW_PRESS &&
           rasen_sgnl_bindings.count(event.key) > 0) {
            cs_sgnl.actions.emplace(rasen_sgnl_bindings.at(event.key));
            context.key_events.erase(i);
        } else ++i;
    }
    for(auto const& binding : rasen_tick_bindings) {
        if(glfwGetKey(glfw_window, binding.first)) {
            cs_tick.actions.emplace(binding.second);
        }
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    {   ImGui::Begin("assembly editor");
        static char label[32] = {};
        ImGui::InputText("label", label, arr_len(label));
        static char text[256] = {};
        ImGui::InputTextMultiline("", text, arr_len(text), {0, 256});
        if(ImGui::Button("assemble")) {
            Str label_str = {label, strnlen(label, arr_len(label))};
            Str text_str  = {text , strnlen(text , arr_len(text ))};
            if(client_send_assembly(label_str, text_str) != LUX_OK) {
                //@TODO
            }
        }
        ImGui::End();
    }
    {   ImGui::Begin("bindings");

        static char label[32] = {};
        static int stack_val;
        static DynArr<U8> stack;
        static int key = GLFW_KEY_UNKNOWN;
        static bool continuous = false;

        ImGui::InputText("label", label, arr_len(label));
        ImGui::Checkbox("continuous", &continuous);
        if(ImGui::Button("add")) {
            const char* c_str = glfwGetKeyName(key, 0);
            if(c_str != nullptr) {
                Str label_str = {label, strnlen(label, arr_len(label))};
                if(rasen_labels_id.count(label_str) == 0) {
                    LUX_LOG_ERR("undefined label \"%.*s\"", (int)arr_len(label),
                                label);
                } else {
                    if(continuous) {
                        ui_add_continuous_binding(label_str, key, stack);
                    } else {
                        ui_add_discrete_binding(label_str, key, stack);
                    }
                    stack.clear();
                }
                key = GLFW_KEY_UNKNOWN;
            }
        }
        ImGui::SameLine();
        ImGui::Button("key (hover)");
        if(ImGui::IsItemHovered()) {
            if(context.key_events.len > 0 &&
               context.key_events.last().action == GLFW_PRESS) {
                key = context.key_events.last().key;
                context.key_events.pop();
            }
        }
        const char* c_str = glfwGetKeyName(key, 0);
        ImGui::SameLine();
        ImGui::Text("current: %s", c_str);
        ImGui::InputInt("val", &stack_val);
        if(ImGui::Button("push")) {
            stack.push(stack_val);
        }
        ImGui::SameLine();
        if(ImGui::Button("pop") && stack.len > 0) {
            stack.pop();
        }
        {   ImGui::BeginChild("info");
            ImGui::Text("stack");
            for(auto const& val : stack) {
                ImGui::SameLine();
                ImGui::Text("%d", val);
            }
            ImGui::Separator();
            ImGui::Text("label list");
            for(auto const& label : rasen_labels_id) {
                ImGui::Text("%.*s -> 0x%03x", (int)label.first.len,
                            label.first.beg, label.second);
            }
            ImGui::Separator();
            ImGui::Text("bind list");
            for(auto const& bind : rasen_tick_bindings) {
                const char* c_str = glfwGetKeyName(bind.first, 0);
                if(c_str != nullptr) {
                    ImGui::Text("cont. %s -> 0x%03x",
                        c_str, bind.second.id);
                }
            }
            for(auto const& bind : rasen_sgnl_bindings) {
                const char* c_str = glfwGetKeyName(bind.first, 0);
                if(c_str != nullptr) {
                    ImGui::Text("%s -> 0x%03x",
                        c_str, bind.second.id);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
    Vec2F total_scale = tr.scale * ui->tr.scale;
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
    ui_io_tick(ui_screen, {{0.f, 0.f}, {1.f, 1.f}});
    ui_nodes.free_slots();
    ui_texts.free_slots();
    ui_panes.free_slots();
    io_context.mouse_events.clear();
    io_context.scroll_events.clear();
    io_context.key_events.clear();
}

LUX_MAY_FAIL ui_add_rasen_label(NetRasenLabel const& label) {
    rasen_labels_id[label.str_id] = label.id;
    return LUX_OK;
}

bool ui_has_rasen_label(Str const& str_id) {
    return rasen_labels_id.count(str_id) > 0;
}

void ui_add_continuous_binding(Str const& str_id, int key, Slice<U8> const& stack) {
    LUX_ASSERT(ui_has_rasen_label(str_id));
    rasen_tick_bindings[key] = {stack, rasen_labels_id.at(str_id)};
}

void ui_add_discrete_binding(Str const& str_id, int key, Slice<U8> const& stack) {
    LUX_ASSERT(ui_has_rasen_label(str_id));
    rasen_sgnl_bindings[key] = {stack, rasen_labels_id.at(str_id)};
}
