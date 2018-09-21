#version 100
precision lowp float;

varying vec2 f_tex_pos;
varying vec3 f_col;

uniform sampler2D tileset;

void main()
{
    gl_FragColor = texture2D(tileset, f_tex_pos) * vec4(f_col, 1.0);
} 
