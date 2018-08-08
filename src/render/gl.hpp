#pragma once

#include <config.h>

#if   LUX_GL_VARIANT == LUX_GL_VARIANT_2_1
#   include <glad/glad-2.1.h>
#elif LUX_GL_VARIANT == LUX_GL_VARIANT_ES_2_0
#   include <glad/glad-es-2.0.h>
#   define GLFW_INCLUDE_ES2
#else
#   error "Unsupported GL variant selected"
#endif
#include <GLFW/glfw3.h>
