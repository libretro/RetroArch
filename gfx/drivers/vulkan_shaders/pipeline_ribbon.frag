#version 310 es
#extension GL_GOOGLE_include_directive : require
precision highp float;
#include "../menu_ribbon_shader.h"

layout(location = 0) in vec4 vEC;
layout(location = 0) out vec4 FragColor;
RIBBON_FRAGMENT

void main()
{
   FragColor = ribbon_fragment(vEC);
}
