#pragma once

#include <lux/alias/scalar.hpp>
#include <lux/alias/vector.hpp>
#include <lux/alias/hash_map.hpp>
#include <lux/alias/string.hpp>
#include <lux/common/tile.hpp>

namespace map { struct TileType; }

namespace data
{

class Database //TODO code repetition from server
{
public:
    Database();

    template<typename... Args>
    void add_tile(Args const &...args);

    map::TileType const &get_tile(String const &str_id) const;
    map::TileType const &get_tile(tile::Id id) const;
    tile::Id const &get_tile_id(String const &str_id) const;
private:
    Vector<map::TileType> tiles;
    HashMap<String, tile::Id> tiles_lookup;
};

template<typename... Args>
void Database::add_tile(Args const &...args)
{
    auto &tile = tiles.emplace_back(args...);
    tile.id = tiles.size() - 1;
    tiles_lookup[tile.str_id] = tile.id;
}

}
