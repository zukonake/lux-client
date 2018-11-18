#pragma once

#include <config.hpp>
//
#include <include_opengl.hpp>
//
#include <lux_shared/map.hpp>
#include <lux_shared/net/data.hpp>

struct Chunk {
    Arr<LightLvl, CHK_VOL> light_lvl = {};
    Arr<TileId  , CHK_VOL> floor;
    Arr<TileId  , CHK_VOL> wall;
    Arr<TileId  , CHK_VOL> roof;
};

extern VecSet<ChkPos> chunk_requests;

void map_init();
void map_reload_program();
bool is_chunk_loaded(ChkPos const& pos);
void tiles_update(ChkPos const& pos,
                  NetSsSgnl::Tiles::Chunk const& net_chunk);
void light_update(ChkPos const& pos,
                  NetSsSgnl::Light::Chunk const& net_chunk);
Chunk const& get_chunk(ChkPos const& pos);
