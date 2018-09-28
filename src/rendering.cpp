#include <config.hpp>
//
#include <cstring>
#include <vector>
#include <fstream>
//
#include <include_opengl.hpp>
#include <include_glfw.hpp>
#define GLM_FORCE_PURE
#include <glm/gtc/constants.hpp>
#include <lodepng.h>
//
#include <lux_shared/common.hpp>
//
#include <rendering.hpp>

GLFWwindow* glfw_window;

static void glfw_error_cb(int err, char const* desc) {
    LUX_FATAL("GLFW error: %d - %s", err, desc);
}

Vec2U get_window_size() {
    Vec2<I32> temp;
    glfwGetWindowSize(glfw_window, &temp.x, &temp.y);
    return temp;
}

Vec2D get_mouse_pos() {
    Vec2D temp;
    glfwGetCursorPos(glfw_window, &temp.x, &temp.y);
    return temp;
}

void rendering_init() {
    constexpr Vec2U WINDOW_SIZE = {800, 600};
    { ///GLFW
        glfwInit();
        glfwSetErrorCallback(glfw_error_cb);
        LUX_LOG("initializing GLFW window");
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#endif
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        //@TODO win size
        glfw_window = glfwCreateWindow(WINDOW_SIZE.x, WINDOW_SIZE.y,
            "Lux", nullptr, nullptr);
        if(glfw_window == nullptr) {
            //@TODO more info
            LUX_FATAL("couldn't create GLFW window");
        }
        glfwMakeContextCurrent(glfw_window);
        glfwSwapInterval(0);
    }

    { ///GLAD
        LUX_LOG("initializing GLAD");
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
        if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
        if(gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress) == 0)
#endif
        {
            LUX_FATAL("couldn't initialize GLAD");
        }
    }
    glViewport(0, 0, WINDOW_SIZE.x, WINDOW_SIZE.y);
}

void rendering_deinit() {
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

GLuint load_shader(GLenum type, char const* path) {
    GLuint id = glCreateShader(type);
    char constexpr VERSION_DIRECTIVE[] = "#version "
#if LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
        "330 core"
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
        "100"
#endif
        "\n";
    constexpr SizeT VERSION_LEN = sizeof(VERSION_DIRECTIVE) - 1;
    char* str;
    {   std::ifstream file(path);
        file.seekg(0, file.end);
        long len = file.tellg();
        if(len == -1) {
            LUX_FATAL("failed to load shader: %s", path);
        }
        file.seekg(0, file.beg);

        str = lux_alloc<char>((SizeT)len + VERSION_LEN + 1);
        LUX_DEFER { lux_free(str); };
        std::memcpy(str, VERSION_DIRECTIVE, VERSION_LEN);
        file.read(str + VERSION_LEN, len);
        file.close();
        str[(SizeT)len + VERSION_LEN] = '\0';
        glShaderSource(id, 1, &str, nullptr);
        glCompileShader(id);
    }

    {   int success;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if(!success) {
            static constexpr SizeT OPENGL_LOG_SIZE = 512;
            char log[OPENGL_LOG_SIZE];
            glGetShaderInfoLog(id, OPENGL_LOG_SIZE, nullptr, log);
            LUX_FATAL("shader compilation error: \n%s", log);
        }
    }
    return id;
}

GLuint load_program(char const* vert_path, char const* frag_path) {
    GLuint vert_id = load_shader(GL_VERTEX_SHADER  , vert_path);
    GLuint frag_id = load_shader(GL_FRAGMENT_SHADER, frag_path);

    GLuint id = glCreateProgram();
    glAttachShader(id, vert_id);
    glAttachShader(id, frag_id);
    glLinkProgram(id);
    glDeleteShader(vert_id);
    glDeleteShader(frag_id);

    {   int success;
        glGetProgramiv(id, GL_LINK_STATUS, &success);
        if(!success) {
            static constexpr SizeT OPENGL_LOG_SIZE = 512;
            char log[OPENGL_LOG_SIZE];
            glGetProgramInfoLog(id, OPENGL_LOG_SIZE, nullptr, log);
            LUX_FATAL("program linking error: \n%s", log);
        }
    }
    return id;
}

GLuint load_texture(char const* path, Vec2U& size_out) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    Vec2<unsigned> size;
    std::vector<U8> img;
    {   auto err = lodepng::decode(img, size.x, size.y, path);
        if(err) LUX_FATAL("couldn't load texture: %s", path);
    }
    size_out = size;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_out.x, size_out.y,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return id;
}

void generate_mipmaps(GLuint texture_id, U32 max_lvl) {
    LUX_LOG("unimplemented");
}

F32 get_aim_rotation() {
    Vec2U window_size = get_window_size();
    Vec2D mouse_pos = (get_mouse_pos() / (Vec2D)window_size) - 0.5;
    return -std::atan2(-mouse_pos.y, mouse_pos.x) + glm::half_pi<F32>();
}
