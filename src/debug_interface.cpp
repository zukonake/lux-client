#include <lux/net/client/packet.hpp>
#include <lux/net/server/tick.hpp>
//
#include <data/config.hpp>
#include <renderer.hpp>
#include "debug_interface.hpp"

DebugInterface::DebugInterface(GLFWwindow *win, Renderer &renderer,
                               data::Config const &conf) :
    IoNode(win),
    char_size(conf.char_size),
    char_scale(conf.char_scale),
    tick_time(0.0),
    conf_signal_queued(false),
    renderer(renderer)
{
    program.init(conf.interface_shader_path + ".vert",
                 conf.interface_shader_path + ".frag");
    program.use();
    Vec2UI font_size = font.load(conf.font_path);
    font_char_size = font_size / char_size;
    Vec2F font_scale = Vec2F(1.f, 1.f) / (Vec2F)font_char_size;
    program.set_uniform("tex_scale", glUniform2f, font_scale.x, font_scale.y);

    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    Vec2I screen_size_temp;
    glfwGetWindowSize(win, &screen_size_temp.x, &screen_size_temp.y);
    screen_size = screen_size_temp;
}

void DebugInterface::set_tick_time(F64 time)
{
    tick_time = time;
}

void DebugInterface::take_key(I32 key, I32 code, I32 action, I32 mods)
{
    (void)code;
    (void)mods;
    if(key == GLFW_KEY_Y && action == GLFW_PRESS)
    {
        renderer.toggle_wireframe();
    }
    else if(key == GLFW_KEY_U && action == GLFW_PRESS)
    {
        renderer.toggle_face_culling();
    }
    else if(key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        renderer.toggle_frustrum_culling();
    }
    else if(key == GLFW_KEY_J && action == GLFW_PRESS)
    {
        renderer.toggle_distance_sorting();
    }
    else if(key == GLFW_KEY_I && action == GLFW_PRESS)
    {
        renderer.increase_view_range();
        conf_signal_queued = true;
    }
    else if(key == GLFW_KEY_O && action == GLFW_PRESS)
    {
        renderer.decrease_view_range();
        conf_signal_queued = true;
    }
}

void DebugInterface::take_st(net::server::Tick const &st)
{
    program.use();
    font.use();
    //TODO optimize this, use buffer instead of separate draw calls?
    render_text("x: " + std::to_string(st.player_pos.x), {1, 0});
    render_text("y: " + std::to_string(st.player_pos.y), {1, 1});
    render_text("z: " + std::to_string(st.player_pos.z), {1, 2});
    render_text("view: " + std::to_string(renderer.get_view_range()), {1, 3});
    render_text("tick: " + std::to_string(tick_time), {1, 4});
    render_text("y - toggle wireframe mode  ", {-1, 0});
    render_text("u - toggle face culling    ", {-1, 1});
    render_text("h - toggle frustrum culling", {-1, 2});
    render_text("j - toggle distance sorting", {-1, 3});
    render_text("i - increase view range    ", {-1, 4});
    render_text("o - decrease view range    ", {-1, 5});
}

bool DebugInterface::give_cs(net::client::Packet &cs)
{
    if(conf_signal_queued)
    {
        cs.type = net::client::Packet::CONF;
        cs.conf.load_range = renderer.get_view_range() + 1.f;
        conf_signal_queued = false;
        return true;
    }
    return false;
}

void DebugInterface::take_resize(Vec2UI const &size)
{
    screen_size = size;
}

void DebugInterface::render_text(String const &str, Vec2I const &base_pos)
{
    Vector<Vert>  vertices;
    Vector<Index> indices;

    vertices.reserve(str.size() * 4);
    indices.reserve(str.size() * 6);

    Vec2UI pos;
    Vec2UI screen_char_size = screen_size / char_size;
    if(base_pos.x < 0)
    {
        pos.x = screen_char_size.x -
                (std::abs(base_pos.x) + str.size() - 1) * char_scale;
    }
    else pos.x = base_pos.x * char_scale;
    if(base_pos.y < 0)
    {
        pos.y = screen_char_size.y - std::abs(base_pos.y) * char_scale;
    }
    else pos.y = base_pos.y * char_scale;

    Index index_offset = 0;
    constexpr Vec2UI verts[4] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};

    for(auto const &character : str)
    {
        Vec2UI char_pos = get_char_pos(character);
        for(auto const &vert : verts)
        {
            Vec2UI vert_pos = pos + (vert * char_scale);
            Vec2F ndc_pos = (((Vec2F)(vert_pos * char_size) /
                                (Vec2F)screen_size) * 2.f) - Vec2F(1, 1);
            //TODO this should be moved to vert shader
            ndc_pos.y = -ndc_pos.y;
            vertices.emplace_back(ndc_pos, (Vec2F)(char_pos + vert));
        }
        for(auto const &idx : {0, 1, 2, 2, 1, 3})
        {
            indices.emplace_back(index_offset + idx);
        }
        index_offset += 4;
        pos.x += char_scale;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            sizeof(Vert),
            (void*)offsetof(Vert, pos));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            sizeof(Vert),
            (void*)offsetof(Vert, tex_pos));

    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(Vert) * vertices.size(),
                 vertices.data(),
                 GL_STREAM_DRAW);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(Index) * indices.size(),
                 indices.data(),
                 GL_STREAM_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glDrawElements(GL_TRIANGLES, indices.size(), INDEX_GL_TYPE, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

Vec2UI DebugInterface::get_char_pos(char character)
{
    /* this works for the specific font that is currently used */
    U8 idx = character;
    return Vec2UI(idx % font_char_size.x, idx / font_char_size.x);
}

