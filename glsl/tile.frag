#if   __VERSION__ == 100
    precision lowp float;
    #define IN  varying
#elif __VERSION__ == 330
    #define IN  in
#endif
IN vec3  f_light;
IN vec2  f_floor_tex;
IN vec2  f_wall_tex;
IN vec2  f_roof_tex;
IN float f_roof_alpha;

uniform sampler2D tileset;

void main()
{
    vec4 col;
    vec4 floor_col = texture2D(tileset, f_floor_tex);
    vec4 wall_col  = texture2D(tileset, f_wall_tex);
    vec4 roof_col  = texture2D(tileset, f_roof_tex);
    roof_col.a *= f_roof_alpha;
    col = vec4(mix(floor_col.rgb, wall_col.rgb, wall_col.a), 1.0);
    col *= vec4(f_light, 1.0);
    col = vec4(mix(col.rgb, roof_col.rgb, roof_col.a), 1.0);
    gl_FragColor = col;
} 
