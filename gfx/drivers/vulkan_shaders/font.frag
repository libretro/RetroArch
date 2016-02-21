#version 310 es
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform highp sampler2D uTex;

void main()
{
   // Workaround for early drivers which fail to apply swizzle in VkImageView.
   FragColor = vec4(vColor.rgb, vColor.a * texture(uTex, vTexCoord).x);
}
