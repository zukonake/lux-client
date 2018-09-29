#pragma once

#include <lux_shared/common.hpp>

void client_init(char const* server_hostname, U16 server_port, F64& tick_rate);
void client_deinit();
void client_tick(GLFWwindow* glfw_window, Vec3F& player_pos);
void client_quit();
bool client_should_close();
LUX_MAY_FAIL send_command(char const* str);
