#pragma once

#include <lux/alias/string.hpp>
#include <lux/common/tile.hpp>
//
#include <render/common.hpp>

namespace map
{

struct TileType
{
    tile::Id       id;
    String         str_id;
    String         name;
    render::TexPos tex_pos;

    TileType(String const &_str_id, String const &_name,
             render::TexPos _tex_pos) :
        str_id(_str_id), name(_name), tex_pos(_tex_pos)
    {

    }
};

}
