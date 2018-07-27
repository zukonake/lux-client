#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/set.hpp>
#include <lux/common/chunk.hpp>
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

    void render();
    void render_chunk(map::Chunk const &chunk);

    void init_glfw_core();
    void init_glfw_window();
    void init_glad();
   // void init_tileset(); 
    GLFWwindow *glfw_window;
    data::Config const &conf;
    map::Map map;
    glm::vec2 mouse_pos;
    glm::mat4 world_mat;

    render::Program program;
    render::Camera  camera;
    Set<chunk::Pos> chunk_requests;
};
