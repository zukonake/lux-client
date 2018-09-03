#pragma once

#include <glm/detail/type_vec2.hpp>

namespace render
{

#pragma pack(push, 1)
struct InterfaceVertex
{
    InterfaceVertex(Vec2F pos, Vec2F tex_pos) :
        pos(pos), tex_pos(tex_pos) { }
    Vec2F pos;
    Vec2F tex_pos;
};
#pragma pack(pop)

}
