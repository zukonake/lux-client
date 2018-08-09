#include <stdexcept>
//
#include <lodepng.h>
//
#include <lux/alias/scalar.hpp>
#include <lux/alias/vector.hpp>
//
#include "render/tileset.hpp"

namespace render
{

Vec2<U32> Tileset::init(String const &path)
{
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    Vector<U8> image;
    Vec2<U32>  size;
    auto err = lodepng::decode(image, size.x, size.y, path);
    if(err) throw std::runtime_error("couldn't load tileset: " + path);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    // TODO ^ no mipmaps unless I find a way to generate them reliably
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return size;
}

GLuint Tileset::get_id() const
{
    return id;
}

}
