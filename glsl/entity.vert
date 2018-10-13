#if   __VERSION__ == 100
attribute vec2 pos;
#elif __VERSION__ == 330
in vec2 pos;
#endif

uniform vec2 scale;
uniform vec2 translation;

void main()
{
    gl_Position = vec4((pos + translation) * scale, 0.0, 1.0);
}
