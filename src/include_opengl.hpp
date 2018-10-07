#pragma once

#include <config.hpp>
//
#if defined(LUX_GL_3_3)
#   include <glad/glad-3.3.h>
#elif defined(LUX_GLES_2_0)
#   include <glad/glad-es-2.0.h>
#endif
