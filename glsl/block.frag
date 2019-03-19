in vec3 f_map_pos;
in vec2 f_tex_pos0;
in vec2 f_tex_pos1;
in vec2 f_tex_pos2;
in vec3 f_tex_w;
in vec3 f_norm;

//#define RENDER_BLOCK_GRID
//#define RENDER_NO_TEX
//#define RENDER_NORMAL_COLOR

uniform float     time;
uniform vec2      tex_scale;
uniform vec3      ambient_light;
uniform sampler2D tileset;

void main()
{
    const vec4 fogcolor = vec4(0.6, 0.8, 1.0, 1.0);
    const float fogdensity = .00001;

    float z = gl_FragCoord.z / gl_FragCoord.w;
    float fog = clamp(exp(-fogdensity * z * z), 0.2, 1);
    vec3 texel;
#ifndef RENDER_NO_TEX
    vec3 anorm = abs(f_norm);
    vec2 uvx = tex_scale * fract(f_map_pos.yz);
    vec2 uvy = tex_scale * fract(f_map_pos.xz);
    vec2 uvz = tex_scale * fract(f_map_pos.xy);
    texel = ((vec3(texture2D(tileset, f_tex_pos0 + uvx)) * f_tex_w.x +
              vec3(texture2D(tileset, f_tex_pos1 + uvx)) * f_tex_w.y +
              vec3(texture2D(tileset, f_tex_pos2 + uvx)) * f_tex_w.z) * anorm.x +
             (vec3(texture2D(tileset, f_tex_pos0 + uvy)) * f_tex_w.x +
              vec3(texture2D(tileset, f_tex_pos1 + uvy)) * f_tex_w.y +
              vec3(texture2D(tileset, f_tex_pos2 + uvy)) * f_tex_w.z) * anorm.y +
             (vec3(texture2D(tileset, f_tex_pos0 + uvz)) * f_tex_w.x +
              vec3(texture2D(tileset, f_tex_pos1 + uvz)) * f_tex_w.y +
              vec3(texture2D(tileset, f_tex_pos2 + uvz)) * f_tex_w.z) * anorm.z) /
             (anorm.x + anorm.y + anorm.z);
#else
    texel = vec3(1.0);
#endif
#ifdef RENDER_NORMAL_COLOR
    texel = abs(f_norm);
#endif
    float r_n = max(dot(f_norm, normalize(vec3(-10.0+cos(time * 0.9) * 50.0, 10.0+sin(time * 0.9) * 50.0, 100.0) - f_map_pos)), 0.0);
    float n = r_n * 0.9 + 0.1;
    gl_FragColor =
        mix(fogcolor, vec4(texel * ambient_light * n, 0.0), fog);
#ifdef RENDER_BLOCK_GRID
    vec3 fr_map = fract(vec3(f_map_pos));
    fr_map -= 0.5;
    fr_map = abs(fr_map) * 2.;
    fr_map = step(fr_map, vec3(0.95));
    fr_map *= 0.5;
    gl_FragColor *= max(fr_map.x + fr_map.y + fr_map.z, 0.5);
#endif
} 
