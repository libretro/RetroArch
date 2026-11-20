#version 310 es
#extension GL_GOOGLE_include_directive : require
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 1) uniform highp sampler2D uTex;
#define XMB_COVERAGE(p) texture(uTex, p).r
#include "../../xmb_plastic.h"

void main()
{
   vec2 uv = vTexCoord;
   vec4 tint = vColor;
   XMB_FONT_SAMPLE
   XMB_FONT_LIGHT
   XMB_FONT_COLOR
   FragColor = plastic;
}
