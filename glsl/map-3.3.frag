#version 330 core

out vec4 gl_FragColor;

in vec2 f_tex_pos;
in vec3 f_col;

uniform sampler2D tileset;

void main()
{
    gl_FragColor = texture2D(tileset, f_tex_pos) * vec4(f_col, 1.0);
} 
