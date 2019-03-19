layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;

out VS_OUT {
    vec4 line_end;
} vs_out;

uniform mat4 mvp;
uniform vec3 chk_pos;

void main()
{
    vec3 map_pos = chk_pos + pos;
    gl_Position = mvp * vec4(map_pos, 1.0); 
    vs_out.line_end = mvp * vec4(map_pos + norm * 0.6, 1.0);
}

