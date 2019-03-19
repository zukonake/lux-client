layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in VS_OUT {
    vec4 line_end;
} gs_in[];

void main() {
    for(int i = 0; i < 3; ++i) {
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
        gl_Position = gs_in[i].line_end;
        EmitVertex();
        EndPrimitive();
    }
}
