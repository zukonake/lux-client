#pragma once

#include <include_opengl.hpp>
//
#include <lux_shared/map.hpp>
#include <lux_shared/net/data.hpp>

struct Chunk {
    Arr<VoxelId ,  CHK_VOL> voxels;
    Arr<LightLvl,  CHK_VOL> light_lvls;
    struct Mesh {
        typedef U32 Idx;
        struct GVert {
            Vec2<I64> pos;
            Vec2<U16> tex_pos;
        };
        struct LVert {
            Vec3<U8> col;
        };
        bool has_empty = false;
        bool is_built  = false;
        GLuint g_vbo;
        GLuint l_vbo;
        GLuint ebo;
    } mesh;
};

struct MapAssets {
    char const* vert_path;
    char const* frag_path;
    char const* tileset_path;
    Vec2U tile_size;
};

void map_init(MapAssets assets);
void map_render();
bool is_chunk_loaded(ChkPos const& pos);
void load_chunk(NetServerSignal::MapLoad::Chunk const& net_chunk);
Chunk const& get_chunk(ChkPos const& pos);

///assumes that chunk is loaded
void try_build_mesh(ChkPos const& pos);
