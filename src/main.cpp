#include <config.hpp>
//
#include <cstdlib>
#include <cmath>
#include <cstring>
//
#include <enet/enet.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>
//
#include <lux_shared/common.hpp>
#include <lux_shared/util/tick_clock.hpp>
//
#include <db.hpp>
#include <map.hpp>
#include <rendering.hpp>
#include <client.hpp>
#include <entity.hpp>
#include <ui.hpp>

struct {
    Vec2U window_size = {800, 600};
} conf;

void scroll_cb(GLFWwindow* window, F64 d_x, F64 d_y) {
    ImGui_ImplGlfw_ScrollCallback(window, d_x, d_y);
    ui_scroll(get_mouse_pos(), d_y);
}

void mouse_button_cb(GLFWwindow* window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    ui_mouse(get_mouse_pos(), button, action);
}

void window_resize_cb(GLFWwindow*, int win_w, int win_h) {
    static Vec2U old_sz = {1.f, 1.f};
    LUX_LOG("window size change to %ux%u", win_w, win_h);
    glViewport(0, 0, win_w, win_h);
    ui_window_sz_cb(old_sz, {win_w, win_h});
    old_sz = {win_w, win_h};
}

void key_cb(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    ui_key(key, action);
}

void char_cb(GLFWwindow* window, unsigned int ch) {
    ImGui_ImplGlfw_CharCallback(window, ch);
}

void show_error(const char* str) {
    ImGui::Begin("error");
    ImGui::Text("error: %s", str);
    ImGui::End();
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
    entity_init();
    check_opengl_error();
    glfwSetWindowSizeCallback(glfw_window, window_resize_cb);
    glfwSetMouseButtonCallback(glfw_window, mouse_button_cb);
    glfwSetScrollCallback(glfw_window, scroll_cb);
    glfwSetKeyCallback(glfw_window, key_cb);
    glfwSetCharCallback(glfw_window, char_cb);
    UiPaneId eq_pane =
        ui_pane_create(ui_hud, {{-1.f, 0.f}, {0.7f, 1.f}},
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
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if(client_tick(glfw_window) != LUX_OK) {
                LUX_FATAL("game state corrupted");
            }
            if(entity_comps.container.count(ss_tick.player_id) > 0) {
                F32 off = 0.f;
                for(auto const& item : entity_comps.container.at(ss_tick.player_id).items) {
                    if(entity_comps.name.count(item) > 0 &&
                       entity_comps.text.count(item) == 0) {
                        auto const& name = entity_comps.name.at(item);
                        entity_comps.text[item].text =
                            ui_text_create(ui_panes[eq_pane].ui,
                                {{0.f, off}, {0.08f, 0.1f}}, name);
                        off += 0.1f;
                    }
                }
            }
            ui_nodes[ui_camera].tr.pos = -Vec2F(last_player_pos);
            ui_io_tick();
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
