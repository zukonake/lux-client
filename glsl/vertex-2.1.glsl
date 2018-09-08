#version 120

attribute vec3 pos;
attribute vec2 tex_pos;
attribute vec3 col;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 world;
uniform mat4 wvp;
uniform vec2 tex_size;

varying vec2 f_tex_pos;
varying vec3 f_col;

void main()
{
    gl_Position = wvp * vec4(pos, 1.0);
    f_tex_pos = tex_pos * tex_size; //TODO precalculate (geometry shader?)?
    f_col = col;
}
