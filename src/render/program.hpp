#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
//
#include <alias/int.hpp>
#include <alias/string.hpp>

namespace render
{

class Program
{
    public:
    Program() = default;
    ~Program();

    void init(String vert_path, String frag_path);
    template<typename F, typename... Args>
    void set_uniform(String name, F gl_fun, Args... args);

    GLuint get_id() const;
    GLuint get_vert_id() const;
    GLuint get_frag_id() const;
    private:
    static const SizeT OPENGL_LOG_SIZE = 512;

    GLuint load_shader(GLenum type, String path);

    GLuint id;
    GLuint vert_id;
    GLuint frag_id;
};

template<typename F, typename... Args>
void Program::set_uniform(String name, F gl_fun, Args... args)
{
    gl_fun(glGetUniformLocation(id, name.c_str()), args...);
}

}
