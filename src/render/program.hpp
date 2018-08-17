#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/string.hpp>

namespace render
{

class Program
{
    public:
    Program() = default;

    void init(String const &vert_path, String const &frag_path);
    template<typename F, typename... Args>
    void set_uniform(String const &name, F const &gl_fun, Args const &...args);
    void use();

    GLuint get_id() const;
    GLuint get_vert_id() const;
    GLuint get_frag_id() const;
    private:
    static const SizeT OPENGL_LOG_SIZE = 512;

    GLuint load_shader(GLenum type, String const &path);

    GLuint id;
    GLuint vert_id;
    GLuint frag_id;
};

template<typename F, typename... Args>
void Program::set_uniform(String const &name, F const &gl_fun, Args const &...args)
{
    gl_fun(glGetUniformLocation(id, name.c_str()), args...);
}

}
