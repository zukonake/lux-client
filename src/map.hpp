#pragma once

#include <lux_opengl.hpp>
//
#include <lux_shared/map.hpp>
#include <lux_shared/net/data.hpp>

struct Chunk {
    Arr<VoxelId ,  CHK_VOL> voxels;
    Arr<LightLvl,  CHK_VOL> light_lvls;
    struct Mesh {
        typedef U32 Idx;
        struct GVert {
            MapPos pos;
            Vec2F tex_pos;
        };
        struct LVert {
            Vec3F col;
        };
        GLuint g_vbo;
        GLuint l_vbo;
        GLuint ebo;
        enum {
            NOT_BUILT,
            BUILT_NO_MESH, ///basically we "built" the mesh, but it had no verts
                           ///so we didn't create VBOs etc.
            BUILT_MESH,
        } state = NOT_BUILT;
    } mesh;
};

bool is_chunk_loaded(ChkPos const& pos);
void load_chunk(NetServerSignal::MapLoad::Chunk const& net_chunk);
Chunk const& get_chunk(ChkPos const& pos);

///assumes that chunk is loaded
void try_build_mesh(ChkPos const& pos);
