#version 450
/* a vertex stage: nothing to rewrite */
layout(push_constant) uniform Push
{
   vec4 SourceSize;
} params;
layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
} global;
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord   = TexCoord * params.SourceSize.xy;
}
