#version 150

uniform UBO
{
   mat4 MVP;
   vec2 OutputSize;
   float time;
   float ribbon_time;
   float ribbon_alpha;
} global;

layout(location = 0) in vec2 VertexCoord;
layout(location = 0) out vec4 vEC;

void main()
{
   float x = VertexCoord.x;
   float z = VertexCoord.y;
   float t = global.ribbon_time;
   float a = x*1.45+t*0.08;
   float b = z*1.75+x*(1.2+0.45*z)-t*0.06;
   float c = z*(5.8+1.2*z)+x*0.75-t*0.09;
   float d = z*10.0+x*(2.2-0.6*z)+t*0.075;
   float width = 0.13+0.05*cos(a);
   float y = -0.28+0.12*sin(a)+width*cos(b)*cos(c)+0.04*sin(d)+0.06*z;
   float dx = 0.174*cos(a)-0.0725*sin(a)*cos(b)*cos(c)
         - width*((1.2+0.45*z)*sin(b)*cos(c)+0.75*cos(b)*sin(c))
         + 0.04*(2.2-0.6*z)*cos(d);
   float dz = -width*((1.75+0.45*x)*sin(b)*cos(c)
         + (5.8+2.4*z)*cos(b)*sin(c))
         + 0.04*(10.0-0.6*x)*cos(d)+0.06;
   float w = 1.0+0.10*z;
   vec3 normal = vec3(dx,-1.12,1.12*dz);
   vec3 eye = vec3(-0.112*x,-0.10*y,-w);
   gl_Position = vec4(1.12*x,y,0.0,w);
   vEC = vec4(dot(normal,eye)*inversesqrt(dot(normal,normal)*dot(eye,eye)),
         (dz*w-0.10*y)/(w*w), z, (0.65+0.35*cos(z*28.0))*global.ribbon_alpha);
}
