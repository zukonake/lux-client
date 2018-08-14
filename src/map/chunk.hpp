#pragma once

#include <render/mesh.hpp>
#include <map/tile/tile.hpp>

namespace map
{

struct Chunk
{
    Chunk() : mesh(nullptr) { }
    ~Chunk()
    {
        if(mesh != nullptr) delete mesh;
    }
    Vector<Tile> tiles;
    render::Mesh *mesh;
};

}
