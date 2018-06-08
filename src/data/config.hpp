#pragma once

#include <alias/cstring.hpp>
#include <linear/size_2d.hpp>

namespace data
{

struct Config
{
    linear::Size2d<U16> tile_pixel_size;
    CString             vertex_shader_path;
    CString             fragment_shader_path;
};

}
