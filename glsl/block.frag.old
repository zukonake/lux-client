in vec3 f_map_pos;
in vec2 f_tex_pos;
in vec3 f_norm;

#define RENDER_BLOCK_GRID
//#define RENDER_NO_TEX
//#define RENDER_NORMAL_COLOR

uniform float     time;
uniform vec2      tex_scale;
uniform vec3      ambient_light;
uniform sampler2D tileset;

void main()
{
    const float fogdensity = .000001;

    float z = gl_FragCoord.z / gl_FragCoord.w;
    float fog = clamp(exp(-fogdensity * z * z), 0.2, 1);
    vec3 texel;
#ifndef RENDER_NO_TEX
    vec3 anorm = abs(f_norm);
    vec2 uvx = tex_scale * fract(f_map_pos.yz);
    vec2 uvy = tex_scale * fract(f_map_pos.xz);
    vec2 uvz = tex_scale * fract(f_map_pos.xy);
    texel = (vec3(texture2D(tileset, f_tex_pos + uvx)) * anorm.x +
             vec3(texture2D(tileset, f_tex_pos + uvy)) * anorm.y +
             vec3(texture2D(tileset, f_tex_pos + uvz)) * anorm.z) /
             (anorm.x + anorm.y + anorm.z);
#else
    texel = vec3(1.0);
#endif
#ifdef RENDER_NORMAL_COLOR
    texel = abs(f_norm);
#endif
    float r_n = max(dot(f_norm, normalize(vec3(1000., 2000., 3000.0) - f_map_pos)), 0.0);
    float n = r_n * 0.7 + 0.3;
    gl_FragColor = vec4(mix(ambient_light, vec3(texel * ambient_light * n), fog), 1.0);
#ifdef RENDER_BLOCK_GRID
    vec3 fr_map = fract(vec3(f_map_pos) / vec3(32.0));
    fr_map -= 0.5;
    fr_map = abs(fr_map) * 2.;
    fr_map = step(fr_map, vec3(0.95));
    fr_map *= 0.5;
    gl_FragColor *= max(fr_map.x + fr_map.y + fr_map.z, 0.5);
#endif
} 
