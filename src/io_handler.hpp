#pragma once

#include <thread>
#include <mutex>
#include <atomic>
//
#include <glad/glad.h>
#include <GLFW/glfw3.h>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/vector.hpp>
#include <lux/util/tick_clock.hpp>
#include <lux/net/server/server_data.hpp>
#include <lux/net/client/client_data.hpp>
//
#include <data/config.hpp>
#include <render/vertex.hpp>
#include <render/program.hpp>
#include <map/map.hpp>

class IoHandler
{
    public:
    IoHandler(data::Config const &config, double fps);
    ~IoHandler();

    void receive(net::ServerData const &sd);
    void send(net::ClientData &cd);
    bool should_close();
    private:
    static const SizeT OPENGL_LOG_SIZE = 512;

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void error_callback(int err, const char* desc);
    static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);

    void start();
    void init_glfw_core();
    void init_glfw_window();
    void init_glad();
    void init_vbo();
    void init_ebo();
    void init_vert_attribs();
    void init_tileset();

    void run();
    void render();
    void handle_input();
    void set_view_size(linear::Vec2<U16> const &val);
    void resize_indices();

    GLFWwindow *glfw_window;
    std::mutex  io_mutex;
    std::thread thread;
    data::Config const &config;
    net::ServerData sd_buffer;
    net::ClientData cd_buffer;
    util::TickClock tick_clock;
    render::Program program;
    std::atomic<bool> initialized;
    map::Map map;

    Vector<render::Vertex> vertices;
    Vector<GLuint>         indices;
    linear::Vec2<U16>      view_size;
    glm::vec2              tileset_size;
    GLuint vbo_id;
    GLuint ebo_id;
    GLuint tileset_id;
};
