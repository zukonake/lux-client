in vec2 pos;
in vec4 col;

out vec4 f_col;

uniform vec2 scale;
uniform vec2 translation;

void main()
{
    gl_Position = vec4((pos + translation) * scale, 0.0, 1.0);

    f_col = col;
}
