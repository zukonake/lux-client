IN vec2 pos;
IN vec2 tex_pos;

OUT vec2 f_tex_pos;

uniform vec2 scale;
uniform vec2 chk_pos;
uniform vec2 camera_pos;
uniform vec2 tex_scale;

void main()
{
    gl_Position = vec4((pos / 2.0 + chk_pos + camera_pos) * scale, 0.0, 1.0);
    f_tex_pos   = tex_pos * tex_scale;
}
