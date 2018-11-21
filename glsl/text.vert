IN vec2 pos;
IN vec2 font_pos;
IN vec4 fg_col;
IN vec4 bg_col;

OUT vec2 f_font_pos;
OUT vec4 f_fg_col;
OUT vec4 f_bg_col;

uniform vec2 font_pos_scale;

void main()
{
    gl_Position = vec4(pos, 0.0, 1.0);
    f_font_pos  = font_pos * font_pos_scale;

    f_fg_col = fg_col;
    f_bg_col = bg_col;
}
