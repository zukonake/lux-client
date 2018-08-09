#version 120
//#define ENABLE_FOG

attribute vec3 pos;
attribute vec4 col;
attribute vec2 tex_pos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 world;
uniform vec2 tex_size;

varying vec2 f_tex_pos;
varying vec4 f_col;

void main()
{
    gl_Position = projection * view * world * vec4(pos, 1.0);
    f_tex_pos = tex_pos * tex_size;
#ifdef ENABLE_FOG
    float fog = clamp(1.0 - (gl_Position.z / 5.0), 0.0, 1.0);
    f_col = col * vec4(fog, fog, fog, 1.0);
#else
    f_col = col;
#endif
}
