#pragma once

#include <render/gl.hpp>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/string.hpp>
#include <lux/alias/vector.hpp>
#include <lux/alias/vec_2.hpp>

namespace render
{

class Texture
{
    public:
    Texture() = default;

    Vec2UI load(String const &path);
    void generate_mipmaps(U32 max_lvl);
    void use();

    GLuint get_id() const;
    private:
    Vector<U8> value;
    Vec2UI   size;

    GLuint id;
};

}
