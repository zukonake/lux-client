#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/string.hpp>
#include <lux/alias/vec_2.hpp>

namespace render
{

class Texture
{
    public:
    Texture() = default;

    Vec2<U32> load(String const &path);

    GLuint get_id() const;
    private:

    GLuint id;
};

}
