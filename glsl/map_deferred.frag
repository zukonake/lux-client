out vec4 o_col;

in vec2 f_tex_pos;

uniform sampler2D g_pos;
uniform sampler2D g_norm;
uniform sampler2D g_col;

void main() {

    vec3 pos  = texture(g_pos, f_tex_pos).xyz;
    vec3 norm = texture(g_norm, f_tex_pos).xyz;
    vec3 col  = texture(g_col, f_tex_pos).rgb;
    float n = max(dot(norm, normalize(vec3(1000., 2000., 3000.) - pos)), 0.0) *
        0.7 + 0.3;
    o_col = vec4(col * n, 1.0);
}
