layout (location = 0) in vec3  pos;
layout (location = 1) in float norm;
layout (location = 2) in float tex;

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
    int n_a = (int(norm) & 6) >> 1;
    f_norm[n_a] = 1.;
    if((int(norm) & 1) == 0) f_norm *= -1.;
    vec2 uv;
    //if(n_a == 0) {
    //    uv = map_pos.yz;
    //} else if(n_a == 1) {
    //    uv = map_pos.xz;
    //} else if(n_a == 2) {
        uv = map_pos.xy;
    //}
    f_tex_pos = (vec2(tex, 0.0) + fract(uv)) * tex_scale;
    f_map_pos = map_pos;
}
