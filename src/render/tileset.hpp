#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/string.hpp>
#include <lux/alias/vec_2.hpp>

namespace render
{

class Tileset
{
    public:
    Tileset() = default;

    Vec2<U32> init(String const &path);

    GLuint get_id() const;
    private:

    GLuint id;
};

}
