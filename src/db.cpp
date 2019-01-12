#include <config.hpp>
//
#include <functional>
//
#include "db.hpp"

static DynArr<BlockBp>    blocks;
static DynArr<EntitySprite> entities;
static HashMap<Str, BlockId> blocks_lookup;

void add_block(BlockBp &&block_bp) {
    auto &block = blocks.push(std::move(block_bp));
    blocks_lookup[block.str_id] = blocks.len - 1;
    LUX_LOG("added block %zu: \"%.*s\"",
            blocks.len - 1, (int)block.str_id.len, block.str_id.beg);
}

void db_init() {
    add_block({"void"_l       , "Void"_l       , {0, 0}, false});
    add_block({"stone_floor"_l, "Stone Floor"_l, {1, 0}, false});
    add_block({"stone_wall"_l , "Stone Wall"_l , {0, 2}, true });
    add_block({"raw_stone"_l  , "Raw Stone"_l  , {0, 2}, true });
    add_block({"dirt"_l       , "Dirt"_l       , {3, 0}, false});
    add_block({"grass"_l      , "Grass"_l      , {0, 1}, false});
    add_block({"dark_grass"_l , "Dark grass"_l , {1, 1}, false});
    add_block({"tree_trunk"_l , "Tree trunk"_l , {4, 0}, false});
    add_block({"tree_leaves"_l, "Tree leaves"_l, {4, 2}, true });
    add_block({"dbg_block_0"_l, "Debug Block 0"_l, {2, 1}, false});
    add_block({"dbg_block_1"_l, "Debug Block 1"_l, {3, 1}, false});
    entities.push({{0, 0}, {1, 1}});
    entities.push({{0, 2}, {1, 1}});
    entities.push({{1, 0}, {1, 1}});
}

EntitySprite const& db_entity_sprite(U32 id) {
    LUX_ASSERT(id < entities.len);
    return entities[id];
}

BlockBp const& db_block_bp(BlockId id) {
    LUX_ASSERT(id < blocks.len);
    return blocks[id];
}

BlockBp const& db_block_bp(Str const& str_id) {
    return db_block_bp(db_block_id(str_id));
}

BlockId const& db_block_id(Str const& str_id) {
    LUX_ASSERT(blocks_lookup.count(str_id) > 0);
    return blocks_lookup.at(str_id);
}
