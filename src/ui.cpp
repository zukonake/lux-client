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

TextHandle create_text(Vec2I pos, F32 scale, const char* str) {
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
    struct Vert {
        Vec2<U16> pos;
        Vec2<U8> font_pos;
        Vec4<U8> fg_col;
        Vec4<U8> bg_col;
    };

    GLuint program;
    GLuint vbo;
    GLuint ebo;
#if defined(LUX_GL_3_3)
    GLuint vao;
#endif

    GLuint font_texture;

    DynArr<Vert> verts;
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
    glUseProgram(text_system.program);

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
#endif
}
