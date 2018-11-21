IN vec2 f_font_pos;
IN vec4 f_fg_col;
IN vec4 f_bg_col;

uniform sampler2D font;

void main()
{
    float lvl = texture2D(font, f_font_pos).a;
    vec4 col = lvl * f_fg_col + (1.0 - lvl) * f_bg_col;
    gl_FragColor = col;
}
