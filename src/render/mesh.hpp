#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/vector.hpp>
//
#include <render/vertex.hpp>
#include <render/index.hpp>

namespace render
{

struct Mesh
{
    Vector<render::Vertex> vertices;
    Vector<render::Index>   indices;
    GLuint vbo_id;
    GLuint ebo_id;
};

}
