#pragma once

#include <glm/glm.hpp>
//
#include <lux/alias/scalar.hpp>
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

    void toggle_wireframe();
    void toggle_face_culling();
    void increase_view_range();
    void decrease_view_range();
    F32  get_view_range();
protected:
    virtual void take_st(net::server::Tick const &) override;
    virtual void take_ss(net::server::Packet const &) override;
    virtual void take_resize(Vec2<U32> const &size) override;
    virtual void give_ct(net::client::Tick &) override;
private:
    static constexpr F32 Z_NEAR = 0.1f;

    void render_world(entity::Pos const &player_pos);
    void render_chunk(ChkPos const &chunk_pos);
    void render_mesh(render::Mesh const &mesh);

    void update_view(entity::Pos const &player_pos);
    void update_projection(F32 width_to_height);
    void update_wvp();
    void update_view_range();

    Map map;
    F32 view_range;
    F32 z_far;
    F32 fov;
    bool wireframe;
    bool face_culling;

    glm::vec4 sky_color;
    Vec2<I32> last_mouse_pos;
    glm::mat4 world_mat;
    glm::mat4 view_mat;
    glm::mat4 projection_mat;
    glm::mat4 wvp_mat;
    render::Camera camera;
    render::Program program;
    render::Texture tileset;
};
