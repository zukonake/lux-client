#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/vector.hpp>
//
#include <data/config.hpp>
#include <render/vertex.hpp>
#include <render/program.hpp>
#include <map/map.hpp>

namespace serial
{
    struct ServerData;
    struct ClientData;
}

class IoClient
{
    public:
    IoClient(data::Config const &config, F64 fps);
    ~IoClient();

    void set_server_data(serial::ServerData const &sd);
    void get_client_data(serial::ClientData       &cd);

    bool should_close();
    private:
    static const SizeT OPENGL_LOG_SIZE = 512;

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void error_callback(int err, const char* desc);
    static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);

    void init_glfw_core();
    void init_glfw_window();
    void init_glad();
    void init_vbo();
    void init_ebo();
    void init_vert_attribs();
    void init_tileset();

    GLFWwindow *glfw_window;
    data::Config const &conf;
    map::Map map;

    render::Program program;
    Vector<render::Vertex> vertices;
    Vector<GLuint>         indices;
    linear::Vec2<U16>      view_size;
    glm::vec2              tileset_size;
    GLuint vbo_id;
    GLuint ebo_id;
    GLuint tileset_id;
};
