layout (location = 0) out vec3 g_pos;
layout (location = 1) out vec3 g_norm;
layout (location = 2) out vec3 g_col;

in vec3 f_map_pos;
in vec2 f_tex_pos;
in vec3 f_norm;

uniform sampler2D tileset;

void main() {
    g_pos = f_map_pos;
    g_norm = f_norm;
    g_col = texture(tileset, f_tex_pos).rgb;
}
