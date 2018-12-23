in vec2  f_tex_pos;
in float f_alpha;

uniform sampler2D tileset;
uniform vec3      ambient_light;

void main()
{
    vec4 texel = texture2D(tileset, f_tex_pos);
    texel.a   *= f_alpha;
    texel.rgb *= ambient_light;
    gl_FragColor = texel;
} 
