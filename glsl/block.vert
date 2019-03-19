layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec3 tex_idx; //@TODO ivec

out vec3 f_map_pos;
out vec2 f_tex_pos0;
out vec2 f_tex_pos1;
out vec2 f_tex_pos2;
out vec3 f_tex_w;
out vec3 f_norm;

uniform vec3 chk_pos;
uniform vec2 tex_scale;
uniform mat4 mvp;

void main()
{
    vec3 map_pos = pos + chk_pos;
    gl_Position  = mvp * vec4(map_pos, 1.0);
    f_norm       = norm;
    f_tex_pos0   = (vec2(tex_idx.x, 0.0)) * tex_scale;
    f_tex_pos1   = (vec2(tex_idx.y, 0.0)) * tex_scale;
    f_tex_pos2   = (vec2(tex_idx.z, 0.0)) * tex_scale;
    vec3 tex_w = vec3(0.0);
    tex_w[gl_VertexID % 3] = 1.0;
    f_map_pos = map_pos;
    f_tex_w = tex_w;
}
