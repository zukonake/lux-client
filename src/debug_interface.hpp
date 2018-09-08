#pragma once

#include <render/gl.hpp>
//
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
    typedef U32 Index;
    static constexpr GLenum INDEX_GL_TYPE = GL_UNSIGNED_INT;
    #pragma pack(push, 1)
    struct Vert
    {
        Vec2F pos;
        Vec2F tex_pos;
        Vert(Vec2F pos, Vec2F tex_pos) :
            pos(pos), tex_pos(tex_pos) { }
    };
    #pragma pack(pop)
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
    U32    char_scale;
    F64    tick_time;
    bool   conf_signal_queued;
    Renderer &renderer;

    GLuint vbo;
    GLuint ebo;
};
