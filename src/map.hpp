#pragma once

#include <config.hpp>
//
#include <include_opengl.hpp>
//
#include <lux_shared/map.hpp>
#include <lux_shared/net/data.hpp>
//
#include <db.hpp>

struct Chunk {
    bool loaded = false;
    Arr<BlockId, CHK_VOL> blocks;
};

extern VecSet<ChkPos> chunk_requests;

void map_init();
void map_reload_program();
bool is_chunk_loaded(ChkPos const& pos);
void map_load_chunks(NetSsSgnl::ChunkLoad const& net_chunks);
void map_update_chunks(NetSsSgnl::ChunkUpdate const& net_chunks);
///@NOTE: if you modify the chunk, you probably have to rebuild the mesh
Chunk& get_chunk(ChkPos const& pos);
BlockId get_block(MapPos const& pos);
BlockBp const& get_block_bp(MapPos const& pos);
