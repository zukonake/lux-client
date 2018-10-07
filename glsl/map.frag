in vec2 f_tex_pos;

uniform sampler2D map;

void main()
{
    gl_FragColor = texture2D(map, f_tex_pos);
} 
