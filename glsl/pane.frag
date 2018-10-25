#if __VERSION__ == 100
precision lowp float;

varying vec4 f_bg_col;
#elif __VERSION__ == 330
in vec4 f_bg_col;
#endif

void main()
{
//    if(f_rel_pos.x < 0.05 || f_rel_pos.x > 0.95 ||
 //      f_rel_pos.y < 0.05 || f_rel_pos.y > 0.95) {
  //      gl_FragColor = vec4(0.0, 0.0, 0.0, f_bg_col.a);
   // } else {
        gl_FragColor = f_bg_col;
    //}
}
