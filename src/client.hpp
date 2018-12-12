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

void add_dbg_point(NetSsTick::DbgInf::Shape::Point const& val,
    Vec4F col = {1.f, 0.f, 1.f, 0.5f}, bool border = true);
void add_dbg_line(NetSsTick::DbgInf::Shape::Line const& val,
    Vec4F col = {1.f, 0.f, 1.f, 0.5f}, bool border = true);
void add_dbg_arrow(NetSsTick::DbgInf::Shape::Arrow const& val,
    Vec4F col = {1.f, 0.f, 1.f, 0.5f}, bool border = true);
void add_dbg_cross(NetSsTick::DbgInf::Shape::Cross const& val,
    Vec4F col = {1.f, 0.f, 1.f, 0.5f}, bool border = true);
void add_dbg_sphere(NetSsTick::DbgInf::Shape::Sphere const& val,
    Vec4F col = {1.f, 0.f, 1.f, 0.5f}, bool border = true);
void add_dbg_triangle(NetSsTick::DbgInf::Shape::Triangle const& val,
    Vec4F col = {1.f, 0.f, 1.f, 0.5f}, bool border = true);
void add_dbg_rect(NetSsTick::DbgInf::Shape::Rect const& val,
    Vec4F col = {1.f, 0.f, 1.f, 0.5f}, bool border = true);

