#version 310 es
#extension GL_GOOGLE_include_directive : require
#include "../menu_ribbon_shader.h"

layout(location = 0) in vec2 Position;
layout(location = 0) out vec4 vEC;
layout(std140, set = 0, binding = 0) uniform UBO
{
   float time;
   float yflip;
   float alpha;
   float padding;
   vec4 Ribbon[50];
} constants;

#define RIBBON_PARAMS
#define Ribbon constants.Ribbon
#define RIBBON_OUT out vec4
RIBBON_VERTEX

void main()
{
   ribbon_vertex(Position, gl_Position, vEC);
   gl_Position.y *= constants.yflip;
}
