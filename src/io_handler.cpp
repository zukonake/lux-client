#include <stdexcept>
//
#include <glad/glad.h>
#include <GLFW/glfw3.h>
//
#include <alias/string.hpp>
#include "io_handler.hpp"

IoHandler::IoHandler(double fps) :
    tick_clock(util::TickClock::Duration(fps))
{
    init_glfw();
    thread = std::thread(&IoHandler::run, this);
}

IoHandler::~IoHandler()
{
    glfwTerminate();
    thread.join();
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

void IoHandler::init_glfw()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing GLFW");
    glfwInit();
    glfwSetErrorCallback(error_callback);
    init_glfw_window();
}

void IoHandler::init_glfw_window()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing window");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfw_window = glfwCreateWindow(800, 600, "Lux", NULL, NULL);
    glfwMakeContextCurrent(glfw_window);
    init_glad();
    glViewport(0, 0, 800, 600);
    glfwSetFramebufferSizeCallback(glfw_window, framebuffer_size_callback);
    glfwSwapInterval(1);
}

void IoHandler::init_glad()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing GLAD");
    if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
    {
        throw std::runtime_error("couldn't initialize GLAD");
    }
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
    glfwSwapBuffers(glfw_window);
}

void IoHandler::handle_input()
{
    glfwPollEvents();
}
