#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#undef GLM_ENABLE_EXPERIMENTAL
//
#include <lux/alias/vec_4.hpp>
#include <lux/math.hpp>
#include <lux/net/client/tick.hpp>
//
#include "entity_controller.hpp"

EntityController::EntityController(GLFWwindow *win) :
    IoNode(win)
{

}

void EntityController::give_ct(net::client::Tick &ct)
{
    ct.is_moving = false;
    if(glfwGetKey(IoNode::win, GLFW_KEY_A))
    {
        ct.character_dir.x = -1.0;
        ct.is_moving = true;
    }
    else if(glfwGetKey(IoNode::win, GLFW_KEY_D))
    {
        ct.character_dir.x = 1.0;
        ct.is_moving = true;
    }
    else ct.character_dir.x = 0.0;
    if(glfwGetKey(IoNode::win, GLFW_KEY_W))
    {
        ct.character_dir.y = -1.0;
        ct.is_moving = true;
    }
    else if(glfwGetKey(IoNode::win, GLFW_KEY_S))
    {
        ct.character_dir.y = 1.0;
        ct.is_moving = true;
    }
    else ct.character_dir.y = 0.0;
    if(glfwGetKey(IoNode::win, GLFW_KEY_SPACE))
    {
        ct.is_jumping = true;
    }
    else ct.is_jumping = false;
    //TODO we shouldn't read from ct here
    glm::mat4 rotation =
        glm::eulerAngleZ(ct.yaw + TAU / 4.f);
    ct.character_dir =
        Vec3F(rotation * Vec4F(ct.character_dir, 0.0, 1.0));

}
