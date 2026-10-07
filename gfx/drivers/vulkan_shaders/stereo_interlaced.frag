#version 310 es
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform highp sampler2D uTex;

/* uTex: left eye on the left half, right eye on the right. Even rows of
 * the texture come from the left half, odd rows from the right. */
void main()
{
   float row = floor(vTexCoord.y * float(textureSize(uTex, 0).y));
   vec2 uv   = vec2(vTexCoord.x * 0.5 + 0.5 * mod(row, 2.0), vTexCoord.y);
   FragColor = vec4(texture(uTex, uv).rgb, 1.0);
}
