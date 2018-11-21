IN vec2 pos;
IN vec4 bg_col;

OUT vec4 f_bg_col;

void main()
{
    gl_Position = vec4(pos, 0.0, 1.0);

    f_bg_col  = bg_col;
}
