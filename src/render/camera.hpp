#pragma once

#include <glm/glm.hpp>
//
#include <lux/alias/vec_2.hpp>
#include <lux/alias/vec_3.hpp>

namespace render
{

class Camera
{
    public:
    Camera();

    Vec2F get_rotation() const;
    glm::mat4 get_view() const;
    Vec3F get_pos() const;

    void teleport(Vec3F new_pos);
    void rotate(Vec2F change);
    private:
    Vec2F rotation;
    Vec3F pos;
    Vec3F dir;
    Vec3F up;

    Vec2F rotate_speed;
};

}
