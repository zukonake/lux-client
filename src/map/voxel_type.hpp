#pragma once

#include <lux/alias/string.hpp>
#include <lux/world/map.hpp>
//
#include <render/common.hpp>

struct VoxelType
{
    String  str_id;
    String  name;
    render::TexPos tex_pos;

    VoxelType(String const &_str_id, String const &_name,
             render::TexPos _tex_pos) :
        str_id(_str_id), name(_name), tex_pos(_tex_pos)
    {

    }
};
