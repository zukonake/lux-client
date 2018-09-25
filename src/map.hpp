#pragma once

#include <include_opengl.hpp>
//
#include <lux_shared/map.hpp>
#include <lux_shared/net/data.hpp>

struct Chunk {
    Arr<VoxelId , CHK_VOL> voxels;
    Arr<LightLvl, CHK_VOL> light_lvls;
    struct Mesh {
        typedef U32 Idx;
        static constexpr GLenum INDEX_GL_TYPE = GL_UNSIGNED_INT;
#pragma pack(push, 1)
        struct GVert {
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
            Vec2<F32> pos;
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
            Vec2<I32> pos;
#endif
            Vec2<U16> tex_pos;
        };
        struct LVert {
            Vec3<U8> col;
        };
#pragma pack(pop)
        bool has_empty = false;
        bool is_built  = false;
        GLuint g_vbo;
        GLuint l_vbo;
        GLuint ebo;
#if LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
        GLuint vao;
#endif
        U32 trig_count;
    } mesh;
};

struct MapAssets {
    char const* shader_path;
    char const* tileset_path;
    Vec2U tile_size;
};

extern VecSet<ChkPos> chunk_requests;

void map_init(MapAssets assets);
void map_render(EntityVec const& player_pos);
bool is_chunk_loaded(ChkPos const& pos);
void load_chunk(NetSsSgnl::MapLoad::Chunk const& net_chunk);
void light_update(NetSsSgnl::LightUpdate::Chunk const& net_chunk);
Chunk const& get_chunk(ChkPos const& pos);
