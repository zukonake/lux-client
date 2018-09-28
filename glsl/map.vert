#if   __VERSION__ == 100
attribute vec2 pos;
attribute vec2 tex_pos;
attribute vec3 col;

varying vec2 f_tex_pos;
varying vec3 f_col;
#elif __VERSION__ == 330
in vec2 pos;
in vec2 tex_pos;
in vec3 col;

out vec2 f_tex_pos;
out vec3 f_col;
#endif

uniform mat4 matrix;
uniform vec2 tex_scale;

void main()
{
    gl_Position = matrix * vec4(pos, 0.0, 1.0);
    f_tex_pos = tex_pos * tex_scale; //TODO precalculate (geometry shader?)?
    f_col = col;
}
