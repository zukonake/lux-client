#pragma once

#include <config.hpp>
//
#if   LUX_GL_VARIANT == LUX_GL_VARIANT_3_3
#   include <GLFW/glfw3.h>
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
#   define GLFW_INCLUDE_ES2
#   include <GLFW/glfw3.h>
#endif
