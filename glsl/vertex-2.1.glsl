#version 120

attribute vec2 pos;
attribute vec2 tex_pos;
attribute vec3 col;

uniform mat4 matrix;
uniform vec2 tex_scale;

varying vec2 f_pos;
varying vec2 f_tex_pos;
varying vec3 f_col;

void main()
{
    gl_Position = matrix * vec4(pos, 0.0, 1.0);
    f_pos = pos;
    f_tex_pos = tex_pos * tex_scale; //TODO precalculate (geometry shader?)?
    f_col = col;
}
