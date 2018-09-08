#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/array.hpp>
#include <lux/alias/vector.hpp>
#include <lux/alias/vec_2.hpp>
#include <lux/alias/vec_3.hpp>
#include <lux/world/map.hpp>

struct Chunk
{
    Array<VoxelId , CHK_VOLUME> voxels;
    Array<LightLvl, CHK_VOLUME> light_lvls;

    struct Mesh
    {
        typedef U32 Index;
        static constexpr GLenum INDEX_GL_TYPE = GL_UNSIGNED_INT;
        #pragma pack(push, 1)
        struct GeometryVert
        {
            MapPos pos;
            Vec2F  tex_pos;
            GeometryVert(MapPos const &pos, Vec2F const &tex_pos) :
                pos(pos), tex_pos(tex_pos) { }
        };

        struct LightningVert
        {
            Vec3F col;
            LightningVert(Vec3F const &col) :
                col(col) { }
        };
        #pragma pack(pop)

        Vector<GeometryVert> geometry_verts;
        Vector<LightningVert> lightning_verts;
        Vector<U32>  indices;

        GLuint geometry_vbo;
        GLuint lightning_vbo;
        GLuint ebo;

        bool is_generated = false;
    } mesh;
};
