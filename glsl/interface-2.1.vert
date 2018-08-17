#version 120

attribute vec2 pos;
attribute vec2 tex_pos;

uniform vec2 tex_scale;

varying vec2 f_tex_pos;

void main()
{
    gl_Position = vec4(pos, 0.0, 1.0);
    f_tex_pos = tex_pos * tex_scale;
}
