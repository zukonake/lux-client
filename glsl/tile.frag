IN vec2 f_tex_pos;

uniform sampler2D tileset;

void main()
{
    gl_FragColor = texture2D(tileset, f_tex_pos);
} 
