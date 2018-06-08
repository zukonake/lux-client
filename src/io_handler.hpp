#pragma once

#include <thread>
#include <mutex>
#include <atomic>
//
#include <GLFW/glfw3.h>
//
#include <alias/vector.hpp>
#include <util/tick_clock.hpp>
#include <net/server_data.hpp>
#include <net/client_data.hpp>
#include <data/config.hpp>
#include <render/vertex.hpp>

class IoHandler
{
    public:
    IoHandler(data::Config const &config, double fps);
    ~IoHandler();

    void receive(net::ServerData const &sd);
    void send(net::ClientData &cd);
    bool should_close();
    private:
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void error_callback(int err, const char* desc);
    static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);

    void start();
    void run();
    void render();
    void handle_input();

    GLFWwindow *glfw_window;
    std::mutex  io_mutex;
    std::thread thread;
    data::Config const &config;
    net::ServerData sd_buffer;
    net::ClientData cd_buffer;
    util::TickClock tick_clock;
    std::atomic<bool> initialized;

    Vector<render::Vertex> vertices;
    Vector<unsigned>       indices;
    linear::Size2d<U16>    view_size;
};
