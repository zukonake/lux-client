#include <lux_shared/common.hpp>
#include <lux_shared/map.hpp>
#include <lux_shared/net/net_order.hpp>
#include <lux_shared/util/packer.hpp>
//
#include "map.hpp"

HashMap<ChkPos, Chunk, util::Packer<ChkPos>> chunks;

bool is_chunk_loaded(ChkPos const& pos) {
    return chunks.count(pos) > 0;
}

void load_chunk(NetServerSignal::MapLoad::Chunk const& net_chunk) {
    ChkPos pos = net_order(net_chunk.pos);
    LUX_LOG("loading chunk");
    LUX_LOG("    pos: {%zu, %zu, %zu}", pos.x, pos.y, pos.z);
    if(is_chunk_loaded(pos)) {
        LUX_LOG("chunk already loaded, ignoring it");
        return;
    }
    Chunk& chunk = chunks[pos];
    for(Uns i = 0; i < CHK_VOL; ++i) {
        chunk.voxels[i]     = net_order(net_chunk.voxels[i]);
        chunk.light_lvls[i] = net_order(net_chunk.light_lvls[i]);
    }
}
