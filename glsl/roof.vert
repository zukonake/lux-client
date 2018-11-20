IN vec2 pos;
IN vec2 tex_pos;

OUT vec2  f_tex_pos;
OUT float f_alpha;

uniform float reveal_rad;
uniform vec2 cursor_pos;
uniform vec2 scale;
uniform vec2 chk_pos;
uniform vec2 camera_pos;
uniform vec2 tex_scale;

void main()
{
    vec2 map_pos = pos / 2.0 + chk_pos;
    gl_Position  = vec4((map_pos + camera_pos) * scale, 0.0, 1.0);
    f_tex_pos    = tex_pos * tex_scale;
    //@CONSIDER revealing the area around camera
    f_alpha = pow(1.0 - max(0.0, 1.0 - distance(cursor_pos, map_pos) /
                  reveal_rad), 2.0);
}
