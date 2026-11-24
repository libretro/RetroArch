/* Shared normalized YUV sampling for viewer quads and GLSL presets.
 * Convert and quantize each RGB tap before filtering, matching RGBA8.
 * ES plane samplers keep the converter's lowp precision for RGB8 rounding. */
#ifndef GL_PLANAR_SAMPLE_H
#define GL_PLANAR_SAMPLE_H
#define GL_PLANAR_SAMPLE_SOURCE \
   "\n#ifndef RARCH_P_DEFINED\n#define RARCH_P_DEFINED\n" \
   "#ifdef GL_ES\n" \
   "#if !defined(GL_FRAGMENT_PRECISION_HIGH) && !defined(VERTEX)\n" \
   "#error Planar viewer samples require highp\n#endif\n" \
   "#define RARCH_P_HP highp\n#define RARCH_P_SP lowp\n#else\n#define RARCH_P_HP\n#define RARCH_P_SP\n#endif\n" \
   "#if __VERSION__ >= 130\n#define RARCH_P_TEX texture\n" \
   "#else\n#define RARCH_P_TEX texture2D\n#endif\n" \
   "uniform RARCH_P_SP sampler2D RARCH_P_Y,RARCH_P_C0,RARCH_P_C1;\n" \
   "uniform RARCH_P_HP vec4 RARCH_P_Size,RARCH_P_Coef[2];\n" \
   "uniform RARCH_P_HP vec2 RARCH_P_Filter;\n" \
   "RARCH_P_HP vec3 RARCH_P_pixel(RARCH_P_HP vec2 p) {\n" \
   " p=clamp(p,vec2(0.0),RARCH_P_Size.xy-1.0);\n" \
   " RARCH_P_HP vec2 cc=(floor(p*0.5)+0.5)/RARCH_P_Size.zw;\n" \
   " RARCH_P_HP float y=RARCH_P_TEX(RARCH_P_Y,(p+0.5)/RARCH_P_Size.xy).r;\n" \
   " RARCH_P_HP vec4 raw=RARCH_P_TEX(RARCH_P_C0,cc);\n" \
   " RARCH_P_HP vec2 c=RARCH_P_Coef[1].z>0.5\n" \
   "    ? vec2(raw.r,mix(raw.a,raw.g,RARCH_P_Filter.y))\n" \
   "    : vec2(raw.r,RARCH_P_TEX(RARCH_P_C1,cc).r);\n" \
   " if(RARCH_P_Coef[1].w>0.5)c=c.yx;\n" \
   " c-=vec2(128.0/255.0);\n" \
   " RARCH_P_HP vec3 rgb=vec3(RARCH_P_Coef[0].x*y+RARCH_P_Coef[0].y)\n" \
   "    +vec3(RARCH_P_Coef[0].z*c.y,RARCH_P_Coef[0].w*c.x+RARCH_P_Coef[1].x*c.y,RARCH_P_Coef[1].y*c.x);\n" \
   " return floor(clamp(rgb,0.0,1.0)*255.0+0.5)/255.0;}\n" \
   "RARCH_P_HP vec4 RARCH_P_sample(RARCH_P_HP vec2 uv) {\n" \
   " RARCH_P_HP vec2 q=uv*RARCH_P_Size.xy;RARCH_P_HP vec3 rgb;\n" \
   " if(RARCH_P_Filter.x>0.5){\n" \
   "  RARCH_P_HP vec2 p=floor(q-0.5),f=fract(q-0.5);\n" \
   "  rgb=mix(mix(RARCH_P_pixel(p),RARCH_P_pixel(p+vec2(1,0)),f.x),\n" \
   "      mix(RARCH_P_pixel(p+vec2(0,1)),RARCH_P_pixel(p+vec2(1,1)),f.x),f.y);\n" \
   " }else rgb=RARCH_P_pixel(floor(q));\n" \
   " return vec4(rgb,1.0);}\n#endif\n"
#endif
