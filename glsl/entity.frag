in vec2 f_tex_pos;

uniform sampler2D tileset;

void main()
{
    gl_FragColor = texture2D(tileset, f_tex_pos);
    if(gl_FragColor.a < 0.1) discard;
} 
