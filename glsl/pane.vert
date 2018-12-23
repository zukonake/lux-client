in vec2 pos;
in vec4 bg_col;

out vec4 f_bg_col;

void main()
{
    gl_Position = vec4(pos, 0.0, 1.0);

    f_bg_col  = bg_col;
}
