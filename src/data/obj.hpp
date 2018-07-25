#pragma once

#include <glad/glad.h>
//
#include <data/database.hpp>
#include <data/config.hpp>
#include <render/model.hpp>
#include <map/tile/tile_type.hpp>

map::TileType const void_tile =
{
    "void",
    "Void",
    {0, 0}
};

map::TileType const stone_floor =
{
    "stone_floor",
    "Stone Floor",
    {1, 0}
};

map::TileType const stone_wall =
{
    "stone_wall",
    "Stone Wall",
    {2, 0}
};

data::Database const default_db =
{
    {
        {std::hash<String>()(void_tile.id)  , &void_tile},
        {std::hash<String>()(stone_floor.id), &stone_floor},
        {std::hash<String>()(stone_wall.id) , &stone_wall}
    }
};

data::Config const default_config =
{
    &default_db,
    {16.0, 16.0},
    {32.0, 32.0},
    "tileset.png",
    "glsl/vertex.glsl",
    "glsl/fragment.glsl",
    {
        "localhost",
        31337
    }
};

render::Model const block_model =
{
    {
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0, 1.0}}, //0
        {{0.0, 0.0, 1.0}, {0.0, 1.0, 0.0, 1.0}}, //1
        {{1.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 1.0}}, //2
        {{1.0, 0.0, 1.0}, {1.0, 1.0, 0.0, 1.0}}, //3
        {{1.0, 1.0, 0.0}, {1.0, 1.0, 1.0, 1.0}}, //4
        {{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0, 1.0}}, //5
        {{0.0, 1.0, 0.0}, {1.0, 0.0, 1.0, 1.0}}, //6
        {{0.0, 1.0, 1.0}, {0.5, 0.5, 0.5, 1.0}}  //7
    },
    { 0, 1, 2, 1, 3, 2,
      2, 3, 4, 3, 5, 4,
      4, 5, 6, 5, 7, 6,
      6, 7, 0, 7, 1, 0,
      1, 3, 5, 1, 5, 7,
      0, 4, 2, 4, 6, 0 }
};
