#pragma once

#include <glm/glm.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/string.hpp>
#include <lux/net/port.hpp>
#include <lux/alias/vec_3.hpp>

namespace data
{

struct Database;

struct Config
{
    Database const *db;
    String   vert_shader_path;
    String   frag_shader_path;
    struct
    {
        String hostname;
        net::Port port;
    } server;
    Vec3<U8> view_range;
    String   client_name;
};

}
