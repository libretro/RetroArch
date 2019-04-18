#version 150

uniform UBO
{
   mat4 MVP;
   vec2 OutputSize;
   float time;
} global;

layout(location = 0) out vec4 FragColor;

void main(void)
{

      float speed = global.time * 4.0;
      vec2 uv = -1.0 + 2.0 * gl_FragCoord.xy / global.OutputSize;
      uv.x *= global.OutputSize.x / global.OutputSize.y;
    vec3 color = vec3(0.0);

for( int i=0; i < 6; i++ )
      {
         float pha = sin(float(i) * 546.13 + 1.0) * 0.5 + 0.5;
         float siz = pow(sin(float(i) * 651.74 + 5.0) * 0.5 + 0.5, 4.0);
         float pox = sin(float(i) * 321.55 + 4.1) * global.OutputSize.x / global.OutputSize.y;
         float rad = 0.1 + 0.5 * siz + sin(pha + siz) / 4.0;
         vec2  pos = vec2(pox + sin(speed / 15. + pha + siz), - 1.0 - rad + (2.0 + 2.0 * rad) * fract(pha + 0.3 * (speed / 7.) * (0.2 + 0.8 * siz)));
         float dis = length(uv - pos);
    if(dis < rad)
         {
            vec3  col = mix(vec3(0.194 * sin(speed / 6.0) + 0.3, 0.2, 0.3 * pha), vec3(1.1 * sin(speed / 9.0) + 0.3, 0.2 * pha, 0.4), 0.5 + 0.5 * sin(float(i)));
            color +=  col.zyx * (1.0 - smoothstep(rad * 0.15, rad, dis));
         }
      }
      color *= sqrt(1.5 - 0.5 * length(uv));
   FragColor = vec4(color.r, color.g, color.b , 0.5);
}
