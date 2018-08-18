#pragma once

#include <glm/detail/type_vec2.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/detail/type_vec4.hpp>

namespace render
{

#pragma pack(push, 1)
struct Vertex
{
    Vertex(glm::vec3 pos, glm::vec4 col, glm::vec2 tex_pos) :
        pos(pos), col(col), tex_pos(tex_pos) { }
    glm::vec3 pos;
    glm::vec4 col;
    glm::vec2 tex_pos;
};
#pragma pack(pop)

}
