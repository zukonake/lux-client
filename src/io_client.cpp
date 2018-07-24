#include <cstring>
#include <stdexcept>
#include <sstream>
#include <fstream>
//
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <lodepng.h>
//
#include <lux/util/log.hpp>
#include <lux/alias/c_string.hpp>
#include <lux/alias/string.hpp>
#include <lux/linear/vec_2.hpp>
#include <lux/serial/server_data.hpp>
#include <lux/serial/client_data.hpp>
//
#include "io_client.hpp"

IoClient::IoClient(data::Config const &config, F64 fps) :
    conf(config),
    map(*conf.db)
{
    //TODO gl initializer class?
    init_glfw_core();
    init_glfw_window();
    init_glad();

    glfwSwapInterval(1);
    //^ TODO not sure if needed

    program.init(conf.vert_shader_path, conf.frag_shader_path);
    init_vbo();
    init_ebo();
    init_vert_attribs();
    init_tileset();

    framebuffer_size_callback(glfw_window, 800, 600);
}

IoClient::~IoClient()
{
    glfwSetWindowShouldClose(glfw_window, GLFW_TRUE);
    glfwTerminate();
}

void IoClient::set_server_data(serial::ServerData const &sd)
{

}

void IoClient::get_client_data(serial::ClientData &cd)
{
    cd.is_moving = false;
    if(glfwGetKey(glfw_window, GLFW_KEY_A))
    {
        cd.character_dir.x = -1.0;
        cd.is_moving = true;
    }
    else if(glfwGetKey(glfw_window, GLFW_KEY_D))
    {
        cd.character_dir.x = 1.0;
        cd.is_moving = true;
    }
    else cd.character_dir.x = 0.0;
    if(glfwGetKey(glfw_window, GLFW_KEY_W))
    {
        cd.character_dir.y = 1.0;
        cd.is_moving = true;
    }
    else if(glfwGetKey(glfw_window, GLFW_KEY_S))
    {
        cd.character_dir.y = -1.0;
        cd.is_moving = true;
    }
    else cd.character_dir.y = 0.0;
    glfwPollEvents();
}

bool IoClient::should_close()
{
    return glfwWindowShouldClose(glfw_window);
}

void IoClient::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    IoClient *io_handler = (IoClient *)glfwGetWindowUserPointer(window);
    /*io_handler->set_view_size({width  / io_handler->conf.tile_quad_size.x + 2,
                               height / io_handler->conf.tile_quad_size.y + 2}); //TODO*/
    util::log("IO_CLIENT", util::DEBUG, "screen size change to %ux%u", width, height);
}

void IoClient::error_callback(int err, const char* desc)
{
    throw std::runtime_error(String("GLFW error: ") +
                             std::to_string(err) +
                             String("; ") + desc);
}

void IoClient::key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    (void)key;
    (void)window;
    (void)scancode;
    (void)action;
    (void)mode;
}


void IoClient::init_glfw_core()
{
    util::log("IO_CLIENT", util::DEBUG, "initializing GLFW core");
    glfwInit();
    glfwSetErrorCallback(error_callback);
}

void IoClient::init_glfw_window()
{
    util::log("IO_CLIENT", util::DEBUG, "initializing GLFW window");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfw_window = glfwCreateWindow(800, 600, "Lux", NULL, NULL);
    glfwMakeContextCurrent(glfw_window);
    glfwSetWindowUserPointer(glfw_window, this);
    glfwSetFramebufferSizeCallback(glfw_window, framebuffer_size_callback);
    glfwSetKeyCallback(glfw_window, key_callback);
}

void IoClient::init_glad()
{
    util::log("IO_CLIENT", util::DEBUG, "initializing GLAD");
    if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
    {
        throw std::runtime_error("couldn't initialize GLAD");
    }
}

void IoClient::init_vbo()
{
    util::log("IO_CLIENT", util::DEBUG, "initializing VBO");
    glGenBuffers(1, &vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
} 
void IoClient::init_ebo()
{
    util::log("IO_CLIENT", util::DEBUG, "initializing EBO");
    glGenBuffers(1, &ebo_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_id);
}

void IoClient::init_vert_attribs()
{
    util::log("IO_CLIENT", util::DEBUG, "initializing vertex attributes");
    static_assert(sizeof(render::Vertex) == 3 * sizeof(GLfloat) + 
                                            2 * sizeof(GLfloat) +
                                            4 * sizeof(GLfloat));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(render::Vertex),
                          (void*)offsetof(render::Vertex, pos));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(render::Vertex),
                          (void*)offsetof(render::Vertex, tex_pos));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(render::Vertex),
                          (void*)offsetof(render::Vertex, color));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
}

void IoClient::init_tileset()
{
    util::log("IO_CLIENT", util::DEBUG, "loading tileset texture %s", conf.tileset_path);

    glGenTextures(1, &tileset_id);
    glBindTexture(GL_TEXTURE_2D, tileset_id);
    Vector<U8> image;
    linear::Vec2<unsigned> image_size;
    unsigned error = lodepng::decode(image,
                                     image_size.x,
                                     image_size.y,
                                     String(conf.tileset_path));
    tileset_size = (glm::vec2)image_size;

    auto const &tile_size = conf.tile_tex_size;
    glm::vec2 tile_scale = {tile_size.x / tileset_size.x, tile_size.y / tileset_size.y};
    program.set_uniform("tile_scale", glUniform2f, tile_scale.x, tile_scale.y);

    if(error) throw std::runtime_error("couldn't load tileset texture");

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 tileset_size.x,
                 tileset_size.y,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 image.data());

    //glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    // TODO ^ no mipmaps unless I find a way to generate them reliably
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    util::log("IO_CLIENT",
              util::DEBUG,
              "loaded tileset texture of size %ux%u",
              tileset_size.x,
              tileset_size.y);
}
