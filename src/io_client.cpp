#include <cstring>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <algorithm>
//
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <lodepng.h>
//
#include <lux/util/log.hpp>
#include <lux/alias/c_string.hpp>
#include <lux/alias/string.hpp>
#include <lux/linear/vec_2.hpp>
#include <lux/serial/server_data.hpp>
#include <lux/serial/client_data.hpp>
//
#include "io_client.hpp"

IoClient::IoClient(data::Config const &config, F64 fps) :
    has_initialized(false),
    sd({{}, {}, {0, 0, 0}}),
    cd({{}, {0, 0}, false}),
    tick_clock(util::TickClock::Duration(1.0 / fps)),
    conf(config),
    map(*conf.db),
    view_range(config.view_range),
    world_mat({1.0, 0.0, 0.0, 0.0},
              {0.0, 0.0, 1.0, 0.0},
              {0.0, 1.0, 0.0, 0.0},
              {0.0, 0.0, 0.0, 1.0})
{
    thread = std::thread(&IoClient::init, this);
}

IoClient::~IoClient()
{
    glfwSetWindowShouldClose(glfw_window, GLFW_TRUE);
    thread.join();
}

void IoClient::set_server_data(serial::ServerData const &_sd)
{
    std::lock_guard<std::mutex> lock(sd_mutex);
    sd = _sd;
}

void IoClient::get_client_data(serial::ClientData &_cd)
{
    std::lock_guard<std::mutex> lock(cd_mutex);
    _cd = cd;
}

bool IoClient::should_close()
{
    return has_initialized && glfwWindowShouldClose(glfw_window);
    /* the sequence is important here, the glfw_window can be invalid when
     * has_initialized is not true
     */
}

void IoClient::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    IoClient *io_client = (IoClient *)glfwGetWindowUserPointer(window);
    glm::mat4 projection =
        glm::perspective(glm::radians(120.f), (float)width/(float)height, 0.1f, 40.0f);
    io_client->program.set_uniform("projection", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(projection));
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

void IoClient::run()
{
    while(!should_close())
    {
        tick_clock.start();
        check_gl_error();
        glfwPollEvents();
        handle_server_data();
        handle_client_data();
        tick_clock.stop();
        tick_clock.synchronize();
    }
    deinit();
}

void IoClient::handle_server_data()
{
    {
        std::lock_guard<std::mutex> lock(sd_mutex);
        for(auto const &chunk : sd.chunks)
        {
            util::log("IO_CLIENT", util::DEBUG, "receiving chunk %d, %d, %d",
                       chunk.pos.x, chunk.pos.y, chunk.pos.z);
            map.add_chunk(chunk);
        }
        build_entity_buffer(sd.player_pos, sd.entities);
    }
    render(sd.player_pos, sd.entities.size() == 0 ? 0 : sd.entities.size() - 1);
    /* TODO render should be moved to run(), it should not take any values from
     * sd, as sd must be locked before accessed
     */
}

void IoClient::handle_client_data()
{
    std::lock_guard<std::mutex> lock(cd_mutex);
    cd.chunk_requests.clear();
    std::copy(chunk_requests.begin(),
              chunk_requests.end(),
              std::back_inserter(cd.chunk_requests));
    chunk_requests.clear();
    cd.is_moving = false;
    if(glfwGetKey(glfw_window, GLFW_KEY_A))
    {
        cd.character_dir.x = -1.0;
        cd.is_moving = true;
    }
    else if(glfwGetKey(glfw_window, GLFW_KEY_D))
    {
        cd.character_dir.x = 1.0;
        cd.is_moving = true;
    }
    else cd.character_dir.x = 0.0;
    if(glfwGetKey(glfw_window, GLFW_KEY_W))
    {
        cd.character_dir.y = -1.0;
        cd.is_moving = true;
    }
    else if(glfwGetKey(glfw_window, GLFW_KEY_S))
    {
        cd.character_dir.y = 1.0;
        cd.is_moving = true;
    }
    else cd.character_dir.y = 0.0;
    cd.character_dir =
        glm::vec3(camera.get_rotation() * glm::vec4(cd.character_dir, 0.0, 1.0));
}

