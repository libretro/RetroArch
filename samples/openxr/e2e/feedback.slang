#version 450
layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; uint FrameCount; } global;
#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main() { gl_Position = global.MVP * Position; vTexCoord = TexCoord; }
#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D PassFeedback0;
void main()
{
   FragColor = vec4(fract(texture(PassFeedback0, vTexCoord).r + 0.03125),
         float(global.FrameCount % 251u) / 250.0, 0.25, 1.0);
}
