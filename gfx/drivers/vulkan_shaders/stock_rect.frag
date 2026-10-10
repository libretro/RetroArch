#version 310 es
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform highp sampler2D uTex;

/* Clamp: the rectangle's first and last texel centres, so filtering
 * stops at its edge as it would at the edge of a texture of its own. */
layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
   vec4 Clamp;
} global;

/* A rectangle of a texture drawn as the stock chain draws a texture of
 * its own: a view, or a frame in a larger texture. */
void main()
{
   FragColor = vec4(texture(uTex,
         clamp(vTexCoord, global.Clamp.xy, global.Clamp.zw)).rgb, 1.0);
}
