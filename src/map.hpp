#pragma once

#include <config.hpp>
//
#include <include_opengl.hpp>
//
#include <lux_shared/map.hpp>
#include <lux_shared/net/data.hpp>

struct Chunk {
    Arr<VoxelId , CHK_VOL> voxels;
    Arr<LightLvl, CHK_VOL> light_lvls;
};

extern VecSet<ChkPos> chunk_requests;

void map_init();
void map_render();
void light_render();
void map_reload_program();
bool is_chunk_loaded(ChkPos const& pos);
void load_chunk(ChkPos const& pos,
                NetSsSgnl::MapLoad::Chunk const& net_chunk);
void light_update(ChkPos const& pos,
                  NetSsSgnl::LightUpdate::Chunk const& net_chunk);
Chunk const& get_chunk(ChkPos const& pos);
