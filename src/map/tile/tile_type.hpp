#pragma once

#include <lux/alias/string.hpp>
//
#include <render/common.hpp>

namespace map
{

struct TileType
{
    String         id;
    String         name;
    render::TexPos tex_pos;
};

}
