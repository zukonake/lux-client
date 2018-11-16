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
UiId ui_dbg_shapes;
UiId ui_hud;

SparseDynArr<UiElement, U16> ui_elems;
SparseDynArr<UiText   , U16> ui_texts;
SparseDynArr<UiPane   , U16> ui_panes;

static void render_text(void*, Vec2F const&, Vec2F const&);
static void render_pane(void*, Vec2F const&, Vec2F const&);
static void render_dbg_shapes(void* ptr, Vec2F const& pos, Vec2F const& scale);

UiId new_ui() {
    UiId id = ui_elems.emplace();
    return id;
}

UiId new_ui(UiId parent, U8 priority) {
    UiId id = ui_elems.emplace();
    LUX_ASSERT(ui_elems.contains(parent));
    ui_elems[id].priority = priority;
    ui_elems[parent].children.emplace_back(id);
    std::sort(ui_elems[parent].children.begin(),
              ui_elems[parent].children.end(),
              [](UiId const& a, UiId const& b) {
                  if(ui_elems.contains(a) && ui_elems.contains(b)) {
                      return ui_elems[a].priority < ui_elems[b].priority;
                  }
                  return false;
              });
    return id;
}

void erase_ui(UiId id) {
    LUX_ASSERT(ui_elems.contains(id));
    for(UiId child : ui_elems[id].children) {
        erase_ui(child);
    }
    ui_elems.erase(id);
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

TextId create_text(Vec2F pos, Vec2F scale, const char* str, UiId parent) {
    TextId id  = ui_texts.emplace();
    auto& text = ui_texts[id];
    text.ui    = new_ui(parent);
    text.v_buff.init();
    text.i_buff.init();
    text.context.init({text.v_buff}, text_system.vert_fmt);
    auto& ui   = ui_elems[text.ui];
    ui.render  = &render_text;
    ui.pos     = pos;
    ui.scale   = scale;
    ui.ptr     = (void*)(std::uintptr_t)id;
    ui.fixed_aspect = true;
    SizeT str_sz = std::strlen(str);
    text.buff.resize(str_sz);
    std::memcpy(text.buff.data(), str, str_sz);
    return id;
}

void erase_text(TextId id) {
    LUX_ASSERT(ui_texts.contains(id));
    auto& text = ui_texts[id];
    text.v_buff.deinit();
    text.i_buff.deinit();
    text.context.deinit();
    erase_ui(ui_texts[id].ui);
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

PaneId create_pane(Vec2F pos, Vec2F scale, Vec2F size,
                       Vec4F const& bg_col, UiId parent) {
    PaneId id  = ui_panes.emplace();
    auto& pane = ui_panes[id];
    pane.ui    = new_ui(parent);
    pane.v_buff.init();
    pane.i_buff.init();
    pane.context.init({pane.v_buff}, pane_system.vert_fmt);
    auto& ui   = ui_elems[pane.ui];
    ui.render  = &render_pane;
    ui.pos     = pos;
    ui.scale   = scale;
    ui.ptr     = (void*)(std::uintptr_t)id;
    ui.fixed_aspect = true;
    pane.size   = size;
    pane.bg_col = bg_col;
    return id;
}

void erase_pane(PaneId id) {
    LUX_ASSERT(ui_panes.contains(id));
    auto& pane = ui_panes[id];
    pane.v_buff.deinit();
    pane.i_buff.deinit();
    pane.context.deinit();
    erase_ui(ui_panes[id].ui);
    ui_panes.erase(id);
}

static void update_aspect(UiId id, F32 w_to_h) {
    auto& ui = ui_elems[id];
    if(ui.fixed_aspect) {
        ui.scale.x = ui.scale.y * w_to_h;
    } else {
        for(auto& child : ui.children) {
            update_aspect(child, w_to_h);
        }
    }
}

void ui_window_sz_cb(Vec2U const& sz) {
    update_aspect(ui_screen, (F32)sz.y / (F32)sz.x);
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

    ui_screen = new_ui();
    ui_elems[ui_screen].scale = {1.f, -1.f};
    ui_world  = new_ui(ui_screen);
    ui_dbg_shapes = new_ui(ui_world, -1);
    ui_elems[ui_dbg_shapes].render = &render_dbg_shapes;
    //@TODO calculate
    ui_elems[ui_world].scale = {1.f / 5.f, 1.f / 5.f};
    ui_elems[ui_world].fixed_aspect = true;

    ui_hud = new_ui(ui_screen);
    ui_elems[ui_hud].scale = {0.01f, 0.01f};
}

void ui_deinit() {
    erase_ui(ui_screen);
}

static void render_text(void* ptr, Vec2F const& pos, Vec2F const& scale) {
    TextId text_id = (TextId)(std::uintptr_t)ptr;
    auto& text = ui_texts[text_id];
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
                pos + ((Vec2F)quad[i] + off) * scale,
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

static void render_dbg_shapes(void*, Vec2F const& pos, Vec2F const& scale) {
    //@IMPROVE we could optimize by sharing verts between borders and fills
    auto& verts = dbg_shapes_system.verts;
    auto& idxs  = dbg_shapes_system.idxs;
    verts.clear();
    idxs.clear();
    Uns lines_start;
    Uns triangles_start;
    for(auto const& shape : ss_tick.dbg_inf.shapes) {
        auto add_vert = [&](Vec2F off) {
            verts.push_back({pos + off * scale, shape.col});
        };
        switch(shape.tag) {
            case NetSsTick::DbgInf::Shape::POINT: {
                idxs.emplace_back(verts.size());
                add_vert(shape.point.pos);
                break;
            }
            default: break;
        }
    }
    lines_start = idxs.size();
    for(auto const& shape : ss_tick.dbg_inf.shapes) {
        auto add_vert = [&](Vec2F off) {
            verts.push_back({pos + off * scale, shape.col});
        };
        switch(shape.tag) {
            case NetSsTick::DbgInf::Shape::LINE: {
                constexpr U32 order[] = {0, 1};
                for(auto const& idx : order) {
                    idxs.emplace_back(idx + verts.size());
                }
                add_vert(shape.line.beg);
                add_vert(shape.line.end);
                break;
            }
            case NetSsTick::DbgInf::Shape::ARROW: {
                if(shape.arrow.beg == shape.arrow.end) break;
                constexpr U32 order[] = {0, 1, 1, 2, 1, 3};
                for(auto const& idx : order) {
                    idxs.emplace_back(idx + verts.size());
                }
                add_vert(shape.arrow.beg);
                add_vert(shape.arrow.end);
                Vec2F d = glm::normalize(shape.arrow.beg - shape.arrow.end);
                add_vert(shape.arrow.end +
                    glm::rotate(d, tau / -8.f) * DBG_ARROW_HEAD_LEN);
                add_vert(shape.arrow.end +
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
                    add_vert(shape.cross.pos + vert * shape.cross.sz);
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
                    add_vert(shape.sphere.pos + vert * shape.sphere.rad);
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
                    add_vert(shape.triangle.verts[i]);
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
                    add_vert(shape.rect.pos +
                        glm::rotate(vert, shape.rect.angle) * shape.rect.sz);
                }
                break;
            }
            default: break;
        }
    }
    triangles_start = idxs.size();
    for(auto const& shape : ss_tick.dbg_inf.shapes) {
        auto add_vert = [&](Vec2F off) {
            verts.push_back({pos + off * scale, shape.col});
        };
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
                    add_vert(shape.sphere.pos + vert * shape.sphere.rad);
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
                    add_vert(shape.triangle.verts[i]);
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
                    add_vert(shape.rect.pos +
                        glm::rotate(vert, shape.rect.angle) * shape.rect.sz);
                }
                break;
            }
            default: break;
        }
    }
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

static void render_pane(void* ptr, Vec2F const& pos, Vec2F const& scale) {
    Arr<PaneSystem::Vert, 4> verts;
    Arr<U32             , 6> idxs;
    PaneId pane_id = (PaneId)(std::uintptr_t)ptr;
    auto& pane = ui_panes[pane_id];
    for(Uns i = 0; i < 4; ++i) {
        constexpr Vec2F quad[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        verts[i] ={pos + quad[i] * scale * pane.size, pane.bg_col};
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

static void ui_render(UiId id, Vec2F const& pos, Vec2F const& scale) {
    UiElement* ui = ui_elems.at(id);
    Vec2F total_scale = scale * ui->scale;
    if(ui->render != nullptr) {
        (*ui->render)(ui->ptr, ui->pos * scale + pos, total_scale);
        ui = ui_elems.at(id);
        if(ui == nullptr) return;
    }
    for(auto it = ui->children.begin(); it != ui->children.end();) {
        if(!ui_elems.contains(*it)) {
            it = ui->children.erase(it);
        } else {
            ui_render(*it, ui->pos * scale + pos, total_scale);
            ui = ui_elems.at(id);
            if(ui == nullptr) return;
            ++it;
        }
    }
}

void ui_render() {
    ui_render(ui_screen, {0.f, 0.f}, {1.f, 1.f});
    ui_elems.free_slots();
    ui_texts.free_slots();
    ui_panes.free_slots();
}
