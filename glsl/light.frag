#if __VERSION__ == 100
precision lowp float;

varying vec3 f_col;
#elif __VERSION__ == 330
flat in vec3 f_col;
#endif

void main()
{
    gl_FragColor = vec4(f_col, 1.0);
} 
