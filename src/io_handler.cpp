#include <cstring>
#include <stdexcept>
#include <sstream>
#include <fstream>
//
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
//
#include <util/log.hpp>
#include <alias/cstring.hpp>
#include <alias/string.hpp>
#include <linear/size_3d.hpp>
#include "io_handler.hpp"

IoHandler::IoHandler(data::Config const &config, double fps) :
    config(config),
    tick_clock(util::TickClock::Duration(1.0 / fps)),
    initialized(false)
{
    set_view_size({11, 11});
    thread = std::thread(&IoHandler::start, this);
    // ^ all the opengl related stuff must be initialized in the corresponding thread
}

IoHandler::~IoHandler()
{
    glfwSetWindowShouldClose(glfw_window, GLFW_TRUE);
    thread.join();
    glfwTerminate();
}

void IoHandler::receive(net::ServerData const &sd)
{
    std::lock_guard lock(io_mutex);
    sd_buffer = sd;
}

void IoHandler::send(net::ClientData &cd)
{
    std::lock_guard lock(io_mutex);
    cd = cd_buffer;
}

bool IoHandler::should_close()
{
    return initialized && glfwWindowShouldClose(glfw_window);
    // order is important, if initialized == false,
    // the second operand won't be evaluated
}

void IoHandler::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window;
    glViewport(0, 0, width, height);
}

void IoHandler::error_callback(int err, const char* desc)
{
    throw std::runtime_error(String("GLFW error: ") +
                             std::to_string(err) +
                             String("; ") + desc);
}

void IoHandler::key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    (void)window;
    (void)key;
    (void)scancode;
    (void)action;
    (void)mode;
    IoHandler *io_handler = (IoHandler *)glfwGetWindowUserPointer(window);
    io_handler->set_view_size({io_handler->view_size.x + 1, io_handler->view_size.y});
}

void IoHandler::start()
{
    init_glfw_core();
    init_glfw_window();
    init_glad();

    glViewport(0, 0, 800, 600);
    glfwSwapInterval(1);
    //^ TODO not sure if needed since io loop is regulated by tick_clock anyway

    init_shader_program();
    init_vbo();
    init_ebo();
    init_vert_attribs();

    initialized = true;

    run();
}

void IoHandler::init_glfw_core()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing GLFW core");
    glfwInit();
    glfwSetErrorCallback(error_callback);
}

void IoHandler::init_glfw_window()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing GLFW window");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfw_window = glfwCreateWindow(800, 600, "Lux", NULL, NULL);
    glfwMakeContextCurrent(glfw_window);
    glfwSetWindowUserPointer(glfw_window, this);
    glfwSetFramebufferSizeCallback(glfw_window, framebuffer_size_callback);
    glfwSetKeyCallback(glfw_window, key_callback);
}

void IoHandler::init_glad()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing GLAD");
    if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
    {
        throw std::runtime_error("couldn't initialize GLAD");
    }
}

unsigned IoHandler::init_shader(GLenum type, CString path)
{
    util::log("IO_HANDLER", util::DEBUG, "initializing shader type %u from %s", type, path);
    unsigned id = glCreateShader(type);

    std::ifstream file(path);
    file.seekg (0, file.end);
    long len = file.tellg();
    file.seekg (0, file.beg);

    char *str = new char[(SizeT)len + 1];
    file.read(str, len);
    str[(SizeT)len] = '\0';
    glShaderSource(id, 1, &str, NULL);
    glCompileShader(id);
    file.close();
    delete[] str;

    int success;
    char log[512];
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(id, 512, NULL, log);
        throw std::runtime_error("shader compile error: \n" + std::string(log));
    }
    return id;
}

void IoHandler::init_shader_program()
{
    unsigned vert_shader = init_shader(GL_VERTEX_SHADER, config.vertex_shader_path);
    unsigned frag_shader = init_shader(GL_FRAGMENT_SHADER, config.fragment_shader_path);
    util::log("IO_HANDLER", util::DEBUG, "initializing shader program");
    programId = glCreateProgram();
    glAttachShader(programId, vert_shader);
    glAttachShader(programId, frag_shader);
    glLinkProgram(programId);
    {
        int success;
        char log[512];
        glGetProgramiv(programId, GL_LINK_STATUS, &success);
        if(!success)
        {
            glGetProgramInfoLog(programId, 512, NULL, log);
            throw std::runtime_error("program linking error: \n" + std::string(log));
        }
    }
    glUseProgram(programId);
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);  
}

void IoHandler::init_vbo()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing VBO");
    glGenBuffers(1, &vboId);
    glBindBuffer(GL_ARRAY_BUFFER, vboId);
}

