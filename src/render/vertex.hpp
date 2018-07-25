#pragma once

#include <glm/detail/type_vec3.hpp>
#include <glm/detail/type_vec4.hpp>

namespace render
{

#pragma pack(push, 1)
struct Vertex
{
    Vertex(glm::vec3 pos, glm::vec4 col) : pos(pos), col(col) { }
    glm::vec3 pos;
    glm::vec4 col;
};
#pragma pack(pop)

}
