#version 120
attribute vec3 i_pos;
attribute vec2 i_tex_pos;
attribute vec4 i_color;

varying vec2 tex_pos;
varying vec4 color;

uniform vec2 tile_scale;

void main()
{
    gl_Position = vec4(i_pos.xyz, 1.0);
    tex_pos = i_tex_pos * tile_scale;
    color   = i_color;
}
