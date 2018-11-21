IN vec2 pos;
IN vec4 col;

OUT vec4 f_col;

void main()
{
    gl_Position = vec4(pos, 0.0, 1.0);

    f_col = col;
}
