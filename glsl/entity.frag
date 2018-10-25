#if __VERSION__ == 100
precision lowp float;

varying vec2 f_tex_pos;
#elif __VERSION__ == 330
in vec2 f_tex_pos;
#endif

uniform sampler2D tileset;

void main()
{
    gl_FragColor = texture2D(tileset, f_tex_pos);
    if(gl_FragColor.a < 0.1) discard;
} 
