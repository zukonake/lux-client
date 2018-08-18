#include "io_node.hpp"

IoNode::IoNode(GLFWwindow *_win) :
    win(_win),
    active(true)
{

}

void IoNode::add_node(IoNode &node)
{
    nodes.emplace_back(&node);
}

void IoNode::dispatch_key(I32 key, I32 code, I32 action, I32 mods)
{
    if(active)
    {
        this->take_key(key, code, action, mods);
        for(auto *node : nodes)
        {
            node->dispatch_key(key, code, action, mods);
        }
    }
}

void IoNode::dispatch_mouse(I32 button, I32 action, I32 mods)
{
    if(active)
    {
        this->take_mouse(button, action, mods);
        for(auto *node : nodes)
        {
            node->dispatch_mouse(button, action, mods);
        }
    }
}

void IoNode::dispatch_resize(Vec2<U32> const &size)
{
    if(active)
    {
        this->take_resize(size);
        for(auto *node : nodes)
        {
            node->dispatch_resize(size);
        }
    }
}

void IoNode::dispatch_tick(net::server::Tick const &st)
{
    if(active)
    {
        this->take_st(st);
        for(auto *node : nodes)
        {
            node->dispatch_tick(st);
        }
    }
}

void IoNode::dispatch_signal(net::server::Packet const &ss)
{
    if(active)
    {
        this->take_ss(ss);
        for(auto *node : nodes)
        {
            node->dispatch_signal(ss);
        }
    }
}

void IoNode::gather_tick(net::client::Tick &ct)
{
    if(active)
    {
        this->give_ct(ct);
        for(auto *node : nodes)
        {
            node->gather_tick(ct);
        }
    }
}

bool IoNode::gather_signal(net::client::Packet &cs)
{
    if(active)
    {
        while(this->give_cs(cs)) return true;
        for(auto *node : nodes)
        {
            while(node->gather_signal(cs)) return true;
        }
    }
    return false;
}

void IoNode::activate()
{
    active = true;
    Vec2<I32> size;
    glfwGetWindowSize(win, &size.x, &size.y);
    dispatch_resize(size); /* need to update, since we missed the callbacks */
}

void IoNode::deactivate()
{
    active = false;
}

bool IoNode::is_active()
{
    return active;
}

void IoNode::take_key(I32, I32, I32, I32) { }
void IoNode::take_mouse(I32, I32, I32) { }
void IoNode::take_resize(Vec2<U32> const &) { }
void IoNode::take_st(net::server::Tick const &) { }
void IoNode::take_ss(net::server::Packet const &) { }
void IoNode::give_ct(net::client::Tick &) { }
bool IoNode::give_cs(net::client::Packet &) { return false; }

