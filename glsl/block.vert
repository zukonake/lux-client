layout(location = 0) in vec3  pos;
layout(location = 1) in float norm;
layout(location = 2) in float tex;

out vec3 f_map_pos;
out vec2 f_tex_pos;
out vec3 f_norm;

uniform vec3 chk_pos;
uniform vec2 tex_scale;
uniform mat4 mvp;

void main()
{
    vec3 map_pos = pos + chk_pos;
    gl_Position  = mvp * vec4(map_pos, 1.0);
    f_norm = vec3(0.);
    f_norm[(int(norm) & 6) >> 1] = 1.;
    if((int(norm) & 1) == 0) f_norm *= -1.;
    f_tex_pos = (vec2(tex, 0.0)) * tex_scale;
    f_map_pos = map_pos;
}
