#pragma once

#include <lux/alias/vector.hpp>
//
#include <render/mesh.hpp>
#include <map/tile/tile.hpp>

namespace map
{

struct Chunk
{
    Vector<Tile> tiles;
    render::Mesh mesh;
    bool is_mesh_generated;
};

}
