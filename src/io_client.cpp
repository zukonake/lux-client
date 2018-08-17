#include <stdexcept>
//
#include <render/gl.hpp>
//
#include <lux/util/log.hpp>
#include <lux/alias/string.hpp>
//
#include "io_client.hpp"

IoClient::IoClient(GLFWwindow *win, data::Config const &conf) :
    IoNode(win),
    renderer(win, conf),
    entity_controller(win)
{
    IoNode::add_node(renderer);
    IoNode::add_node(entity_controller);

    glfwSetErrorCallback(glfw_error_cb); //move to gl_initializer?
    glfwSetWindowUserPointer(IoNode::win, this);
    glfwSetFramebufferSizeCallback(IoNode::win, window_resize_cb);
    glfwSetKeyCallback(IoNode::win, key_cb);
    glfwSetMouseButtonCallback(IoNode::win, mouse_cb);

    window_resize_cb(IoNode::win, window_size.x, window_size.y);
}

void IoClient::give_ct(net::client::Tick &)
{
    glfwPollEvents();
}

bool IoClient::should_close()
{
    return glfwWindowShouldClose(IoNode::win);
}

void IoClient::window_resize_cb(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    IoClient *io_client = (IoClient *)glfwGetWindowUserPointer(window);
    io_client->dispatch_resize({width, height});
    util::log("IO_CLIENT", util::DEBUG, "screen size change to %ux%u", width, height);
}

void IoClient::key_cb(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    IoClient *io_client = (IoClient *)glfwGetWindowUserPointer(window);
    io_client->dispatch_key(key, scancode, action, mods);
}

void IoClient::mouse_cb(GLFWwindow* window, int button, int action, int mode)
{
    IoClient *io_client = (IoClient *)glfwGetWindowUserPointer(window);
    io_client->dispatch_mouse(button, action, mode);
}

void IoClient::glfw_error_cb(int err, const char* desc)
{
    throw std::runtime_error(String("GLFW error: ") +
                             std::to_string(err) +
                             String("; ") + desc);
}

void IoClient::take_st(net::server::Tick const &)
{
    check_gl_error();
    glfwSwapBuffers(IoNode::win);
}

void IoClient::check_gl_error()
{
    GLenum error = glGetError();
    while(error != GL_NO_ERROR)
    {
        String str;
        switch(error)
        {
            case GL_INVALID_ENUM: str = "invalid enum"; break;
            case GL_INVALID_VALUE: str = "invalid value"; break;
            case GL_INVALID_OPERATION: str = "invalid operation"; break;
            case GL_OUT_OF_MEMORY: str = "out of memory"; break;
            default: str = "unknown"; break;
        }
        util::log("OPEN_GL", util::ERROR, "#%d - %s", error, str);
        error = glGetError();
    }
}
