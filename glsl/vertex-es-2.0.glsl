#version 100
#define ENABLE_FOG

attribute vec3 pos;
attribute vec4 i_col;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 world;

varying vec4 col;

void main()
{
    gl_Position = projection * view * world * vec4(pos, 1.0);
#ifdef ENABLE_FOG
    float fog = 10.0 - gl_Position.z;
    col = i_col * min(1.0, max(0.0, fog));
#else
    col = i_col;
#endif
}
