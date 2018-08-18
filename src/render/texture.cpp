#include <cstdlib>
#include <stdexcept>
//
#include <lodepng.h>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/vector.hpp>
//
#include "render/texture.hpp"

namespace render
{

Vec2<U32> Texture::load(String const &path)
{
    glGenTextures(1, &id);
    use();

    auto err = lodepng::decode(value, size.x, size.y, path);
    if(err) throw std::runtime_error("couldn't load tileset: " + path);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, value.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return size;
}

void Texture::generate_mipmaps(U32 levels)
{
    assert((size.x & (size.x - 1)) == 0); //assert powers of 2
    assert((size.y & (size.y - 1)) == 0); //
    assert(size.x == size.y); //only this is supported now

    glBindTexture(GL_TEXTURE_2D, id);
    Vector<U8> mipmap_buf;
    for(U32 s = size.x / 2, i = 1; s > 0; s /= 2, ++i)
    {
        U32 mul = size.x / s;
        mipmap_buf.resize(s * s * 4);
        for(U32 p = 0; p < s * s; ++p)
        {
            std::memcpy(&mipmap_buf[p * 4],
                        &value[(((p % s) * mul) +
                                ((p / s) * size.x * mul)) * 4], 4);
        }
        glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA, s, s,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, mipmap_buf.data());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, levels - 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void Texture::use()
{
    glBindTexture(GL_TEXTURE_2D, id);
}

GLuint Texture::get_id() const
{
    return id;
}

}
