#version 310 es
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform highp sampler2D uTex;

/* uTex: left eye on the left half, right eye on the right.
 * Dubois least-squares red/cyan matrices. */
void main()
{
   vec2 uv = vec2(vTexCoord.x * 0.5, vTexCoord.y);
   vec3 l  = texture(uTex, uv).rgb;
   vec3 r  = texture(uTex, uv + vec2(0.5, 0.0)).rgb;
   vec3 c  = vec3(
         dot(l, vec3( 0.456100,  0.500484,  0.176381))
       + dot(r, vec3(-0.0434706, -0.0879388, -0.00155529)),
         dot(l, vec3(-0.0400822, -0.0378246, -0.0157589))
       + dot(r, vec3( 0.378476,   0.73364,   -0.0184503)),
         dot(l, vec3(-0.0152161, -0.0205971, -0.00546856))
       + dot(r, vec3(-0.0721527, -0.112961,   1.2264)));
   FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
