#if   __VERSION__ == 100
attribute vec2 pos;
attribute vec2 font_pos;
attribute vec3 col;

varying vec2 f_font_pos;
varying vec3 f_col;
#elif __VERSION__ == 330
in vec2 pos;
in vec2 font_pos;
in vec3 col;

out vec2 f_font_pos;
out vec3 f_col;
#endif

uniform mat4 transform;
uniform vec2 font_pos_scale;

void main()
{
    gl_Position = transform * vec4(pos, 0.0, 1.0);
    f_font_pos  = font_pos * font_pos_scale;
    f_col       = col;
}
