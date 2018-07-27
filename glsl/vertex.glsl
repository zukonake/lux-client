#version 120
attribute vec3 pos;
attribute vec4 i_col;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 world;

varying vec4 col;

void main()
{
    gl_Position = projection * view * world * vec4(pos, 1.0);
    col = i_col;
}
