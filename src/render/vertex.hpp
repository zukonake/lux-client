#pragma once

#include <lux/alias/vec_2.hpp>
#include <lux/alias/vec_3.hpp>
#include <lux/alias/vec_4.hpp>

namespace render
{

#pragma pack(push, 1)
struct Vertex
{
    Vertex(Vec3F pos, Vec4F col, Vec2F tex_pos) :
        pos(pos), col(col), tex_pos(tex_pos) { }
    Vec3F pos;
    Vec4F col;
    Vec2F tex_pos;
};
#pragma pack(pop)

}
