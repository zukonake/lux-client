#include <config.hpp>
//
#include <functional>
//
#include "db.hpp"

static DynArr<TileBp>    tiles;
static DynArr<EntitySprite> entities;
static HashMap<Str, TileId> tiles_lookup;

void add_tile(TileBp &&tile_bp) {
    auto &tile = tiles.push(std::move(tile_bp));
    tiles_lookup[tile.str_id] = tiles.len - 1;
    LUX_LOG("added tile %zu: \"%.*s\"",
            tiles.len - 1, (int)tile.str_id.len, tile.str_id.beg);
}

void db_init() {
    add_tile({"void"_l       , "Void"_l       , {0, 0}, false});
    add_tile({"stone_floor"_l, "Stone Floor"_l, {1, 0}, false});
    add_tile({"stone_wall"_l , "Stone Wall"_l , {0, 2}, true });
    add_tile({"raw_stone"_l  , "Raw Stone"_l  , {0, 2}, true });
    add_tile({"dirt"_l       , "Dirt"_l       , {3, 0}, false});
    add_tile({"grass"_l      , "Grass"_l      , {0, 1}, false});
    add_tile({"dark_grass"_l , "Dark grass"_l , {1, 1}, false});
    add_tile({"tree_trunk"_l , "Tree trunk"_l , {4, 0}, false});
    add_tile({"tree_leaves"_l, "Tree leaves"_l, {4, 2}, true });
    entities.push({{0, 0}, {1, 1}});
    entities.push({{0, 2}, {1, 1}});
    entities.push({{1, 0}, {1, 1}});
}

EntitySprite const& db_entity_sprite(U32 id) {
    LUX_ASSERT(id < entities.len);
    return entities[id];
}

TileBp const& db_tile_bp(TileId id) {
    LUX_ASSERT(id < tiles.len);
    return tiles[id];
}

TileBp const& db_tile_bp(Str const& str_id) {
    return db_tile_bp(db_tile_id(str_id));
}

TileId const& db_tile_id(Str const& str_id) {
    LUX_ASSERT(tiles_lookup.count(str_id) > 0);
    return tiles_lookup.at(str_id);
}
