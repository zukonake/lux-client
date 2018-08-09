#include <stdexcept>
#include <fstream>
//
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <render/gl.hpp>
#include <lodepng.h>
//
#include <lux/util/log.hpp>
#include <lux/alias/string.hpp>
#include <lux/net/server/packet.hpp>
#include <lux/net/client/packet.hpp>
//
#include "io_client.hpp"

IoClient::IoClient(data::Config const &config) :
    conf(config),
    map(*conf.db),
    view_range(config.view_range),
    world_mat({1.0, 0.0, 0.0, 0.0},
              {0.0, 0.0, 1.0, 0.0},
              {0.0, 1.0, 0.0, 0.0},
              {0.0, 0.0, 0.0, 1.0})
{
    init_glfw_core();
    init_glfw_window();
    init_glad();

    glfwSwapInterval(0);

    program.init(conf.vert_shader_path, conf.frag_shader_path);
    glm::vec2 tileset_size = tileset.load(conf.tileset_path);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    framebuffer_size_callback(glfw_window, 800, 600);

    program.set_uniform("world", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(world_mat));
    glm::vec2 tile_scale = {(F32)conf.tile_size.x / (F32)tileset_size.x,
                            (F32)conf.tile_size.y / (F32)tileset_size.y};
    program.set_uniform("tex_size", glUniform2f, tile_scale.x, tile_scale.y);
}

IoClient::~IoClient()
{
    glfwTerminate();
}

void IoClient::take_server_tick(net::server::Tick const &st)
{
    player_pos = st.player_pos;
    check_gl_error();
    glfwPollEvents();
    glFlush();
    render();
}

void IoClient::take_server_signal(net::server::Packet const &sp)
{
    if(sp.type == net::server::Packet::MAP)
    {
        for(auto const &chunk : sp.map.chunks)
        {
            map.add_chunk(chunk);
        }
    }
}

void IoClient::give_client_tick(net::client::Packet &cp)
{
    cp.type = net::client::Packet::TICK;
    cp.tick.is_moving = false;
    if(glfwGetKey(glfw_window, GLFW_KEY_A))
    {
        cp.tick.character_dir.x = -1.0;
        cp.tick.is_moving = true;
    }
    else if(glfwGetKey(glfw_window, GLFW_KEY_D))
    {
        cp.tick.character_dir.x = 1.0;
        cp.tick.is_moving = true;
    }
    else cp.tick.character_dir.x = 0.0;
    if(glfwGetKey(glfw_window, GLFW_KEY_W))
    {
        cp.tick.character_dir.y = -1.0;
        cp.tick.is_moving = true;
    }
    else if(glfwGetKey(glfw_window, GLFW_KEY_S))
    {
        cp.tick.character_dir.y = 1.0;
        cp.tick.is_moving = true;
    }
    else cp.tick.character_dir.y = 0.0;
    if(glfwGetKey(glfw_window, GLFW_KEY_SPACE))
    {
        cp.tick.is_jumping = true;
    }
    else cp.tick.is_jumping = false;
    cp.tick.character_dir =
        glm::vec3(camera.get_rotation() * glm::vec4(cp.tick.character_dir, 0.0, 1.0));
}

bool IoClient::give_client_signal(net::client::Packet &cp)
{
    return false;
}

bool IoClient::should_close()
{
    return glfwWindowShouldClose(glfw_window);
}

void IoClient::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    IoClient *io_client = (IoClient *)glfwGetWindowUserPointer(window);
    io_client->set_projection((F32)width/(F32)height);
    util::log("IO_CLIENT", util::DEBUG, "screen size change to %ux%u", width, height);
}

void IoClient::error_callback(int err, const char* desc)
{
    throw std::runtime_error(String("GLFW error: ") +
                             std::to_string(err) +
                             String("; ") + desc);
}

void IoClient::key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    (void)key;
    (void)window;
    (void)scancode;
    (void)action;
    (void)mode;
}

void IoClient::mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    IoClient *io_client = (IoClient *)glfwGetWindowUserPointer(window);
    io_client->camera.rotate({xpos - io_client->mouse_pos.x,
                              io_client->mouse_pos.y - ypos});
    io_client->mouse_pos = glm::vec2(xpos, ypos);
}

void IoClient::render()
{
    camera.teleport(glm::vec3(world_mat *
        glm::vec4(player_pos + glm::vec3(0.0, 0.0, 0.8), 1.0)));
    program.set_uniform("view", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(camera.get_view()));
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    chunk::Pos iter;
    chunk::Pos center = chunk::to_pos(player_pos); //TODO entity::to_pos
    for(iter.z = center.z - view_range.z;  //TODO render all chunks instead
        iter.z <= center.z + view_range.z; // server will handle the loading
        ++iter.z)
    {
        for(iter.y = center.y - view_range.y;
            iter.y <= center.y + view_range.y;
            ++iter.y)
        {
            for(iter.x = center.x - view_range.x;
                iter.x <= center.x + view_range.x;
                ++iter.x)
            {
                render_chunk(iter);
            }
        }
    }
    glfwSwapBuffers(glfw_window);
}

void IoClient::render_chunk(chunk::Pos const &pos)
{
    map::Chunk const *chunk = map[pos];
    if(chunk != nullptr)
    {
        glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk->ebo_id);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(render::Vertex), (void*)offsetof(render::Vertex, pos));
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
            sizeof(render::Vertex), (void*)offsetof(render::Vertex, col));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
            sizeof(render::Vertex), (void*)offsetof(render::Vertex, tex_pos));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glDrawElements(GL_TRIANGLES, chunk->indices.size(), render::INDEX_TYPE, 0);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
}

void IoClient::check_gl_error()
{
    GLenum error = glGetError();
    while(error != GL_NO_ERROR)
    {
        String str;
        switch(error)
        {
            case GL_INVALID_ENUM: str = "invalid enum"; break;
            case GL_INVALID_VALUE: str = "invalid value"; break;
            case GL_INVALID_OPERATION: str = "invalid operation"; break;
            case GL_OUT_OF_MEMORY: str = "out of memory"; break;
            default: str = "unknown"; break;
        }
        util::log("OPEN_GL", util::ERROR, "#%d - %s", error, str);
        error = glGetError();
    }
}

void IoClient::init_glfw_core()
{
    util::log("IO_CLIENT", util::DEBUG, "initializing GLFW core");
    glfwInit();
    glfwSetErrorCallback(error_callback);
}

void IoClient::init_glfw_window()
{
    util::log("IO_CLIENT", util::DEBUG, "initializing GLFW window");
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#else
#   error "Unsupported GL variant selected"
#endif
    glfw_window = glfwCreateWindow(800, 600, "Lux", NULL, NULL);
    glfwMakeContextCurrent(glfw_window);
    glfwSetWindowUserPointer(glfw_window, this);
    glfwSetFramebufferSizeCallback(glfw_window, framebuffer_size_callback);
    glfwSetKeyCallback(glfw_window, key_callback);
    glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(glfw_window, mouse_callback);
}

void IoClient::init_glad()
{
    util::log("IO_CLIENT", util::DEBUG, "initializing GLAD");
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
    if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
    if(gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress) == 0)
#else
#   error "Unsupported GL variant selected"
#endif
    {
        throw std::runtime_error("couldn't initialize GLAD");
    }
}

void IoClient::set_projection(F32 width_to_height)
{
    glm::mat4 projection =
        glm::perspective(glm::radians(FOV), width_to_height, Z_NEAR, Z_FAR);
    program.set_uniform("projection", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(projection));
}
