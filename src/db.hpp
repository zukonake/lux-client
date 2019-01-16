#pragma once

#include <config.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>

struct BlockBp {
    StrBuff str_id;
    StrBuff name;
    Vec2U tex_pos;
    bool connected_tex;
};

struct EntitySprite {
    Vec2<U8> pos;
    Vec2<U8> sz;
};

void db_init();
EntitySprite const& db_entity_sprite(U32 id);
BlockBp const& db_block_bp(BlockId id);
BlockBp const& db_block_bp(Str const& str_id);
BlockId const& db_block_id(Str const& str_id);
