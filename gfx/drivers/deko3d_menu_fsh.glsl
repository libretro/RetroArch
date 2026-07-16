#version 460

/* RetroArch gfx_display ctx fragment shader for deko3d. Samples one
 * texture and modulates by the per-vertex color. The driver binds a 1x1
 * white texture when the menu code doesn't supply one, so colored-only
 * rects also flow through this shader (texture sample = 1.0). */

layout (location = 0) in vec2 vUV;
layout (location = 1) in vec4 vColor;

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = texture(tex, vUV) * vColor;
}
