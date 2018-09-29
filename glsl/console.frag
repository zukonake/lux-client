#if __VERSION__ == 100
precision lowp float;

varying vec2 f_font_pos;
varying vec4 f_fg_col;
varying vec4 f_bg_col;
#elif __VERSION__ == 330
in vec2 f_font_pos;
in vec4 f_fg_col;
in vec4 f_bg_col;
#endif

uniform sampler2D font;

const float ALPHA_THRESHOLD = 0.1;

void main()
{
    float lvl = texture2D(font, f_font_pos).a;
    vec4 col = lvl * f_fg_col + (1.0 - lvl) * f_bg_col;
    if(col.a < ALPHA_THRESHOLD) discard;
    gl_FragColor = col;
}
