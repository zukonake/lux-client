IN vec3 pos;

uniform vec2 scale;
uniform vec2 translation;

void main()
{
    gl_Position = vec4((pos.xy + translation) * scale, pos.z, 1.0);
}
