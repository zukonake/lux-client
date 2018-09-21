#version 330 core

in vec2 pos;
in vec2 tex_pos;
in vec3 col;

out vec2 f_tex_pos;
out vec3 f_col;

uniform mat4 matrix;
uniform vec2 tex_scale;

void main()
{
    gl_Position = matrix * vec4(pos, 0.0, 1.0);
    f_tex_pos = tex_pos * tex_scale; //TODO precalculate (geometry shader?)?
    f_col = col;
}
