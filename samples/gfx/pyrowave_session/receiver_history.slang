#version 450
// Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
// Offline receiver oracle: history, source dimensions and a visible channel permutation.
layout(std140, binding = 0) uniform UBO
{
   mat4 MVP;
   vec4 OriginalSize;
} params;
#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 uv;
void main()
{
   gl_Position = params.MVP * Position;
   uv = TexCoord;
}
#pragma stage fragment
layout(binding = 2) uniform sampler2D Source;
layout(binding = 3) uniform sampler2D OriginalHistory1;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 FragColor;
void main()
{
   vec3 color = mix(texture(Source, uv).rgb, texture(OriginalHistory1, uv).rgb, 0.5);
   float dimensions = float(abs(params.OriginalSize.x - 130.0) < 0.01 && abs(params.OriginalSize.y - 98.0) < 0.01);
   FragColor = vec4(color.brg * dimensions, 1.0);
}
