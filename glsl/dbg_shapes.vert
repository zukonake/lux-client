#if   __VERSION__ == 100
attribute vec2 pos;
attribute vec4 col;

varying vec4 f_col;
#elif __VERSION__ == 330
in vec2 pos;
in vec4 col;

out vec4 f_col;
#endif

void main()
{
    gl_Position = vec4(pos, 0.0, 1.0);

    f_col = col;
}
