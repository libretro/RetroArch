#ifndef MENU_RIBBON_SHADER_H
#define MENU_RIBBON_SHADER_H

#define RIBBON_VERTEX_0 \
void ribbon_vertex(vec2 p, RIBBON_PARAMS RIBBON_OUT position, RIBBON_OUT vEC) { \
   if (p.y > 1.5) { \
      int id = int(floor((p.y-2.0)*0.25)); \
      vec2 corner = vec2(p.x,(p.y-2.0-float(id)*4.0)*2.0-1.0); \
      vec4 star = Ribbon[1+id]; \
      vec2 radius = star.z*Ribbon[49].y*vec2(Ribbon[49].x,1.0); \
      position = vec4(star.xy+corner*radius,0.0,1.0); \
      vEC = vec4(corner,2.0,star.w*Ribbon[49].z); \
   } else { \
      float x = p.x; \
      float z = p.y; \
      float time = Ribbon[0].x;

#define RIBBON_VERTEX_1 \
      float a = x*1.45+time*0.08; \
      float b = z*1.75+x*(1.2+0.45*z)-time*0.06; \
      float c = z*(5.8+1.2*z)+x*0.75-time*0.09; \
      float d = z*10.0+x*(2.2-0.6*z)+time*0.075; \
      float width = 0.13+0.05*cos(a); \
      float y = -0.28+0.12*sin(a)+width*cos(b)*cos(c)+0.04*sin(d)+0.06*z; \
      float dx = 0.174*cos(a)-0.0725*sin(a)*cos(b)*cos(c) \
            - width*((1.2+0.45*z)*sin(b)*cos(c)+0.75*cos(b)*sin(c)) \
            + 0.04*(2.2-0.6*z)*cos(d);

#define RIBBON_VERTEX_2 \
      float dz = -width*((1.75+0.45*x)*sin(b)*cos(c) \
            + (5.8+2.4*z)*cos(b)*sin(c)) \
            + 0.04*(10.0-0.6*x)*cos(d)+0.06; \
      float w = 1.0+0.10*z; \
      vec3 normal = vec3(dx,-1.12,1.12*dz); \
      vec3 eye = vec3(-0.112*x,-0.10*y,-w); \
      float facing = dot(normal,eye)/sqrt(dot(normal,normal)*dot(eye,eye)); \
      float stripes = 0.65+0.35*cos(z*28.0); \
      position = vec4(1.12*x,y,0.0,w); \
      vEC = vec4(facing,(dz*w-0.10*y)/(w*w),z,stripes*Ribbon[49].z); \
   } \
}

#define RIBBON_VERTEX RIBBON_VERTEX_0 RIBBON_VERTEX_1 RIBBON_VERTEX_2

#define RIBBON_FRAGMENT_0 \
vec4 ribbon_fragment(vec4 v) { \
   float c; \
   if (v.z > 1.5) { \
      float s = max(0.0, 1.0-dot(v.xy,v.xy)); \
      c = 0.12*s*s + 1.8*pow(s,10.0); \
   } else { \
      float rim = max(0.0,1.0-abs(v.x)); \
      float fade = 1.0-smoothstep(0.65,1.0,abs(v.z)); \
      c = (0.018+1.8*pow(rim,6.0))*fade*smoothstep(0.14,0.34,abs(v.y)); \
   } \
   return vec4(c*v.w, c*v.w, c*v.w, 0.0); \
}

#define RIBBON_FRAGMENT RIBBON_FRAGMENT_0

#endif
