#pragma once

#include <render/gl.hpp>
#include <io_node.hpp>

class EntityController : public IoNode
{
public:
    EntityController(GLFWwindow *win);
protected:
    virtual void give_ct(net::client::Tick &) override;
};
