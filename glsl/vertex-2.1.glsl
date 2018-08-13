#version 120
//#define ENABLE_FOG
//#define ENABLE_FOG_SPHERICAL

attribute vec3 pos;
attribute vec4 col;
attribute vec2 tex_pos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 world;
uniform vec2 tex_size;

varying vec2 f_tex_pos;
varying vec4 f_col;

const float PI_2 = 1.57079632679489661923;
const float FOG_DISTANCE = 80.0;

void main()
{
    gl_Position = projection * view * world * vec4(pos, 1.0);
    f_tex_pos = tex_pos * tex_size;
#ifdef ENABLE_FOG
#   ifdef ENABLE_FOG_SPHERICAL
    vec2 rad = sin((gl_Position.xy / FOG_DISTANCE + vec2(1.0)) * PI_2);
    float shape = min(rad.x, rad.y);
#   else
    const float shape = 1.0;
#   endif
    float fog = shape - min(1.0, gl_Position.z / FOG_DISTANCE);
    f_col = col * vec4(fog, fog, fog, 1.0);
#else
    f_col = col;
#endif
}
