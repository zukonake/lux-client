#pragma once

#include <glm/glm.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/c_string.hpp>
#include <lux/net/port.hpp>
#include <lux/alias/vec_3.hpp>

namespace data
{

struct Database;

struct Config
{
    Database const *db;
    CString   vert_shader_path;
    CString   frag_shader_path;
    struct
    {
        CString hostname;
        net::Port port;
    } server;
    Vec3<U8> view_range;
};

}
