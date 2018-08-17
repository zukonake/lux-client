#pragma once

#include <glm/detail/type_vec2.hpp>

namespace render
{

#pragma pack(push, 1)
struct InterfaceVertex
{
    InterfaceVertex(glm::vec2 pos, glm::vec2 tex_pos) :
        pos(pos), tex_pos(tex_pos) { }
    glm::vec2 pos;
    glm::vec2 tex_pos;
};
#pragma pack(pop)

}
