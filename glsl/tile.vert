#if   __VERSION__ == 100
    #define IN  attribute
    #define OUT varying
#elif __VERSION__ == 330
    #define IN  in
    #define OUT out
#endif
IN vec2 pos;
IN vec3 light;
IN vec2 floor_tex;
IN vec2 wall_tex;
IN vec2 roof_tex;

OUT vec3  f_light;
OUT vec2  f_floor_tex;
OUT vec2  f_wall_tex;
OUT vec2  f_roof_tex;
OUT float f_roof_alpha;

uniform float roof_reveal_rad;
uniform vec2 cursor_pos;
uniform vec2 scale;
uniform vec2 chk_pos;
uniform vec2 camera_pos;
uniform vec2 tex_scale;

void main()
{
    vec2 map_pos = pos + chk_pos;
    gl_Position = vec4((map_pos + camera_pos) * scale, 0.0, 1.0);
    f_light     = light;
    //@TODO the tex_scale part should be precalculated in mesh
    f_floor_tex = floor_tex * tex_scale;
    f_wall_tex  = wall_tex * tex_scale;
    f_roof_tex  = roof_tex * tex_scale;
    f_roof_alpha = pow(1.0 - max(0.0, 1.0 - distance(cursor_pos, map_pos) /
                       roof_reveal_rad), 3);
}
