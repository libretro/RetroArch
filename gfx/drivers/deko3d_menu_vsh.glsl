#version 460

/* RetroArch gfx_display ctx vertex shader for deko3d. Consumes the
 * interleaved (pos, uv, color) stream produced by gfx_display_dk3d_draw,
 * applies an orthographic MVP, and forwards uv + color to the fragment
 * stage. Used for all non-shader-pipeline menu draws (rects, icons,
 * font glyphs when routed through the display ctx). */

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec4 inColor;

layout (binding = 0, std140) uniform Transform {
    mat4 mvp;
} u;

layout (location = 0) out vec2 vUV;
layout (location = 1) out vec4 vColor;

void main()
{
    gl_Position = u.mvp * vec4(inPos, 0.0, 1.0);
    vUV    = inUV;
    vColor = inColor;
}
