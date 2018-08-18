#include <algorithm>
//
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#undef GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
//
#include <lux/math.hpp>
//
#include "camera.hpp"

namespace render
{

Camera::Camera() :
    rotation(0.0, 0.0),
    pos(0.0, 0.0, 0.0),
    dir(0.0, 0.0, -1.0),
    up(0.0, 1.0, 0.0),
    rotate_speed(TAU * 0.8, TAU / 4.f)
{

}

glm::vec2 Camera::get_rotation() const
{
    return rotation;
}

glm::mat4 Camera::get_view() const
{
    return glm::lookAt(pos, pos + dir, up);
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
    rotation.y = std::clamp(rotation.y,
                            -(TAU / 4.f) + .001f, (TAU / 4.f) - .001f);
    dir.x = cos(rotation.y) * cos(rotation.x);
    dir.y = sin(rotation.y);
    dir.z = cos(rotation.y) * sin(rotation.x);
    dir = glm::normalize(dir);
}

}
