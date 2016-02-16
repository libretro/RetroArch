#version 310 es
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform highp sampler2D Source;

void main()
{
   FragColor = vec4(texture(Source, vTexCoord).rgb, 1.0);
}
