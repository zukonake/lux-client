#pragma once

#include <glm/detail/type_vec3.hpp>
#include <glm/detail/type_vec4.hpp>
#include <glm/detail/type_vec.hpp>

namespace render
{

#pragma pack(push, 1)
struct Vertex
{
    glm::vec3  pos;
    glm::vec2 tex_pos;
    glm::vec4  color;
};
#pragma pack(pop)

}