void IoHandler::init_ebo()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing EBO");
    glGenBuffers(1, &eboId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboId);
}

void IoHandler::init_vert_attribs()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing vertex attributes");
    static_assert(sizeof(render::Vertex) == 3 * sizeof(float) + 
                                            2 * sizeof(unsigned) +
                                            4 * sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT       , GL_FALSE, sizeof(render::Vertex),
                          (void*)offsetof(render::Vertex, pos));
    glVertexAttribPointer(1, 2, GL_UNSIGNED_INT, GL_FALSE, sizeof(render::Vertex),
                          (void*)offsetof(render::Vertex, tex_pos));
    glVertexAttribPointer(2, 4, GL_FLOAT       , GL_FALSE, sizeof(render::Vertex),
                          (void*)offsetof(render::Vertex, color));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
}

void IoHandler::run()
{
    util::log("IO_HANDLER", util::DEBUG, "IO loop started");
    while(!glfwWindowShouldClose(glfw_window))
    {
        tick_clock.start();
        render();
        handle_input();
        tick_clock.stop();
        tick_clock.synchronize();
    }
    util::log("IO_HANDLER", util::DEBUG, "IO loop stopped");
}

void IoHandler::render()
{
    {
        std::lock_guard lock(io_mutex);
        auto tiles_size = sd_buffer.tiles.size();
        for(SizeT i = 0; i < tiles_size; ++i)
        {
            auto const &tile = sd_buffer.tiles[i];
            vertices[(i * 4) + 0].tex_pos = tile.tex_pos;
            vertices[(i * 4) + 1].tex_pos = tile.tex_pos;
            vertices[(i * 4) + 2].tex_pos = tile.tex_pos;
            vertices[(i * 4) + 3].tex_pos = tile.tex_pos;
            vertices[(i * 4) + 0].color = {(float)tile.shape, 0.0, 0.0, 1.0};
            vertices[(i * 4) + 1].color = {(float)tile.shape, 0.0, 0.0, 1.0};
            vertices[(i * 4) + 2].color = {(float)tile.shape, 0.0, 0.0, 1.0};
            vertices[(i * 4) + 3].color = {(float)tile.shape, 0.0, 0.0, 1.0};
            // ^ TODO placeholder
        }
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(render::Vertex) * vertices.size(),
                 vertices.data(),
                 GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(unsigned) * indices.size(),
                 indices.data(),
                 GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glfwSwapBuffers(glfw_window);
}

void IoHandler::handle_input()
{
    {
        std::lock_guard lock(io_mutex);
        cd_buffer.view_size = view_size;
    }

    glfwPollEvents();
}

void IoHandler::set_view_size(linear::Point2d<U16> const &val)
{
    view_size = val;
    resize_indices();
    resize_vertices();
}

void IoHandler::resize_indices()
{
    SizeT view_tiles = view_size.x * view_size.y;
    indices.resize(view_tiles * 6);
    for(SizeT i = 0; i < view_tiles; ++i)
    {
        indices[(i * 6) + 0] = (unsigned)((i * 4) + 0);
        indices[(i * 6) + 1] = (unsigned)((i * 4) + 1);
        indices[(i * 6) + 2] = (unsigned)((i * 4) + 3);
        indices[(i * 6) + 3] = (unsigned)((i * 4) + 1);
        indices[(i * 6) + 4] = (unsigned)((i * 4) + 2);
        indices[(i * 6) + 5] = (unsigned)((i * 4) + 3);
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(unsigned) * indices.size(),
                 indices.data(),
                 GL_DYNAMIC_DRAW);
}

void IoHandler::resize_vertices()
{
    SizeT view_tiles = view_size.x * view_size.y;
    vertices.resize(view_tiles * 4);
    glm::mat3x2 size(2.0 / view_size.x, 0                ,
                     0                , 2.0 / view_size.y,
                     0                , 0                );
    for(SizeT i = 0; i < view_tiles; ++i)
    {
        glm::vec3 base((i % view_size.x) - (float)(view_size.x / 2),
                       (i / view_size.x) - (float)(view_size.y / 2), 0);
        vertices[(i * 4) + 0].pos = (base + glm::vec3(0, 0, 0)) * size;
        vertices[(i * 4) + 1].pos = (base + glm::vec3(1, 0, 0)) * size;
        vertices[(i * 4) + 2].pos = (base + glm::vec3(1, 1, 0)) * size;
        vertices[(i * 4) + 3].pos = (base + glm::vec3(0, 1, 0)) * size;
    }
}
