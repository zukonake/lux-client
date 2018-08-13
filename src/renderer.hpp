#pragma once

#include <glm/glm.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/string.hpp>
#include <lux/alias/vec_2.hpp>
#include <lux/common/entity.hpp>
//
#include <render/program.hpp>
#include <render/texture.hpp>
#include <render/camera.hpp>
#include <map.hpp>
#include <io_node.hpp>

namespace data { struct Config; }

class Renderer : public IoNode
{
public:
    Renderer(GLFWwindow *win, data::Config const &conf);
protected:
    virtual void take_st(net::server::Tick const &) override;
    virtual void take_ss(net::server::Packet const &) override;
    virtual void take_resize(Vec2<U32> const &size) override;
    virtual void give_ct(net::client::Tick &) override;
private:
    static constexpr F32 FOV    = 120.f;
    static constexpr F32 Z_NEAR = 0.1f;
    static constexpr F32 Z_FAR  = 100.f;

    void render_world(entity::Pos const &player_pos);
    void render_chunk(ChkPos const &chunk_pos);

    void update_view(entity::Pos const &player_pos);
    void update_projection(F32 width_to_height);

    Map map;
    F32 view_range;
    Vec2<I32> last_mouse_pos;
    glm::mat4 world_mat;
    render::Camera camera;
    render::Program program;
    render::Texture tileset;
};
