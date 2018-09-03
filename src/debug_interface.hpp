#pragma once

#include <lux/alias/scalar.hpp>
#include <lux/alias/vec_2.hpp>
//
#include <render/program.hpp>
#include <render/texture.hpp>
#include <io_node.hpp>

namespace data { struct Config; }

class Renderer;

class DebugInterface : public IoNode
{
public:
    DebugInterface(GLFWwindow *win, Renderer &renderer, data::Config const &conf);

    void set_tick_time(F64 time);
protected:
    virtual void take_key(I32 key, I32 code, I32 action, I32 mods) override;
    virtual void take_st(net::server::Tick const &) override;
    virtual bool give_cs(net::client::Packet &) override;
    virtual void take_resize(Vec2UI const &size) override;
private:
    void render_text(String const &str, Vec2I const &pos);
    Vec2UI get_char_pos(char character);

    render::Program program;
    render::Texture font;
    Vec2UI screen_size;
    Vec2UI char_size;
    Vec2UI font_char_size;
    U32       char_scale;
    F64       tick_time;
    bool      conf_signal_queued;
    Renderer &renderer;

    GLuint vbo_id;
    GLuint ebo_id;
};
