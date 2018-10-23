#if   __VERSION__ == 100
attribute vec2 pos;
attribute vec3 col;

varying vec3 f_col;
#elif __VERSION__ == 330
in vec2 pos;
in vec3 col;

out vec3 f_col;
#endif

uniform vec2 scale;
uniform vec2 translation;

void main()
{
    gl_Position = vec4((pos + vec2(0.5, 0.5)) * scale + translation, 0.0, 1.0);
    f_col = col;
}
