#if __VERSION__ == 100
precision lowp float;

varying vec2 f_tex_pos;
varying vec3 f_col;
#elif __VERSION__ == 330
in vec2 f_tex_pos;
in vec3 f_col;
#endif

uniform sampler2D tileset;

const vec3 luma_mul = vec3(0.299, 0.587, 0.114);

void main()
{
    //float luma = dot(f_col * luma_mul, vec3(1.0));
    vec3 texel = vec3(texture2D(tileset, f_tex_pos));
    gl_FragColor = vec4(texel * pow(f_col * 2.0, vec3(1.0 / 2.2)), 1.0);
} 
