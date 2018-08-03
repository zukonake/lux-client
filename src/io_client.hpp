#pragma once

#include <thread>
#include <mutex>
#include <atomic>
//
#include <glad/glad.h>
#include <GLFW/glfw3.h>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/set.hpp>
#include <lux/util/tick_clock.hpp>
#include <lux/common/chunk.hpp>
#include <lux/common/entity.hpp>
//
#include <data/config.hpp>
#include <render/vertex.hpp>
#include <render/program.hpp>
#include <render/camera.hpp>
#include <map/chunk.hpp>
#include <map/map.hpp>

namespace serial
{
    struct ServerData;
    struct ClientData;
}

class IoClient
{
    public:
    IoClient(data::Config const &config, F64 fps);
    ~IoClient();

    void set_server_data(serial::ServerData const &sd);
    void get_client_data(serial::ClientData       &cd);

    bool should_close();
    private:
    static const SizeT OPENGL_LOG_SIZE = 512;

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void error_callback(int err, const char* desc);
    static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
    static void mouse_callback(GLFWwindow* window, double xpos, double ypos);

    void run();

    void handle_server_data();
    void handle_client_data();

    void render(entity::Pos pos, SizeT entities_num);
    void render_entities(SizeT num);
    void render_chunk(chunk::Pos const &pos);

    void check_gl_error();

    void build_entity_buffer(entity::Pos const &player_pos,
                             Vector<entity::Pos> const &entities);

    void init();
    void init_glfw_core();
    void init_glfw_window();
    void init_glad();
    void deinit();

    GLFWwindow *glfw_window;

    std::atomic<bool> has_initialized;
    std::thread thread;
    std::mutex  sd_mutex;
    std::mutex  cd_mutex;
    serial::ServerData sd;
    serial::ClientData cd;
    util::TickClock tick_clock;

    data::Config const &conf;
    map::Map map;
    render::Program program;
    render::Camera  camera;
    linear::Vec3<U8> view_range;
    Set<chunk::Pos> chunk_requests;

    glm::vec2 mouse_pos;
    glm::mat4 world_mat;
    GLuint    entity_vbo;
    GLuint    entity_ebo;
};
