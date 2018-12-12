#pragma once

#include <config.hpp>
//
#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>

struct TileBp {
    TileBp(Str _str_id, Str _name, Vec2U _tex_pos, bool _connected_tex) :
        str_id(_str_id),
        name(_name),
        tex_pos(_tex_pos),
        connected_tex(_connected_tex) { }
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
TileBp const& db_tile_bp(TileId id);
TileBp const& db_tile_bp(Str const& str_id);
TileId const& db_tile_id(Str const& str_id);
