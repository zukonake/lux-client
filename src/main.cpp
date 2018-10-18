#include <config.hpp>
//
#include <cstdlib>
#include <cmath>
//
#include <enet/enet.h>
//
#include <lux_shared/common.hpp>
#include <lux_shared/util/tick_clock.hpp>
//
#include <db.hpp>
#include <map.hpp>
#include <console.hpp>
#include <rendering.hpp>
#include <client.hpp>
#include <entity.hpp>
#include <ui.hpp>
#include <viewport.hpp>

struct {
    Vec2U window_size = {800, 600};
} conf;

void window_resize_cb(GLFWwindow* window, int win_w, int win_h)
{
    (void)window;
    LUX_LOG("window size change to %ux%u", win_w, win_h);
    glViewport(0, 0, win_w, win_h);
    console_window_sz_cb({win_w, win_h});
    ui_window_sz_cb({win_w, win_h});
    world_viewport.scale.y = -world_viewport.scale.x * ((F32)win_w / (F32)win_h);
}

void key_cb(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    //@CONSIDER a centralized input system
    console_key_cb(key, scancode, action, mods);
}

int main(int argc, char** argv) {
    char const* server_hostname;
    U16 server_port;

    { ///read commandline args
        if(argc != 3) {
            LUX_FATAL("usage: %s SERVER_HOSTNAME SERVER_PORT", argv[0]);
        }
        U64 raw_server_port = std::atol(argv[2]);
        if(raw_server_port >= 1 << 16) {
            LUX_FATAL("invalid port %zu given", raw_server_port);
        }
        server_hostname = argv[1];
        server_port = raw_server_port;
    }

    if(client_init(server_hostname, server_port) != LUX_OK) {
        LUX_FATAL("failed to initialize client");
    }
    LUX_DEFER { client_deinit(); };
    db_init();
    rendering_init();
    LUX_DEFER { rendering_deinit(); };
    map_init();
    ui_init();
    console_init();
    LUX_DEFER { console_deinit(); };
    entity_init();
    check_opengl_error();
    glfwSetWindowSizeCallback(glfw_window, window_resize_cb);
    glfwSetKeyCallback(glfw_window, key_cb);
    Vec2<int> win_size;
    glfwGetWindowSize(glfw_window, &win_size.x, &win_size.y);
    window_resize_cb(glfw_window, win_size.x, win_size.y);

    { ///main loop
        auto tick_len = util::TickClock::Duration(1.0 / tick_rate);
        util::TickClock clock(tick_len);
        while(!client_should_close()) {
            clock.start();
            glfwPollEvents();
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            if(client_tick(glfw_window) != LUX_OK) {
                LUX_FATAL("game state corrupted");
            }
            world_viewport.pos = -Vec2F(last_player_pos);
            map_render();
            entity_render();
            console_render();
            check_opengl_error();
            glfwSwapBuffers(glfw_window);

            clock.stop();
            auto remaining = clock.synchronize();
            if(remaining < util::TickClock::Duration::zero()) {
                LUX_LOG("tick overhead of %.2fs", std::abs(remaining.count()));
            }
        }
    }

    return 0;
}
