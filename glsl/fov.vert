IN vec2 pos;

uniform vec2 scale;
uniform vec2 translation;

void main()
{
    gl_Position = vec4((pos + translation) * scale, 0.5, 1.0);
}
