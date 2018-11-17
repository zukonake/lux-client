#if __VERSION__ == 100
precision lowp float;

varying vec4 f_bg_col;
#elif __VERSION__ == 330
in vec4 f_bg_col;
#endif

void main()
{
    gl_FragColor = f_bg_col;
}
