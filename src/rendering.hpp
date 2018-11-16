#pragma once

#include <config.hpp>
//
#include <include_opengl.hpp>
#include <GLFW/glfw3.h>
//
#include <lux_shared/common.hpp>

extern GLFWwindow* glfw_window;

Vec2U get_window_size();
Vec2D get_mouse_pos();

void check_opengl_error();
GLuint load_shader(GLenum type, char const* path);
GLuint load_program(char const* vert_path, char const* frag_path);
GLuint load_texture(char const* path, Vec2U& size_out);
F32 get_aim_rotation();

template<typename F, typename... Args>
void set_uniform(char const* name, GLuint program_id,
                 F const& gl_fun, Args const &...args) {
    gl_fun(glGetUniformLocation(program_id, name), args...);
}

void rendering_init();
void rendering_deinit();

namespace gl {
//@CONSIDER m4 macros for generating VertFmt and Vert struct
struct Buff {
    void init();
    void deinit();
    void bind(GLenum target) const;
    template<typename T>
    void write(GLenum target, SizeT len, T const* data, GLenum usage);
    GLuint id;
};

struct VertBuff : Buff {
    void bind() const;
    template<typename T>
    void write(SizeT len, T const* data, GLenum usage);
};

struct IdxBuff : Buff {
    void bind() const;
    template<typename T>
    void write(SizeT len, T const* data, GLenum usage);
};

struct VertFmt {
    struct AttribDef {
        char const* ident;
        Uns    num;
        GLenum type;
        bool   normalize;
    };
    struct Attrib {
        GLint  pos;
        Uns    num;
        GLenum type;
        GLenum normalize;
        void*  off;
    };
    template<SizeT defs_len>
    void init(GLuint program_id, Arr<AttribDef, defs_len> const& attrib_defs);
    DynArr<Attrib> attribs;
    SizeT vert_sz;
};

//@CONSIDER constructors instead of init
struct VertContext {
    void init(VertBuff const& _vert_buff, VertFmt const& _vert_fmt);
    void deinit();
    void bind();
    void unbind();
    void bind_attribs();
    GLuint vao_id;
    VertBuff const* vert_buff;
    VertFmt  const* vert_fmt;
};

template<typename T>
void Buff::write(GLenum target, SizeT len, T const* data, GLenum usage) {
    glBufferData(target, sizeof(T) * len, data, usage);
}

template<typename T>
void VertBuff::write(SizeT len, T const* data, GLenum usage) {
    Buff::write(GL_ARRAY_BUFFER, len, data, usage);
}

template<typename T>
void IdxBuff::write(SizeT len, T const* data, GLenum usage) {
    Buff::write(GL_ELEMENT_ARRAY_BUFFER, len, data, usage);
}

template<SizeT defs_len>
void VertFmt::init(GLuint program_id, Arr<typename VertFmt::AttribDef,
                                          defs_len> const& attrib_defs) {
    vert_sz = 0;
    attribs.resize(defs_len);
    for(Uns i = 0; i < defs_len; i++) {
        auto&    attrib = attribs[i];
        auto const& def = attrib_defs[i];
        attrib.pos  = glGetAttribLocation(program_id, def.ident);
        attrib.num  = def.num;
        attrib.type = def.type;
        attrib.normalize = def.normalize ? GL_TRUE : GL_FALSE;
        attrib.off  = (void*)vert_sz;
        //@TODO compile-time ?
        //@CONSIDER make_attrib that infers the gl stuff from type
        //e.g. Vec2F etc.
        SizeT attrib_sz;
        switch(attrib.type) {
            case GL_FLOAT:          attrib_sz = sizeof(F32); break;
            case GL_DOUBLE:         attrib_sz = sizeof(F64); break;
            case GL_BYTE:           attrib_sz = sizeof(I8);  break;
            case GL_UNSIGNED_BYTE:  attrib_sz = sizeof(U8);  break;
            case GL_SHORT:          attrib_sz = sizeof(I16); break;
            case GL_UNSIGNED_SHORT: attrib_sz = sizeof(U16); break;
            case GL_INT:            attrib_sz = sizeof(I32); break;
            case GL_UNSIGNED_INT:   attrib_sz = sizeof(U32); break;
            default: LUX_UNREACHABLE();
        }
        vert_sz += attrib_sz * attrib.num;
    }
}

} //namespace gl
