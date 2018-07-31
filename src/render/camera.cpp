#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#undef GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
//
#include "camera.hpp"

namespace render
{

Camera::Camera() :
    rotation(0.0, 0.0),
    pos(0.0, 0.0, 0.0),
    dir(0.0, 0.0, -1.0),
    up(0.0, 1.0, 0.0),
    move_speed(0.10),
    rotate_speed(0.05)
{

}

glm::mat4 Camera::get_view() const
{
    return glm::lookAt(pos, pos + dir, up);
}

glm::mat4 Camera::get_rotation() const
{
    return glm::eulerAngleZ(glm::radians(rotation.x) + glm::half_pi<F32>());
}

glm::vec3 Camera::get_pos() const
{
    return pos;
}

void Camera::teleport(glm::vec3 new_pos)
{
    pos = new_pos;
}

void Camera::rotate(glm::vec2 change)
{
    rotation += change * rotate_speed;
         if(rotation.y >  89.f) rotation.y =  89.f;
    else if(rotation.y < -89.f) rotation.y = -89.f;
    dir.x = cos(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));
    dir.y = sin(glm::radians(rotation.y));
    dir.z = cos(glm::radians(rotation.y)) * sin(glm::radians(rotation.x));
    dir = glm::normalize(dir);
}

}
