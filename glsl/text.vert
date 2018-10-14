#if   __VERSION__ == 100
attribute vec2 pos;
attribute vec2 font_pos;
attribute vec3 fg_col;
attribute vec3 bg_col;

varying vec2 f_font_pos;
varying vec4 f_fg_col;
varying vec4 f_bg_col;
#elif __VERSION__ == 330
in vec2 pos;
in vec2 font_pos;
in vec3 fg_col;
in vec3 bg_col;

out vec2 f_font_pos;
out vec4 f_fg_col;
out vec4 f_bg_col;
#endif

const vec3 TRANSPARENT_COL = vec3(0.0, 0.0, 0.0);

uniform mat4 transform;
uniform vec2 font_pos_scale;

void main()
{
    gl_Position = transform * vec4(pos, 0.0, 1.0);
    f_font_pos  = font_pos * font_pos_scale;

    f_fg_col = vec4(fg_col, fg_col != TRANSPARENT_COL);
    f_bg_col = vec4(bg_col, bg_col != TRANSPARENT_COL);
}
