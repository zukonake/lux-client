#include <lux/util/log.hpp>
//
#include "gl_initializer.hpp"

GlInitializer::GlInitializer(Vec2UI const &window_size)
{
    init_window(window_size);
    init_glad();
}

GlInitializer::~GlInitializer()
{
    glfwTerminate();
}

GLFWwindow *GlInitializer::get_window()
{
    return win;
}

void GlInitializer::init_window(Vec2UI const &size)
{
    glfwInit();
    util::log("GL_INITIALIZER", util::DEBUG, "initializing GLFW window");
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#else
#   error "Unsupported GL variant selected"
#endif
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    win = glfwCreateWindow(size.x, size.y, "Lux", NULL, NULL);
    glfwMakeContextCurrent(win);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSwapInterval(0);
}

void GlInitializer::init_glad()
{
    util::log("GL_INITIALIZER", util::DEBUG, "initializing GLAD");
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
    if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
    if(gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress) == 0)
#else
#   error "Unsupported GL variant selected"
#endif
    {
        throw std::runtime_error("couldn't initialize GLAD");
    }
}

