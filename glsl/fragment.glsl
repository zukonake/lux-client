#version 120
varying vec2 tex_pos;
varying vec4 color;

uniform sampler2D tileset;

void main()
{
    gl_FragColor = texture2D(tileset, tex_pos) * color;
} 
