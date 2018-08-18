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
protected:
    virtual void take_key(I32 key, I32 code, I32 action, I32 mods) override;
    virtual void take_st(net::server::Tick const &) override;
    virtual bool give_cs(net::client::Packet &) override;
    virtual void take_resize(Vec2<U32> const &size) override;
private:
    void render_text(String const &str, Vec2<I32> const &pos);
    Vec2<U32> get_char_pos(char character);

    render::Program program;
    render::Texture font;
    Vec2<U32> screen_size;
    Vec2<U32> char_size;
    Vec2<U32> font_char_size;
    U32       char_scale;
    bool      conf_signal_queued;
    Renderer &renderer;

    GLuint vbo_id;
    GLuint ebo_id;
};
