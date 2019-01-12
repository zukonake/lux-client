in vec3 pos;
in vec2 tex_pos;
in float col;

out vec2  f_tex_pos;
out float f_col;

uniform vec3 scale;
uniform vec3 chk_pos;
uniform vec3 camera_pos;
uniform vec2 tex_scale;
uniform mat4 bobo;

void main()
{
    vec3 map_pos = pos + chk_pos - camera_pos;
    gl_Position  = bobo * vec4(map_pos, 1.0);
    f_tex_pos    = tex_pos * tex_scale;
    f_col = col;
}
