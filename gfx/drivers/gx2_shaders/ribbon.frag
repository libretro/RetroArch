#version 150
layout(location = 0) in vec4 vEC;
layout(location = 0) out vec4 FragColor;

vec4 ribbon_fragment(vec4 v) {
   float c;
   if (v.z > 1.5) {
      float s = max(0.0, 1.0-dot(v.xy,v.xy));
      c = 0.12*s*s + 1.8*pow(s,10.0);
   } else {
      float rim = max(0.0,1.0-abs(v.x));
      float fade = 1.0-smoothstep(0.65,1.0,abs(v.z));
      c = (0.018+1.8*pow(rim,6.0))*fade*smoothstep(0.14,0.34,abs(v.y));
   }
   return vec4(c*v.w, c*v.w, c*v.w, 0.0);
}


void main() { FragColor = ribbon_fragment(vEC); }
