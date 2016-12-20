#version 310 es
precision mediump float;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform mediump subpassInputMS uSubpass0;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform mediump subpassInputMS uSubpass1;
layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = subpassLoad(uSubpass0, 1) + subpassLoad(uSubpass1, 2);
}
