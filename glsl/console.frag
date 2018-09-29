#if __VERSION__ == 100
precision lowp float;

varying vec2 f_font_pos;
varying vec3 f_col;
#elif __VERSION__ == 330
in vec2 f_font_pos;
in vec3 f_col;
#endif

uniform sampler2D font;

const float ALPHA_THRESHOLD = 0.1;

void main()
{
    vec4 texel = texture2D(font, f_font_pos);
    if(texel.a < ALPHA_THRESHOLD) discard;
    gl_FragColor = vec4(texel.rgb * f_col, texel.a);
}
