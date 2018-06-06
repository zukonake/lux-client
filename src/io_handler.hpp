#pragma once

#include <thread>
#include <mutex>
//
#include <GLFW/glfw3.h>
//
#include <net/server_data.hpp>
#include <net/client_data.hpp>
#include <util/tick_clock.hpp>

class IoHandler
{
    public:
    IoHandler(double fps);
    ~IoHandler();

    void receive(net::ServerData const &sd);
    void send(net::ClientData &cd);
    private:
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);  
    static void error_callback(int err, const char* desc);

    void init_glfw();
    void init_glfw_window();
    void init_glad();

    void run();
    void render();
    void handle_input();

    GLFWwindow *glfw_window;
    std::mutex  io_mutex;
    std::thread thread;
    net::ServerData sd_buffer;
    net::ClientData cd_buffer;
    util::TickClock tick_clock;
};
