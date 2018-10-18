#include <cstring>
//
#include <include_opengl.hpp>
#include <glm/gtc/type_ptr.hpp>
//
#include <lux_shared/common.hpp>
//
#include <rendering.hpp>
#include <ui.hpp>

static SparseDynArr<TextField> text_fields;

TextHandle create_text(Vec2F pos, Vec2F scale, const char* str) {
    TextHandle handle = text_fields.emplace();
    TextField& text_field = text_fields[handle];
    text_field.pos   = pos;
    text_field.scale = scale;
    SizeT str_sz = std::strlen(str);
    text_field.buff.resize(str_sz);
    std::memcpy(text_field.buff.data(), str, str_sz);
    return handle;
}

void delete_text(TextHandle handle) {
    text_fields.erase(handle);
}

TextField& get_text_field(TextHandle handle) {
    LUX_ASSERT(text_fields.contains(handle));
    return text_fields[handle];
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

    GLuint program;
    GLuint vbo;
    GLuint ebo;
#if defined(LUX_GL_3_3)
    GLuint vao;
#endif

    GLuint font_texture;

    DynArr<Vert> verts;
    DynArr<U32>  idxs;
    struct {
        GLint pos;
        GLint font_pos;
        GLint fg_col;
        GLint bg_col;
    } shader_attribs;
} static text_system;

void ui_window_sz_cb(Vec2U const& window_sz) {
    Vec2F scale = Vec2F(1.f) / (Vec2F)window_sz;
    set_uniform("scale", text_system.program,
                glUniform2fv, 1, glm::value_ptr(scale));
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
    glm::mat4 transform(1.f);
    set_uniform("transform", text_system.program,
                glUniformMatrix4fv, 1, GL_FALSE, glm::value_ptr(transform));

    text_system.shader_attribs.pos =
        glGetAttribLocation(text_system.program, "pos");
    text_system.shader_attribs.font_pos =
        glGetAttribLocation(text_system.program, "font_pos");
    text_system.shader_attribs.fg_col =
        glGetAttribLocation(text_system.program, "fg_col");
    text_system.shader_attribs.bg_col =
        glGetAttribLocation(text_system.program, "bg_col");

    glGenBuffers(1, &text_system.vbo);
    glGenBuffers(1, &text_system.ebo);

#if defined(LUX_GL_3_3)
    glGenVertexArrays(1, &text_system.vao);
    glBindVertexArray(text_system.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_system.ebo);

    glBindBuffer(GL_ARRAY_BUFFER, text_system.vbo);
    glVertexAttribPointer(text_system.shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, pos));
    glVertexAttribPointer(text_system.shader_attribs.font_pos,
        2, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, font_pos));
    glVertexAttribPointer(text_system.shader_attribs.fg_col,
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, fg_col));
    glVertexAttribPointer(text_system.shader_attribs.bg_col,
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, bg_col));
    glEnableVertexAttribArray(text_system.shader_attribs.pos);
    glEnableVertexAttribArray(text_system.shader_attribs.font_pos);
    glEnableVertexAttribArray(text_system.shader_attribs.fg_col);
    glEnableVertexAttribArray(text_system.shader_attribs.bg_col);
#endif
}

void ui_render() {
    text_system.verts.clear();
    text_system.idxs.clear();
    for(auto const& text_field : text_fields) {
        Vec2F off = {0, 0};
        bool special = false;
        for(auto character : text_field.buff) {
            if(character == '\\') {
                special = true;
                continue;
            }
            if(special == true && character == '\n') {
                off.y += 1.f;
                off.x  = 0.f;
                continue;
            }
            for(Uns i = 0; i < 4; ++i) {
                constexpr Vec2I quad[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
                text_system.verts.push_back({
                    text_field.pos + ((Vec2F)quad[i] + off) * text_field.scale,
                    Vec2<U8>(character % 16, character / 16) + (Vec2<U8>)quad[i],
                    Vec4<U8>(0xFF), Vec4<U8>(0x00)});
            }
            for(Uns i = 0; i < 6; ++i) {
                constexpr U32 idxs[6] = {0, 1, 2, 2, 3, 1};
                text_system.idxs.emplace_back(text_system.verts.size() - 4 + idxs[i]);
            }
            off.x += 1.f;
        }
    }
    if(text_system.verts.size() == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, text_system.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TextSystem::Vert) *
        text_system.verts.size(), text_system.verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_system.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(U32) *
        text_system.idxs.size(), text_system.idxs.data(), GL_DYNAMIC_DRAW);

    glUseProgram(text_system.program);
    glBindTexture(GL_TEXTURE_2D, text_system.font_texture);
#if defined(LUX_GLES_2_0)
    glBindBuffer(GL_ARRAY_BUFFER, text_system.vbo);
    glVertexAttribPointer(text_system.shader_attribs.pos,
        2, GL_FLOAT, GL_FALSE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, pos));
    glVertexAttribPointer(text_system.shader_attribs.font_pos,
        2, GL_FLOAT, GL_FALSE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, font_pos));
    glVertexAttribPointer(text_system.shader_attribs.fg_col,
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, fg_col));
    glVertexAttribPointer(text_system.shader_attribs.bg_col,
        3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextSystem::Vert),
        (void*)offsetof(TextSystem::Vert, bg_col));
    glEnableVertexAttribArray(text_system.shader_attribs.pos);
    glEnableVertexAttribArray(text_system.shader_attribs.font_pos);
    glEnableVertexAttribArray(text_system.shader_attribs.fg_col);
    glEnableVertexAttribArray(text_system.shader_attribs.bg_col);
#elif defined(LUX_GL_3_3)
    glBindVertexArray(text_system.vao);
#endif
    glDrawElements(GL_TRIANGLES, text_system.idxs.size(), GL_UNSIGNED_INT, 0);
#if defined(LUX_GLES_2_0)
    glDisableVertexAttribArray(text_system.shader_attribs.pos);
    glDisableVertexAttribArray(text_system.shader_attribs.font_pos);
    glDisableVertexAttribArray(text_system.shader_attribs.fg_col);
    glDisableVertexAttribArray(text_system.shader_attribs.bg_col);
#endif
}
