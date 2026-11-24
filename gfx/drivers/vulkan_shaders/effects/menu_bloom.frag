#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0, std140) uniform Params { vec4 p; } params;
layout(set = 0, binding = 1) uniform sampler2D source;
vec3 light(vec2 v)
{
   return max(texture(source, v).rgb - params.p.z, 0.0);
}
void main()
{
   vec2 step = params.p.xy;
   vec3 c = light(uv);
   if (dot(step, step) > 0.0)
   {
      c *= 0.227027;
      c += (light(uv + step * 1.384615) + light(uv - step * 1.384615)) * 0.316216;
      c += (light(uv + step * 3.230769) + light(uv - step * 3.230769)) * 0.0702703;
   }
   color = vec4(c * params.p.w, 0.0);
}
