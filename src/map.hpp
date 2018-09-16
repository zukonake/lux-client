#pragma once

#include <lux_shared/map.hpp>
#include <lux_shared/net/data.hpp>

struct Chunk {
    Arr<VoxelId ,  CHK_VOL> voxels;
    Arr<LightLvl,  CHK_VOL> light_lvls;
};

void load_chunk(NetServerSignal::MapLoad::Chunk const& net_chunk);
