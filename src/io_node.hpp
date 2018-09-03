#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/vector.hpp>
#include <lux/alias/vec_2.hpp>

namespace net::server { struct Tick; struct Packet; }
namespace net::client { struct Tick; struct Packet; }

class IoNode
{
    public:
    IoNode(GLFWwindow *win);
    IoNode(IoNode const &) = delete;
    IoNode &operator=(IoNode const &) = delete;

    void add_node(IoNode &node);

    void dispatch_key(I32 key, I32 code, I32 action, I32 mods);
    void dispatch_mouse(I32 button, I32 action, I32 mods);
    void dispatch_resize(Vec2UI const &size);
    void dispatch_tick(net::server::Tick const &);
    void dispatch_signal(net::server::Packet const &);

    void gather_tick(net::client::Tick &);
    bool gather_signal(net::client::Packet &);

    void activate();
    void deactivate();
    bool is_active();

    protected:
    virtual void take_key(I32 key, I32 code, I32 action, I32 mods);
    virtual void take_mouse(I32 button, I32 action, I32 mods);
    virtual void take_resize(Vec2UI const &size);

    virtual void take_st(net::server::Tick const &);
    virtual void take_ss(net::server::Packet const &);
    virtual void give_ct(net::client::Tick &);
    virtual bool give_cs(net::client::Packet &);

    GLFWwindow *win;
    private:
    Vector<IoNode *> nodes;
    bool active;
};
