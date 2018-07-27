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

    void move_x(bool positive);
    void move_y(bool positive);
    void move_z(bool positive);
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
