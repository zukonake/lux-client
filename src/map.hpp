#pragma once

#include <glad/glad.h>
//
#include <lux_shared/map.hpp>
#include <lux_shared/net/data.hpp>
//
#include <db.hpp>

extern VecSet<ChkPos> chunk_requests;

void map_init();
void map_load_chunks(NetSsSgnl::ChunkLoad const& net_chunks);
void map_update_chunks(NetSsSgnl::ChunkUpdate const& net_chunks);
