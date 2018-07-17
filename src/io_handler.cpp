#include <cstring>
#include <stdexcept>
#include <sstream>
#include <fstream>
//
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <lodepng.h>
//
#include <lux/util/log.hpp>
#include <lux/alias/cstring.hpp>
#include <lux/alias/string.hpp>
#include <lux/linear/vec_2.hpp>
//
#include "io_handler.hpp"

IoHandler::IoHandler(data::Config const &config, double fps) :
    config(config),
    tick_clock(util::TickClock::Duration(1.0 / fps)),
    initialized(false)
{
    thread = std::thread(&IoHandler::start, this);
    // ^ all the opengl related stuff must be initialized in the corresponding thread
}

IoHandler::~IoHandler()
{
    glfwSetWindowShouldClose(glfw_window, GLFW_TRUE);
    thread.join();
    glfwTerminate();
}

void IoHandler::receive(net::ServerData const &sd)
{
    std::lock_guard lock(io_mutex);
    sd_buffer = sd;
}

void IoHandler::send(net::ClientData &cd)
{
    std::lock_guard lock(io_mutex);
    cd = cd_buffer;
}

bool IoHandler::should_close()
{
    return initialized && glfwWindowShouldClose(glfw_window);
    // order is important, if initialized == false,
    // the second operand won't be evaluated
}

void IoHandler::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    IoHandler *io_handler = (IoHandler *)glfwGetWindowUserPoser(window);
    io_handler->set_view_size({width  / io_handler->config.tile_quad_size.x + 2,
                               height / io_handler->config.tile_quad_size.y + 2}); //TODO
    util::log("IO_HANDLER", util::DEBUG, "screen size change to %ux%u", width, height);
}

void IoHandler::error_callback(int err, const char* desc)
{
    throw std::runtime_error(String("GLFW error: ") +
                             std::to_string(err) +
                             String("; ") + desc);
}

void IoHandler::key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    (void)key;
    (void)window;
    (void)scancode;
    (void)action;
    (void)mode;
}

void IoHandler::start()
{
    init_glfw_core();
    init_glfw_window();
    init_glad();

    glfwSwapInterval(1);
    //^ TODO not sure if needed since io loop is regulated by tick_clock anyway

    program.init(config.vert_shader_path, config.frag_shader_path);
    init_vbo();
    init_ebo();
    init_vert_attribs();
    init_tileset();

    framebuffer_size_callback(glfw_window, 800, 600);

    initialized = true;

    run();
}

void IoHandler::init_glfw_core()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing GLFW core");
    glfwInit();
    glfwSetErrorCallback(error_callback);
}

void IoHandler::init_glfw_window()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing GLFW window");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfw_window = glfwCreateWindow(800, 600, "Lux", NULL, NULL);
    glfwMakeContextCurrent(glfw_window);
    glfwSetWindowUserPoser(glfw_window, this);
    glfwSetFramebufferSizeCallback(glfw_window, framebuffer_size_callback);
    glfwSetKeyCallback(glfw_window, key_callback);
}

void IoHandler::init_glad()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing GLAD");
    if(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
    {
        throw std::runtime_error("couldn't initialize GLAD");
    }
}

void IoHandler::init_vbo()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing VBO");
    glGenBuffers(1, &vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
}

void IoHandler::init_ebo()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing EBO");
    glGenBuffers(1, &ebo_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_id);
}

void IoHandler::init_vert_attribs()
{
    util::log("IO_HANDLER", util::DEBUG, "initializing vertex attributes");
    static_assert(sizeof(render::Vertex) == 3 * sizeof(GLfloat) + 
                                            2 * sizeof(GLfloat) +
                                            4 * sizeof(GLfloat));
    glVertexAttribPoser(0, 3, GL_FLOAT, GL_FALSE, sizeof(render::Vertex),
                          (void*)offsetof(render::Vertex, pos));
    glVertexAttribPoser(1, 2, GL_FLOAT, GL_FALSE, sizeof(render::Vertex),
                          (void*)offsetof(render::Vertex, tex_pos));
    glVertexAttribPoser(2, 4, GL_FLOAT, GL_FALSE, sizeof(render::Vertex),
                          (void*)offsetof(render::Vertex, color));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
}

void IoHandler::init_tileset()
{
    util::log("IO_HANDLER", util::DEBUG, "loading tileset texture %s", config.tileset_path);

    glGenTextures(1, &tileset_id);
    glBindTexture(GL_TEXTURE_2D, tileset_id);
    Vector<U8> image;
    linear::Vec2<unsigned> image_size;
    unsigned error = lodepng::decode(image,
                                     image_size.x,
                                     image_size.y,
                                     String(config.tileset_path));
    tileset_size = (glm::vec2)image_size;

    auto const &tile_size = config.tile_tex_size;
    glm::vec2 tile_scale = {tile_size.x / tileset_size.x, tile_size.y / tileset_size.y};
    program.set_uniform("tile_scale", glUniform2f, tile_scale.x, tile_scale.y);

    if(error) throw std::runtime_error("couldn't load tileset texture");

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 tileset_size.x,
                 tileset_size.y,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 image.data());

    //glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    // TODO ^ no mipmaps unless I find a way to generate them reliably
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    util::log("IO_HANDLER",
              util::DEBUG,
              "loaded tileset texture of size %ux%u",
              tileset_size.x,
              tileset_size.y);
}

void IoHandler::run()
{
    util::log("IO_HANDLER", util::DEBUG, "IO loop started");
    while(!glfwWindowShouldClose(glfw_window))
    {
        tick_clock.start();
        render();
        handle_input();
        tick_clock.stop();
        auto delta = tick_clock.synchronize();
        if(delta < util::TickClock::Duration::zero())
        {
            util::log("IO_HANDLER", util::WARN, "io loop overhead of %f seconds",
                      std::abs(delta.count()));
        }

    }
    util::log("IO_HANDLER", util::DEBUG, "IO loop stopped");
}

