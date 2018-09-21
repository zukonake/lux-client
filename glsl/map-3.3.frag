#version 330 core

out vec4 col;

in vec2 f_tex_pos;
in vec3 f_col;

uniform sampler2D tileset;

void main()
{
    col = texture2D(tileset, f_tex_pos) * vec4(f_col, 1.0);
} 
