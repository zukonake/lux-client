#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
//
#include <lux/alias/vector.hpp>

namespace render
{

struct Model
{
    Vector<glm::vec3> vertices;
    Vector<GLuint>    indices;
    GLenum            primitive;
};

}