#pragma once

#include <include_opengl.hpp>
#include <include_glfw.hpp>
//
#include <lux_shared/common.hpp>

extern GLFWwindow* glfw_window;

void check_opengl_error();
GLuint load_shader(GLenum type, char const* path);
GLuint load_program(char const* vert_path, char const* frag_path);
GLuint load_texture(char const* path, Vec2U& size_out);
void generate_mipmaps(GLuint texture_id, U32 max_lvl);

template<typename F, typename... Args>
void set_uniform(char const* name, GLuint program_id,
                 F const& gl_fun, Args const &...args) {
    gl_fun(glGetUniformLocation(program_id, name), args...);
}

void rendering_init();
void rendering_deinit();
