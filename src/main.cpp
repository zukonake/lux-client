#include <config.hpp>
//
#include <cstdlib>
#include <cmath>
#include <cstring>
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

struct {
    Vec2U window_size = {800, 600};
} conf;

void scroll_cb(GLFWwindow* window, F64, F64 off) {
    ui_scroll(get_mouse_pos(), off);
}

void mouse_button_cb(GLFWwindow*, int button, int action, int) {
    ui_mouse_button(get_mouse_pos(), button, action);
}

void window_resize_cb(GLFWwindow*, int win_w, int win_h)
{
    static Vec2U old_sz = conf.window_size;
    LUX_LOG("window size change to %ux%u", win_w, win_h);
    glViewport(0, 0, win_w, win_h);
    console_window_sz_cb({win_w, win_h});
    ui_window_sz_cb(old_sz, {win_w, win_h});
    old_sz = {win_w, win_h};
}

void key_cb(GLFWwindow*, int key, int scancode, int action, int mods)
{
    //@CONSIDER a centralized input system
    console_key_cb(key, scancode, action, mods);
}

int main(int argc, char** argv) {
    char const* server_hostname = "localhost";
    U16 server_port = 31337;

    { ///read commandline args
        if(argc == 1) {
            LUX_LOG("no commandline arguments given");
            LUX_LOG("assuming server %s:%u", server_hostname, server_port);
        } else {
            if(argc != 3) {
                LUX_FATAL("usage: %s SERVER_HOSTNAME SERVER_PORT", argv[0]);
            }
            //TODO error handling (also in client)
            U64 raw_server_port = std::atol(argv[2]);
            if(raw_server_port >= 1 << 16) {
                LUX_FATAL("invalid port %zu given", raw_server_port);
            }
            server_hostname = argv[1];
            server_port = raw_server_port;
        }
    }

    if(client_init(server_hostname, server_port) != LUX_OK) {
        LUX_FATAL("failed to initialize client");
    }
    LUX_DEFER { client_deinit(); };
    db_init();
    rendering_init();
    LUX_DEFER { rendering_deinit(); };
    ui_init();
    map_init();
    console_init();
    LUX_DEFER { console_deinit(); };
    entity_init();
    check_opengl_error();
    glfwSetWindowSizeCallback(glfw_window, window_resize_cb);
    glfwSetMouseButtonCallback(glfw_window, mouse_button_cb);
    glfwSetScrollCallback(glfw_window, scroll_cb);
    glfwSetKeyCallback(glfw_window, key_cb);
    UiPaneId coord_pane =
        ui_pane_create(ui_hud, {{-1.f, -1.f}, {0.4f, 0.1f}},
                       {0.5f, 0.5f, 0.5f, 0.5f});
    UiTextId coord_txt =
        ui_text_create(ui_panes[coord_pane].ui,
                       {{0.f, 0.f}, {0.08f, 0.5f}}, "");
    UiPaneId eq_pane =
        ui_pane_create(ui_hud, {{-1.f, 0.f}, {0.8f, 1.f}},
                       {0.5f, 0.5f, 0.5f, 0.5f});

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
            if(entity_comps.container.count(ss_tick.player_id) > 0) {
                F32 off = 0.f;
                for(auto const& item : entity_comps.container.at(ss_tick.player_id).items) {
                    if(entity_comps.name.count(item) > 0 &&
                       entity_comps.text.count(item) == 0) {
                        auto const& name = entity_comps.name.at(item);
                        DynStr name_str(name.cbegin(), name.cend());
                        entity_comps.text[item].text =
                            ui_text_create(ui_panes[eq_pane].ui,
                                {{0.f, off}, {0.08f, 0.1f}}, name_str.c_str());
                        off += 0.1f;
                    }
                }
            }
            DynStr coord_str =
                 "x: " + std::to_string(last_player_pos.x) +
             "\\\ny: " + std::to_string(last_player_pos.y);
            ui_texts[coord_txt].buff.resize(coord_str.size());
            std::memcpy(ui_texts[coord_txt].buff.data(),
                        coord_str.data(), coord_str.size());
            ui_nodes[ui_camera].tr.pos = -Vec2F(last_player_pos);
            ui_render();
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
