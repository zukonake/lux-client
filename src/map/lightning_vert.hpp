#pragma once

#include <lux/alias/vec_3.hpp>

#pragma pack(push, 1)
struct LightningVert
{
    LightningVert(Vec3F col) :
        col(col) { }
    Vec3F  col;
};
#pragma pack(pop)
