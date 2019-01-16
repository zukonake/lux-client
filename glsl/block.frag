in vec3  f_pos;
in vec2  f_tex_pos;
in vec3  f_norm;

uniform vec3      ambient_light;
uniform sampler2D tileset;

void main()
{
    vec3 texel = vec3(texture2D(tileset, f_tex_pos));
    gl_FragColor =
        vec4(texel * ambient_light * ((f_norm.z + 1.0) / 3.0 + 0.3), 0.0);
} 
