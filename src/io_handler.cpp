#include <stdexcept>
//
#include <glad/glad.h>
#include <GLFW/glfw3.h>
//
#include <util/log.hpp>
#include <alias/string.hpp>
#include "io_handler.hpp"

IoHandler::IoHandler(double fps) :
    tick_clock(util::TickClock::Duration(1.0 / fps)),
    initialized(false)
{
    thread = std::thread(&IoHandler::start, this);
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
}

void IoHandler::start()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing GLFW");
    glfwInit();
    glfwSetErrorCallback(error_callback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfw_window = glfwCreateWindow(800, 600, "Lux", NULL, NULL);
    glfwMakeContextCurrent(glfw_window);
    glfwSetFramebufferSizeCallback(glfw_window, framebuffer_size_callback);

    util::log("IO_HANDLER", util::DEBUG, "initializing GLAD");
    if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
    {
        throw std::runtime_error("couldn't initialize GLAD");
    }

    glViewport(0, 0, 800, 600);
    glfwSetKeyCallback(glfw_window, key_callback);
    glfwSwapInterval(1);

    run();
}

void IoHandler::run()
{
    initialized = true;
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
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(glfw_window);
}

void IoHandler::handle_input()
{
    glfwPollEvents();
}
