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

map::TileType const raw_stone =
{
    "raw_stone",
    "Raw Stone",
    {3, 0}
};

map::TileType const dirt =
{
    "dirt",
    "Dirt",
    {0, 1}
};

map::TileType const grass =
{
    "grass",
    "Grass",
    {1, 1}
};

data::Database const default_db =
{
    {
        {std::hash<String>()(void_tile.id)  , &void_tile},
        {std::hash<String>()(stone_floor.id), &stone_floor},
        {std::hash<String>()(stone_wall.id) , &stone_wall},
        {std::hash<String>()(raw_stone.id) , &raw_stone},
        {std::hash<String>()(dirt.id) , &dirt},
        {std::hash<String>()(grass.id) , &grass}
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
    "tileset.png",
    {800, 600},
    {16, 16},
    {
        "localhost",
        31337
    },
    6,
    6,
    "lux client"
};
