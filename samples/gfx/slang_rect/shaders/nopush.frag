#version 450
/* no push constants: a block is made for the rectangle */
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
void main()
{
   FragColor = texture(Source, vTexCoord);
}
