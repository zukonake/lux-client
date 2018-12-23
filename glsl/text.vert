in vec2 pos;
in vec2 font_pos;
in vec4 fg_col;
in vec4 bg_col;

out vec2 f_font_pos;
out vec4 f_fg_col;
out vec4 f_bg_col;

uniform vec2 font_pos_scale;

void main()
{
    gl_Position = vec4(pos, 0.0, 1.0);
    f_font_pos  = font_pos * font_pos_scale;

    f_fg_col = fg_col;
    f_bg_col = bg_col;
}
