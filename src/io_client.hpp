#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/set.hpp>
#include <lux/alias/vec_3.hpp>
#include <lux/common/chunk.hpp>
//
#include <data/config.hpp>
#include <io_node.hpp>
#include <renderer.hpp>
#include <entity_controller.hpp>
//#include <debug_interface.hpp>

namespace net::server { struct Packet; }
namespace net::client { struct Packet; }

class IoClient : public IoNode
{
public:
    IoClient(GLFWwindow *win, data::Config const &conf);

    bool should_close();
protected:
    virtual void take_st(net::server::Tick const &) override;
    virtual void give_ct(net::client::Tick &)       override;
private:
    static void window_resize_cb(GLFWwindow* window, int width, int height);
    static void key_cb(GLFWwindow *window, int key, int scancode, int action, int mode);
    static void mouse_cb(GLFWwindow* window, int button, int action, int mode);
    static void glfw_error_cb(int err, const char* desc);

    void check_gl_error();

    Renderer         renderer;
    EntityController entity_controller;
    //DebugInterface debug_interface;
};
