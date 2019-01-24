in vec3 pos;
in vec2 tex_pos;
in vec3 norm;

out vec2 f_tex_pos;
out vec3 f_norm;
out vec3 f_map_pos;

uniform vec3 chk_pos;
uniform vec2 tex_scale;
uniform mat4 mvp;

void main()
{
    vec3 map_pos = pos + chk_pos;
    gl_Position  = mvp * vec4(map_pos, 1.0);
    f_tex_pos    = tex_pos * tex_scale;
    f_norm       = norm;
    f_map_pos    = map_pos;
}
