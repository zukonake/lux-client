#include <config.hpp>
#include <cstdint>
#include <cstring>
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

F32 constexpr DBG_POINT_SIZE = 5.f;
F32 constexpr DBG_LINE_WIDTH = 2.f;
F32 constexpr DBG_ARROW_HEAD_LEN = 0.2f;

UiId ui_screen;
UiId ui_world;
UiId ui_camera;
UiId ui_dbg_shapes;
UiId ui_hud;

SparseDynArr<UiNode, U16> ui_nodes;
SparseDynArr<UiText, U16> ui_texts;
SparseDynArr<UiPane, U16> ui_panes;

static void text_render(U32, Transform const&);
static void text_deinit(U32);
static void pane_render(U32, Transform const&);
static void pane_deinit(U32);
static void dbg_shapes_render(U32, Transform const&);

UiId ui_create(UiId parent, U8 priority) {
    UiId id = ui_nodes.emplace();
    ui_nodes[id].priority = priority;
    LUX_ASSERT(ui_nodes.contains(parent));
    ui_nodes[parent].children.emplace_back(id);
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

UiTextId ui_text_create(UiId parent, Transform const& tr, const char* str) {
    UiTextId id = ui_texts.emplace();
    auto& text  = ui_texts[id];
    text.ui     = ui_create(parent);
    text.v_buff.init();
    text.i_buff.init();
    text.context.init({text.v_buff}, text_system.vert_fmt);
    auto& ui  = ui_nodes[text.ui];
    ui.deinit = &text_deinit;
    ui.render = &text_render;
    ui.tr     = tr;
    ui.ext_id = id;
    ui.fixed_aspect = true;
    SizeT str_sz = std::strlen(str);
    text.buff.resize(str_sz);
    std::memcpy(text.buff.data(), str, str_sz);
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
    ui.render = &pane_render;
    ui.tr     = tr;
    ui.ext_id = id;
    ui.fixed_aspect = false;
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

    dbg_shapes_system.program = load_program("glsl/dbg_shapes.vert",
                                             "glsl/dbg_shapes.frag");
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
    ui_dbg_shapes = ui_create(ui_world, 0x80);
    ui_nodes[ui_dbg_shapes].render = &dbg_shapes_render;
    //@TODO calculate (config?)
    ui_world  = ui_create(ui_screen);
    ui_nodes[ui_world].tr.scale = {1.f / 5.f, 1.f / 5.f};
    ui_nodes[ui_world].fixed_aspect = true;
    ui_camera = ui_create(ui_world);

    ui_hud = ui_create(ui_screen);
    ui_nodes[ui_hud].tr.scale = {1.f, 1.f};
}

void ui_deinit() {
    ui_erase(ui_screen);
    LUX_ASSERT(ui_nodes.size() == 0);
    LUX_ASSERT(ui_texts.size() == 0);
    LUX_ASSERT(ui_panes.size() == 0);
}

static void text_render(U32 id, Transform const& tr) {
    auto& text = ui_texts[id];
    static DynArr<TextSystem::Vert> verts;
    static DynArr<U32>               idxs;
    idxs.resize(text.buff.size() * 6);
    verts.resize(text.buff.size() * 4);
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
            constexpr Vec2I quad[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
            verts[quad_len * 4 + i] = {
                (tr.pos + (Vec2F)quad[i] + off) * tr.scale,
                Vec2<U8>(character % 16, character / 16) + (Vec2<U8>)quad[i],
                fg_col, bg_col};
        }
        for(Uns i = 0; i < 6; ++i) {
            constexpr U32 order[6] = {0, 1, 2, 2, 3, 1};
            idxs[quad_len * 6 + i] = quad_len * 4 + order[i];
        }
        ++quad_len;
        off.x += 1.f;
    }
    text.context.bind();
    text.v_buff.bind();
    text.v_buff.write(quad_len * 4, verts.data(), GL_DYNAMIC_DRAW);
    text.i_buff.bind();
    text.i_buff.write(quad_len * 6, idxs.data(), GL_DYNAMIC_DRAW);

    glUseProgram(text_system.program);
    glBindTexture(GL_TEXTURE_2D, text_system.font_texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_TRIANGLES, quad_len * 6, GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);
    text.context.unbind();
}

