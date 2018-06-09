#pragma once

#include <glm/glm.hpp>
//
#include <alias/cstring.hpp>
#include <linear/size_2d.hpp>

namespace data
{

struct Config
{
    glm::vec2 tile_tex_size;
    glm::vec2 tile_quad_size;
    CString   tileset_path;
    CString   vert_shader_path;
    CString   frag_shader_path;
};

}
