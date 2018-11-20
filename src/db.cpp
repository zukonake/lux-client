#include <config.hpp>
//
#include <functional>
//
#include "db.hpp"

static DynArr<TileBp>    tiles;
static DynArr<EntitySprite> entities;
static SortMap<DynStr, TileId> tiles_lookup;

void add_tile(TileBp &&tile_bp) {
    auto &tile = tiles.emplace_back(tile_bp);
    tiles_lookup[tile.str_id] = tiles.size() - 1;
}

void db_init() {
    add_tile({"void", "Void"              , {0, 0}, false});
    add_tile({"stone_floor", "Stone Floor", {1, 0}, false});
    add_tile({"stone_wall", "Stone Wall"  , {0, 2}, true});
    add_tile({"raw_stone", "Raw Stone"    , {0, 2}, true});
    add_tile({"dirt", "Dirt"              , {3, 0}, false});
    add_tile({"grass", "Grass"            , {0, 1}, false});
    add_tile({"dark_grass", "Dark grass"  , {1, 1}, false});
    add_tile({"tree_trunk", "Tree trunk"  , {4, 0}, false});
    add_tile({"tree_leaves", "Tree leaves", {4, 2}, true});
    entities.push_back({{0, 0}, {1, 1}});
    entities.push_back({{0, 2}, {1, 1}});
    entities.push_back({{1, 0}, {1, 1}});
}

EntitySprite const& db_entity_sprite(U32 id) {
    LUX_ASSERT(id < entities.size());
    return entities[id];
}

TileBp const& db_tile_bp(TileId id) {
    LUX_ASSERT(id < tiles.size());
    return tiles[id];
}

TileBp const& db_tile_bp(SttStr const& str_id) {
    return db_tile_bp(db_tile_id(str_id));
}

TileId const& db_tile_id(SttStr const& str_id) {
    LUX_ASSERT(tiles_lookup.count(str_id) > 0);
    return tiles_lookup.at(str_id);
}
