#include <map/tile/tile_type.hpp>
#include "database.hpp"

namespace data
{

Database::Database()
{
    add_tile("void", "Void", render::TexPos(0, 0));
    add_tile("stone_floor", "Stone Floor", render::TexPos(1, 0));
    add_tile("stone_wall", "Stone Wall", render::TexPos(2, 0));
    add_tile("raw_stone", "Raw Stone", render::TexPos(3, 0));
    add_tile("dirt", "Dirt", render::TexPos(0, 1));
    add_tile("grass", "Grass", render::TexPos(1, 1));
}

map::TileType const &Database::get_tile(String const &str_id) const
{
    return tiles[get_tile_id(str_id)];
}

map::TileType const &Database::get_tile(tile::Id id) const
{
    return tiles[id];
}

tile::Id const &Database::get_tile_id(String const &str_id) const
{
    return tiles_lookup.at(str_id);
}

}

