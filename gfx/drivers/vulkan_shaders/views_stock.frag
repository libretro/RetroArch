#version 310 es
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform highp sampler2D uTex;

/* A view drawn straight out of the frame's texture: vColor carries the
 * view's first and last texel centres, so filtering stops at its edge
 * as it would at the edge of a texture of its own. */
void main()
{
   FragColor = vec4(texture(uTex,
         clamp(vTexCoord, vColor.xy, vColor.zw)).rgb, 1.0);
}
