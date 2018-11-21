IN vec2 pos;
IN vec2 tex_pos;

OUT vec2 f_tex_pos;

uniform vec2 scale;
uniform vec2 translation;
uniform vec2 tex_scale;

//@CONSIDER merging with tile shader
void main()
{
    gl_Position = vec4((pos + translation) * scale, 0.0, 1.0);
    f_tex_pos = tex_pos * tex_scale;
}
