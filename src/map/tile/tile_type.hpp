#pragma once

#include <lux/alias/string.hpp>
//
#include <render/common.hpp>

namespace map
{

struct TileType
{
    String         name;
    String         id;
    render::TexPos tex_pos;
};

}
