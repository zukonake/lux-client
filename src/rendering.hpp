#include <config.hpp>
//
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
#   include <glad/glad-2.1.h>
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
#   include <glad/glad-es-2.0.h>
#   define GLFW_INCLUDE_ES2
#endif
#include <GLFW/glfw3.h>
//
#include <lux_shared/common.hpp>

extern GLFWwindow* glfw_window;

void init_rendering(Vec2U const &window_size);
void deinit_rendering();
void check_opengl_error();
