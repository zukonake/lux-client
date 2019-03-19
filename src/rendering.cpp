#include <config.hpp>
//
#include <cstring>
#include <vector>
#include <fstream>
//
#include <include_opengl.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <lodepng/lodepng.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>
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
    return Vec2D(-1.f, 1.f) * (((temp / (Vec2D)get_window_size()) * 2.0) - 1.0);
}

void rendering_init() {
    constexpr Vec2U WINDOW_SIZE = {800, 600};
    { ///GLFW
        glfwInit();
        glfwSetErrorCallback(glfw_error_cb);
        LUX_LOG("initializing GLFW window");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        glfw_window = glfwCreateWindow(WINDOW_SIZE.x, WINDOW_SIZE.y,
            "Lux", nullptr, nullptr);
        if(glfw_window == nullptr) {
            LUX_FATAL("couldn't create GLFW window");
        }
        glfwMakeContextCurrent(glfw_window);
        glfwSwapInterval(0);
    }

    { ///GLAD
        LUX_LOG("initializing GLAD");
        if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
        {
            LUX_FATAL("couldn't initialize GLAD");
        }
    }
    glViewport(0, 0, WINDOW_SIZE.x, WINDOW_SIZE.y);
    glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
    char constexpr VERSION_DIRECTIVE[] = "#version 330 core\n";
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
            LUX_FATAL("shader %s compilation error: \n%s", path, log);
        }
    }
    return id;
}

GLuint load_program(char const* vert_path, char const* frag_path) {
    //@TODO geometry shader support
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

GLuint load_program(char const* vert_path, char const* frag_path, char const* geom_path) {
    GLuint vert_id = load_shader(GL_VERTEX_SHADER  , vert_path);
    GLuint frag_id = load_shader(GL_FRAGMENT_SHADER, frag_path);
    GLuint geom_id = load_shader(GL_GEOMETRY_SHADER, geom_path);

    GLuint id = glCreateProgram();
    glAttachShader(id, vert_id);
    glAttachShader(id, frag_id);
    glAttachShader(id, geom_id);
    glLinkProgram(id);
    glDeleteShader(vert_id);
    glDeleteShader(frag_id);
    glDeleteShader(geom_id);

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

//@TODO more abstractions + above
namespace gl {

void Buff::init() {
    glGenBuffers(1, &id);
}

void Buff::deinit() {
    glDeleteBuffers(1, &id);
}

void Buff::bind(GLenum target) const {
    glBindBuffer(target, id);
}

void VertBuff::bind() const {
    Buff::bind(GL_ARRAY_BUFFER);
}

void IdxBuff::bind() const {
    Buff::bind(GL_ELEMENT_ARRAY_BUFFER);
}

void VertContext::deinit() {
    glDeleteVertexArrays(1, &vao_id);
}

void VertContext::bind() const {
    glBindVertexArray(vao_id);
}

void VertContext::bind_attribs() const {
    Uns vbo_it = 0;
    vert_buffs[vbo_it].bind();
    for(auto const& attrib : vert_fmt->attribs) {
        if(attrib.next_vbo) {
            vbo_it++;
            vert_buffs[vbo_it].bind();
        }
        glEnableVertexAttribArray(attrib.pos);
        glVertexAttribPointer(attrib.pos, attrib.num,
            attrib.type, attrib.normalize, attrib.stride, attrib.off);
    }
}

void VertContext::unbind_all() {
    glBindVertexArray(0);
}

}
