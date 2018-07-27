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

void Camera::move_x(bool positive)
{
    pos += glm::cross(dir, up) * move_speed * ((positive == 0) ? -1.f : 1.f);
}

void Camera::move_y(bool positive)
{
    pos += up * move_speed * ((positive == 0) ? -1.f : 1.f);
}
void Camera::move_z(bool positive)
{
    pos -= dir * move_speed * ((positive == 0) ? -1.f : 1.f);
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
