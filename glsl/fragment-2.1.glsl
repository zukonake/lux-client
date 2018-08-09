#version 120
#define ENABLE_NOISE

varying vec2 f_tex_pos;
varying vec4 f_col;

uniform sampler2D tileset;

float random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

void main()
{
#ifdef ENABLE_NOISE
    vec4 pre_dark = texture2D(tileset, f_tex_pos) * f_col;
    gl_FragColor = pre_dark * min(0.8, random(gl_FragCoord.xy * pre_dark.xy));
#else
    gl_FragColor = texture2D(tileset, f_tex_pos) * f_col;
#endif
} 
