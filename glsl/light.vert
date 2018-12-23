in vec2 pos;
in vec2 lvl;

out vec3 f_light;

uniform vec2 scale;
uniform vec2 chk_pos;
uniform vec2 camera_pos;
uniform vec3 ambient_light;

void main()
{
    gl_Position = vec4((pos + vec2(0.5, 0.5) + chk_pos + camera_pos) *
                       scale, 0.0, 1.0);
    f_light = mix(ambient_light * lvl.y, vec3(1.0), lvl.x);
}
