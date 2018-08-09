#pragma once

#include <config.h>
#include <data/database.hpp>
#include <data/config.hpp>
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
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
    "glsl/vertex-2.1.glsl",
    "glsl/fragment-2.1.glsl",
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
    "glsl/vertex-es-2.0.glsl",
    "glsl/fragment-es-2.0.glsl",
#else
#   error "Unsupported GL variant selected"
#endif
    {16, 16},
    "tileset.png",
    {
        "localhost",
        31337
    },
    {2, 2, 1},
    "lux client"
};
