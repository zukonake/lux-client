#pragma once

#include <lux/alias/vec_2.hpp>
#include <lux/world/map.hpp>

#pragma pack(push, 1)
struct GeometryVert
{
    GeometryVert(MapPos pos, Vec2F tex_pos) :
        pos(pos), tex_pos(tex_pos) { }
    MapPos pos;
    Vec2F  tex_pos;
};
#pragma pack(pop)
