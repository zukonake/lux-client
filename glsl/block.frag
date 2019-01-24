in vec2 f_tex_pos;
in vec3 f_norm;
in vec3 f_map_pos;

uniform vec3      ambient_light;
uniform sampler2D tileset;

bool cmp(float a, float b) {
    if(abs(a - b) < 0.1) return true;
}

void main()
{
    const vec4 fogcolor = vec4(0.6, 0.8, 1.0, 1.0);
    const float fogdensity = .00003;

    float z = gl_FragCoord.z / gl_FragCoord.w;
    float fog = clamp(exp(-fogdensity * z * z), 0.2, 1);
    vec3 texel = vec3(texture2D(tileset, f_tex_pos));
    gl_FragColor =
        mix(fogcolor, vec4(texel * ambient_light * ((f_norm.z + 1.0) / 3.0 + 0.3), 0.0), fog);
#ifdef RENDER_BLOCK_GRID
    vec3 fr_map = fract(vec3(f_map_pos - 0.5));
    fr_map -= 0.5;
    fr_map = abs(fr_map) * 2.;
    fr_map = 1. - step(fr_map, vec3(0.05));
    fr_map *= 0.5;
    gl_FragColor *= max(fr_map.x + fr_map.y + fr_map.z, 0.5);
#endif
} 