static void dbg_shapes_render(U32, Transform const& tr) {
    //@IMPROVE we could optimize by sharing verts between borders and fills
    //@CONSIDER static buffs as in text rendering
    auto& verts = dbg_shapes_system.verts;
    auto& idxs  = dbg_shapes_system.idxs;
    verts.clear();
    idxs.clear();
    Uns lines_start;
    Uns triangles_start;
#define ADD_VERT(off) \
        verts.push_back({(tr.pos + off) * tr.scale, shape.col})
    for(auto const& shape : ss_tick.dbg_inf.shapes) {
        switch(shape.tag) {
            case NetSsTick::DbgInf::Shape::POINT: {
                idxs.emplace_back(verts.size());
                ADD_VERT(shape.point.pos);
                break;
            }
            default: break;
        }
    }
    lines_start = idxs.size();
    for(auto const& shape : ss_tick.dbg_inf.shapes) {
        switch(shape.tag) {
            case NetSsTick::DbgInf::Shape::LINE: {
                constexpr U32 order[] = {0, 1};
                for(auto const& idx : order) {
                    idxs.emplace_back(idx + verts.size());
                }
                ADD_VERT(shape.line.beg);
                ADD_VERT(shape.line.end);
                break;
            }
            case NetSsTick::DbgInf::Shape::ARROW: {
                if(shape.arrow.beg == shape.arrow.end) break;
                constexpr U32 order[] = {0, 1, 1, 2, 1, 3};
                for(auto const& idx : order) {
                    idxs.emplace_back(idx + verts.size());
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
                    idxs.emplace_back(idx + verts.size());
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
                    idxs.emplace_back(idx + verts.size());
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
                    idxs.emplace_back(idx + verts.size());
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
                    idxs.emplace_back(idx + verts.size());
                }
                //@TODO rect_points from server
                constexpr Vec2F quad[4] =
                    {{-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}};
                for(auto const& vert : quad) {
                    ADD_VERT(shape.rect.pos +
                        glm::rotate(vert, shape.rect.angle) * shape.rect.sz);
                }
                break;
            }
            default: break;
        }
    }
    triangles_start = idxs.size();
    for(auto const& shape : ss_tick.dbg_inf.shapes) {
        switch(shape.tag) {
            case NetSsTick::DbgInf::Shape::SPHERE: {
                constexpr U32 order[] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5,
                                         0, 5, 6, 0, 6, 7, 0, 7, 8, 0, 8, 1};
                for(auto const& idx : order) {
                    idxs.emplace_back(idx + verts.size());
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
                    idxs.emplace_back(idx + verts.size());
                }
                for(Uns i = 0; i < 3; ++i) {
                    ADD_VERT(shape.triangle.verts[i]);
                }
                break;
            }
            case NetSsTick::DbgInf::Shape::RECT: {
                constexpr U32 order[] = {0, 1, 2, 2, 3, 0};
                for(auto const& idx : order) {
                    idxs.emplace_back(idx + verts.size());
                }
                constexpr Vec2F quad[4] =
                    {{-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}};
                for(auto const& vert : quad) {
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
    dbg_shapes_system.v_buff.write(verts.size(), verts.data(), GL_DYNAMIC_DRAW);
    dbg_shapes_system.i_buff.bind();
    dbg_shapes_system.i_buff.write(idxs.size(), idxs.data(), GL_DYNAMIC_DRAW);

    glUseProgram(dbg_shapes_system.program);
    glEnable(GL_BLEND);
    glPointSize(DBG_POINT_SIZE);
    glLineWidth(DBG_LINE_WIDTH);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    SizeT triangles_sz = idxs.size() - triangles_start;
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
    glPointSize(1.f);
    dbg_shapes_system.context.unbind();
}

static void pane_render(U32 id, Transform const& tr) {
    Arr<PaneSystem::Vert, 4> verts;
    Arr<U32             , 6> idxs;
    auto& pane = ui_panes[id];
    for(Uns i = 0; i < 4; ++i) {
        constexpr Vec2F quad[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        verts[i] ={(tr.pos + quad[i]) * tr.scale, pane.bg_col};
    }
    for(Uns i = 0; i < 6; ++i) {
        constexpr U32 order[6] = {0, 1, 2, 2, 3, 1};
        idxs[i] = order[i];
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
    pane.context.unbind();
}

static void ui_render(UiId id, Transform const& tr) {
    UiNode* ui = ui_nodes.at(id);
    Vec2F total_scale = tr.scale * ui->tr.scale;
    if(ui->render != nullptr) {
        (*ui->render)(ui->ext_id, {(ui->tr.pos + tr.pos) / ui->tr.scale,
                      total_scale});
    }
    for(auto it = ui->children.begin(); it != ui->children.end();) {
        if(!ui_nodes.contains(*it)) {
            it = ui->children.erase(it);
        } else {
            ui_render(*it, {(ui->tr.pos + tr.pos) / ui->tr.scale, total_scale});
            ++it;
        }
    }
}

static bool ui_mouse_button(UiId id, Transform const& tr, int button,
                            int action) {
    return false;
}

static bool ui_scroll(UiId id, Transform const& tr, F64 off) {
    return false;
}

bool ui_mouse_button(Vec2F pos, int button, int action) {
    return ui_mouse_button(ui_screen, {pos, {1.f, 1.f}}, button, action);
}

bool ui_scroll(Vec2F pos, F64 off) {
    return ui_scroll(ui_screen, {pos, {1.f, 1.f}}, off);
}

void ui_render() {
    ui_render(ui_screen, {{0.f, 0.f}, {1.f, 1.f}});
    ui_nodes.free_slots();
    ui_texts.free_slots();
    ui_panes.free_slots();
}
