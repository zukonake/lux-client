#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/vec_2.hpp>

class GlInitializer
{
    public:
    GlInitializer(Vec2UI const &window_size);
    ~GlInitializer();

    GLFWwindow *get_window();
    private:
    void init_window(Vec2UI const &size);
    void init_glad();
    
    void check_gl_error();

    GLFWwindow *win;
};
