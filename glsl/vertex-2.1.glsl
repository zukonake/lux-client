#version 120

attribute vec3 pos;
attribute vec2 tex_pos;
attribute vec3 col;

uniform mat4 model;
uniform vec2 tex_scale;

varying vec2 f_tex_pos;
varying vec3 f_col;

void main()
{
    gl_Position = model * vec4(pos, 1.0);
    f_tex_pos = tex_pos * tex_scale; //TODO precalculate (geometry shader?)?
    f_col = col;
}
