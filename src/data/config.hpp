#pragma once

#include <glm/glm.hpp>
//
#include <lux/alias/c_string.hpp>
#include <lux/linear/vec_2.hpp>

namespace data
{

struct Database;

struct Config
{
    Database const *db;
    glm::vec2 tile_tex_size;
    glm::vec2 tile_quad_size;
    CString   tileset_path;
    CString   vert_shader_path;
    CString   frag_shader_path;
};

}
