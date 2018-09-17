#include <config.hpp>
//
#include <lux_shared/common.hpp>
//
#include <rendering.hpp>

GLFWwindow* glfw_window;

static void glfw_error_cb(int err, char const* desc) {
    LUX_FATAL("GLFW error: %d - %s", err, desc);
}

void init_rendering(Vec2U const &window_size) {
    { ///init glfw
        glfwInit();
        glfwSetErrorCallback(glfw_error_cb);
        LUX_LOG("initializing GLFW window");
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 0);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#endif
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        glfw_window = glfwCreateWindow(window_size.x, window_size.y,
                                  "Lux", nullptr, nullptr);
        if(glfw_window == nullptr) {
            //@TODO more info
            LUX_FATAL("couldn't create GLFW window");
        }
        glfwMakeContextCurrent(glfw_window);
        glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSwapInterval(0);
    }

    {
        LUX_LOG("initializing GLAD");
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
        if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
        if(gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress) == 0)
#endif
        {
            LUX_FATAL("couldn't initialize GLAD");
        }
    }

    glViewport(0, 0, window_size.x, window_size.y);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void deinit_rendering() {
    glfwTerminate();
}

void check_opengl_error()
{
    GLenum error = glGetError();
    if(error != GL_NO_ERROR)
    {
        char const* str;
        switch(error)
        {
            case GL_INVALID_ENUM: str = "invalid enum"; break;
            case GL_INVALID_VALUE: str = "invalid value"; break;
            case GL_INVALID_OPERATION: str = "invalid operation"; break;
            case GL_OUT_OF_MEMORY: str = "out of memory"; break;
            default: str = "unknown"; break;
        }
        LUX_FATAL("OpenGL error: %d - %s", error, str);
    }
}

