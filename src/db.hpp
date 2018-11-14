#pragma once

#include <config.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>

struct TileBp {
    DynStr str_id;
    DynStr name;
    Vec2U tex_pos;
    bool connected_tex;
};

struct EntitySprite {
    Vec2<U8> pos;
    Vec2<U8> sz;
};

void db_init();
EntitySprite const& db_entity_sprite(U32 id);
TileBp const& db_tile_bp(TileId id);
TileBp const& db_tile_bp(SttStr const& str_id);
TileId const& db_tile_id(SttStr const& str_id);
