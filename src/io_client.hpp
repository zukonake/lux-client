#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/set.hpp>
#include <lux/alias/vec_3.hpp>
#include <lux/common/chunk.hpp>
//
#include <data/config.hpp>
#include <render/tileset.hpp>
#include <render/program.hpp>
#include <render/camera.hpp>
#include <map/map.hpp>

namespace net::server { struct Packet; }
namespace net::client { struct Packet; }

class IoClient
{
    public:
    IoClient(data::Config const &config);
    ~IoClient();

    void take_server_tick(net::server::Tick const &);
    void take_server_signal(net::server::Packet const &);
    void give_client_tick(net::client::Packet &);
    bool give_client_signal(net::client::Packet &);

    bool should_close();
    private:
    static constexpr F32 FOV    = 120.f;
    static constexpr F32 Z_NEAR = 0.1f;
    static constexpr F32 Z_FAR  = 40.1f;

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void error_callback(int err, const char* desc);
    static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
    static void mouse_callback(GLFWwindow* window, double xpos, double ypos);

    void render();
    void render_chunk(chunk::Pos const &pos);

    void check_gl_error();

    void init_glfw_core();
    void init_glfw_window();
    void init_glad();

    void set_projection(F32 width_to_height);

    GLFWwindow *glfw_window;

    data::Config const &conf;
    map::Map map;
    render::Program program;
    render::Camera  camera;
    render::Tileset tileset;
    Vec3<U8> view_range;

    entity::Pos player_pos;
    glm::vec2 mouse_pos;
    glm::mat4 world_mat;
};
