#pragma once

#include <lux/alias/vector.hpp>
//
#include <render/vertex.hpp>
#include <render/index.hpp>

namespace render
{

struct Model
{
    Vector<render::Vertex> vertices;
    Vector<render::Index>  indices;
};

}
