#version 120

varying vec2 f_tex_pos;

uniform sampler2D texture;

void main()
{
    vec4 col = texture2D(texture, f_tex_pos);
    if(col.a < 0.1) discard;
    gl_FragColor = col;
} 
