#pragma once

#include <glm/glm.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/vector.hpp>
#include <lux/alias/vec_2.hpp>
#include <lux/world/map.hpp>
#include <lux/world/entity.hpp>
//
#include <render/program.hpp>
#include <render/texture.hpp>
#include <render/camera.hpp>
#include <map/map.hpp>
#include <io_node.hpp>

namespace data { struct Config; }

class Renderer : public IoNode
{
public:
    Renderer(GLFWwindow *win, data::Config const &conf);
    ~Renderer();

    void toggle_wireframe();
    void toggle_face_culling();
    void toggle_frustrum_culling();
    void toggle_distance_sorting();
    void increase_view_range();
    void decrease_view_range();
    F32  get_view_range();
protected:
    virtual void take_st(net::server::Tick const &) override;
    virtual void take_ss(net::server::Packet const &) override;
    virtual void take_resize(Vec2UI const &size) override;
    virtual void give_ct(net::client::Tick &) override;
private:
    static constexpr F32 Z_NEAR = 0.1f;

    void render_world(EntityPos const &player_pos);
    void render_chunk(ChkPos const &chunk_pos);
    void render_mesh(Chunk::Mesh const &mesh);

    bool is_chunk_visible(ChkPos const &pos);
    void get_render_queue(Vector<ChkPos> &render_queue, ChkPos const &center);
    void sort_render_queue(Vector<ChkPos> &render_queue, ChkPos const &center);

    void update_view(EntityPos const &player_pos);
    void update_projection(F32 width_to_height);
    void update_wvp();
    void update_view_range();

    Map map;
    F32 view_range;
    F32 z_far;
    F32 fov;
    bool wireframe;
    bool face_culling;
    bool frustrum_culling;
    bool distance_sorting;
    Vector<ChkPos> render_queue;

    GLuint fb_id;
    GLuint color_rb_id;
    GLuint depth_rb_id;
    Vec4F sky_color;
    Vec2I  last_mouse_pos;
    Vec2UI screen_size;
    Vec2UI screen_scale;
    glm::mat4 world_mat;
    glm::mat4 view_mat;
    glm::mat4 projection_mat;
    glm::mat4 wvp_mat;
    render::Camera camera;
    render::Program program;
    render::Texture tileset;
};
