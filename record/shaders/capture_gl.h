/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_CAPTURE_GL_SHADER_H
#define RECORD_CAPTURE_GL_SHADER_H
static const char *const record_capture_gl_vertex[]
      = {"#version 300 es\n"
         "void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
         "gl_Position=vec4(p*2.0-1.0,0.0,1.0);}"};
static const char *const record_capture_gl_fragment[]
      = {"#version 300 es\nprecision highp float;precision highp int;\n"
         "uniform highp sampler2D image;uniform int mode;uniform int flip_y;"
         "out vec4 color;\n",
            "vec3 pq(vec3 light){vec3 p=pow(clamp(light,0.0,1.0),"
            "vec3(2610.0/16384.0));return pow((3424.0/4096.0+(2413.0/128.0)*p)"
            "/(1.0+(2392.0/128.0)*p),vec3(2523.0/32.0));}\n",
            "void main(){ivec2 at=ivec2(gl_FragCoord.xy);"
            "if(flip_y!=0)at.y=textureSize(image,0).y-1-at.y;"
            "vec3 rgb=texelFetch(image,at,0).rgb;",
            "if(mode==2){vec3 wide=vec3(dot(rgb,vec3(.627404,.329283,.043313)),"
            "dot(rgb,vec3(.069097,.919540,.011362)),"
            "dot(rgb,vec3(.016391,.088013,.895595)));"
            "rgb=pq(wide*(80.0/10000.0));}"
            "color=vec4(clamp(rgb,0.0,1.0),1.0);}"};
#endif