void IoHandler::render()
{
    if(sd_buffer.tiles.size() == view_size.x * view_size.y)
    {
        std::lock_guard lock(io_mutex);
        auto tiles_size = sd_buffer.tiles.size();
        vertices.resize(tiles_size * 4);
        glm::mat3x2 size(2.0 / view_size.x, 0                ,
                         0                , 2.0 / view_size.y,
                         0                , 0                );
        for(SizeT i = 0; i < tiles_size; ++i)
        {
            auto const &tile = sd_buffer.tiles[i];
            glm::vec3 base((i % view_size.x) - (float)(view_size.x / 2),
                           (i / view_size.x) - (float)(view_size.y / 2),
                           tile.shape == net::TileState::WALL ? 1 : 0);
            base.x -= sd_buffer.player_pos.x - (int)sd_buffer.player_pos.x;
            base.y -= sd_buffer.player_pos.y - (int)sd_buffer.player_pos.y;
            vertices[(i * 4) + 0].pos = (base + glm::vec3(0, 0, 0)) * size;
            vertices[(i * 4) + 1].pos = (base + glm::vec3(1, 0, 0)) * size;
            vertices[(i * 4) + 2].pos = (base + glm::vec3(1, 1, 0)) * size;
            vertices[(i * 4) + 3].pos = (base + glm::vec3(0, 1, 0)) * size;
            vertices[(i * 4) + 0].tex_pos = glm::vec2(0, 0) + (glm::vec2)tile.tex_pos;
            vertices[(i * 4) + 1].tex_pos = glm::vec2(1, 0) + (glm::vec2)tile.tex_pos;
            vertices[(i * 4) + 2].tex_pos = glm::vec2(1, 1) + (glm::vec2)tile.tex_pos;
            vertices[(i * 4) + 3].tex_pos = glm::vec2(0, 1) + (glm::vec2)tile.tex_pos;
            base.x += sd_buffer.player_pos.x - (int)sd_buffer.player_pos.x;
            base.y += sd_buffer.player_pos.y - (int)sd_buffer.player_pos.y;
            bool found = false;
            for(auto const &entity : sd_buffer.entities)
            {
                EntityPos map_point = base + sd_buffer.player_pos;
                map_point.z = sd_buffer.player_pos.z;
                if(glm::distance(entity, map_point) <= 0.5)
                {
                    found = true;
                }
            }
            if(found)
            {
                vertices[(i * 4) + 0].color = {1.0, 0.0, 0.0, 1.0};
                vertices[(i * 4) + 1].color = {1.0, 0.0, 0.0, 1.0};
                vertices[(i * 4) + 2].color = {1.0, 0.0, 0.0, 1.0};
                vertices[(i * 4) + 3].color = {1.0, 0.0, 0.0, 1.0};
            }
            else
            {
                vertices[(i * 4) + 0].color = {1.0, 1.0, 1.0, 1.0};
                vertices[(i * 4) + 1].color = {1.0, 1.0, 1.0, 1.0};
                vertices[(i * 4) + 2].color = {1.0, 1.0, 1.0, 1.0};
                vertices[(i * 4) + 3].color = {1.0, 1.0, 1.0, 1.0};
            }
        }
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(render::Vertex) * vertices.size(),
                     vertices.data(),
                     GL_STREAM_DRAW);
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glfwSwapBuffers(glfw_window);
}

void IoHandler::handle_input()
{
    {
        std::lock_guard lock(io_mutex);
        cd_buffer.view_size = view_size;
        cd_buffer.is_moving = false;
        if(glfwGetKey(glfw_window, GLFW_KEY_A))
        {
            cd_buffer.character_dir.x = -1.0;
            cd_buffer.is_moving = true;
        }
        else if(glfwGetKey(glfw_window, GLFW_KEY_D))
        {
            cd_buffer.character_dir.x = 1.0;
            cd_buffer.is_moving = true;
        }
        else cd_buffer.character_dir.x = 0.0;
        if(glfwGetKey(glfw_window, GLFW_KEY_W))
        {
            cd_buffer.character_dir.y = 1.0;
            cd_buffer.is_moving = true;
        }
        else if(glfwGetKey(glfw_window, GLFW_KEY_S))
        {
            cd_buffer.character_dir.y = -1.0;
            cd_buffer.is_moving = true;
        }
        else cd_buffer.character_dir.y = 0.0;
    }
    glfwPollEvents();
}

void IoHandler::set_view_size(linear::Vec2<U16> const &val)
{
    view_size = val;
    util::log("IO_HANDLER", util::DEBUG, "view size change to %ux%u", val.x, val.y);
    resize_indices();
}

void IoHandler::resize_indices()
{
    SizeT view_tiles = view_size.x * view_size.y;
    indices.resize(view_tiles * 6);
    for(SizeT i = 0; i < view_tiles; ++i)
    {
        indices[(i * 6) + 0] = (GLuint)((i * 4) + 0);
        indices[(i * 6) + 1] = (GLuint)((i * 4) + 1);
        indices[(i * 6) + 2] = (GLuint)((i * 4) + 3);
        indices[(i * 6) + 3] = (GLuint)((i * 4) + 1);
        indices[(i * 6) + 4] = (GLuint)((i * 4) + 2);
        indices[(i * 6) + 5] = (GLuint)((i * 4) + 3);
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(GLuint) * indices.size(),
                 indices.data(),
                 GL_DYNAMIC_DRAW);
}