void IoClient::render(entity::Pos pos, SizeT entities_num)
{
    camera.teleport(glm::vec3(world_mat *
        glm::vec4(pos + glm::vec3(0.0, 0.0, 0.8), 1.0)));
    program.set_uniform("world", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(world_mat));
    program.set_uniform("view", glUniformMatrix4fv,
        1, GL_FALSE, glm::value_ptr(camera.get_view()));
    chunk_requests.clear();
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    chunk::Pos index;
    chunk::Pos center = chunk::to_pos(pos); //TODO map::to_pos from entity::Pos
    for(index.z = center.z - view_range.z;
        index.z <= center.z + view_range.z;
        ++index.z)
    {
        for(index.y = center.y - view_range.y;
            index.y <= center.y + view_range.y;
            ++index.y)
        {
            for(index.x = center.x - view_range.x;
                index.x <= center.x + view_range.x;
                ++index.x)
            {
                render_chunk(index);
            }
        }
    }
    render_entities(entities_num);
    glfwSwapBuffers(glfw_window);
}

void IoClient::render_chunk(chunk::Pos const &pos)
{
    map::Chunk const *chunk = map[pos];
    if(chunk == nullptr)
    {
        util::log("IO_CLIENT", util::DEBUG, "requesting chunk %d, %d, %d",
                   pos.x, pos.y, pos.z);
        chunk_requests.insert(pos);
    }
    else
    {
        glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk->ebo_id);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(render::Vertex), (void*)offsetof(render::Vertex, pos));
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
            sizeof(render::Vertex), (void*)offsetof(render::Vertex, col));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glDrawElements(GL_TRIANGLES, chunk->indices.size(), render::INDEX_TYPE, 0);
        glDisableVertexAttribArray(0); //TODO needed?
        glDisableVertexAttribArray(1); // also in render_entities
    }
}

void IoClient::render_entities(SizeT num)
{
    glBindBuffer(GL_ARRAY_BUFFER        , entity_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity_ebo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        sizeof(render::Vertex), (void*)offsetof(render::Vertex, pos));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
        sizeof(render::Vertex), (void*)offsetof(render::Vertex, col));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glDrawElements(GL_TRIANGLES, num * 36, render::INDEX_TYPE, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
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

void IoClient::build_entity_buffer(entity::Pos const &player_pos,
                                   Vector<entity::Pos> const &entities)
{
    glm::vec3 model[8] =
    {
        {0.0, 0.0, 0.0},
        {0.0, 0.8, 0.0},
        {0.0, 0.8, 1.7},
        {0.0, 0.0, 1.7},
        {0.8, 0.0, 0.0},
        {0.8, 0.8, 0.0},
        {0.8, 0.8, 1.7},
        {0.8, 0.0, 1.7},
    };
    Vector<render::Vertex> verts;
    Vector<render::Index>  indices;
    render::Index index_offset = 0;
    for(auto const &entity : entities)
    {
        if(entity != player_pos)
        {
            for(auto const &vert : model)
            {
                verts.push_back({entity + vert, {0.5, 0.0, 0.0, 1.0}});
            }
            for(auto const &idx : {0, 1, 2, 0, 3, 2,
                                   4, 5, 6, 4, 7, 6,
                                   0, 4, 7, 0, 3, 7,
                                   1, 5, 6, 1, 2, 6,
                                   0, 1, 5, 0, 4, 5,
                                   3, 2, 6, 3, 7, 6})
            {
                indices.emplace_back(idx + index_offset);
            }
            index_offset += 8;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, entity_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(render::Vertex) * verts.size(),
                 verts.data(),
                 GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(render::Index) * indices.size(),
                 indices.data(),
                 GL_STREAM_DRAW);
}

void IoClient::init()
{
    //TODO gl initializer class?
    init_glfw_core();
    init_glfw_window();
    init_glad();

    glfwSwapInterval(0);

    program.init(conf.vert_shader_path, conf.frag_shader_path);
    glEnable(GL_DEPTH_TEST);

    framebuffer_size_callback(glfw_window, 800, 600);

    glGenBuffers(1, &entity_vbo);
    glGenBuffers(1, &entity_ebo);

    has_initialized = true;
    run();
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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
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
    if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
    {
        throw std::runtime_error("couldn't initialize GLAD");
    }
}

void IoClient::deinit()
{
    util::log("IO_CLIENT", util::DEBUG, "deinitializing");
    glfwTerminate();
}
