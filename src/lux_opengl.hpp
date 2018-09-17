#pragma once

#include <config.hpp>
//
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
#   include <glad/glad-2.1.h>
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
#   include <glad/glad-es-2.0.h>
#   define GLFW_INCLUDE_ES2
#endif
#include <GLFW/glfw3.h>
//
#include <lux_shared/common.hpp>

void check_opengl_error();
GLuint load_shader(GLenum type, char const* path);
GLuint load_program(char const* vert_path, char const* frag_path);
GLuint load_texture(char const* path, Vec2UI& size_out);
void generate_mipmaps(GLuint texture_id, U32 max_lvl);

template<typename F, typename... Args>
void set_uniform(char const* name, GLuint program_id,
                 F const& gl_fun, Args const &...args) {
    gl_fun(glGetUniformLocation(program_id, name), args...);
}
