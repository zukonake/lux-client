#pragma once

#include <glad/glad.h>
//
#include <lux/alias/vector.hpp>
#include <lux/common/chunk.hpp>
//
#include <render/vertex.hpp>
#include <render/index.hpp>
#include <map/tile/tile.hpp>

namespace map
{

struct Chunk
{
    Vector<Tile> tiles;

    Vector<render::Vertex> vertices;
    Vector<render::Index>   indices;
    GLuint vbo_id;
    GLuint ebo_id;
};

}
