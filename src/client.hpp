#pragma once

#include <config.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/entity.hpp>
#include <lux_shared/net/data.hpp>

extern EntityVec last_player_pos;
extern F64 tick_rate;

extern NetCsTick cs_tick;
extern NetCsSgnl cs_sgnl;
extern NetSsTick ss_tick;
extern NetSsSgnl ss_sgnl;

LUX_MAY_FAIL client_init(char const* server_hostname, U16 server_port);
void client_deinit();
LUX_MAY_FAIL client_tick(GLFWwindow* glfw_window);
void client_quit();
bool client_should_close();
LUX_MAY_FAIL send_command(char const* str);
