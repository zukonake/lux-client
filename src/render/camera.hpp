#pragma once

#include <glm/glm.hpp>
//
#include <lux/alias/scalar.hpp>

namespace render
{

class Camera
{
    public:
    Camera();

    glm::mat4 get_view() const;
    glm::mat4 get_rotation() const;
    glm::vec3 get_pos() const;

    void teleport(glm::vec3 new_pos);
    void rotate(glm::vec2 change);
    private:
    glm::vec2 rotation;
    glm::vec3 pos;
    glm::vec3 dir;
    glm::vec3 up;

    F32 move_speed;
    F32 rotate_speed;
};

}
