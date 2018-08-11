#pragma once

#include <glm/glm.hpp>
//
#include <lux/alias/scalar.hpp>

namespace render
{

//TODO for some reason functions that use rotations do uninitialized jumps,
// tested without -ffast-math as well
class Camera
{
    public:
    Camera();

    glm::vec2 get_rotation() const;
    glm::mat4 get_view() const;
    glm::vec3 get_pos() const;

    void teleport(glm::vec3 new_pos);
    void rotate(glm::vec2 change);
    private:
    glm::vec2 rotation;
    glm::vec3 pos;
    glm::vec3 dir;
    glm::vec3 up;

    glm::vec2 rotate_speed;
};

}
