in vec2  f_tex_pos;
in float f_col;

uniform sampler2D tileset;

void main()
{
    gl_FragColor = texture2D(tileset, f_tex_pos) * f_col;
} 
