#version 460

/* deko3d blit vertex shader for HW libretro path.
 * Consumes an interleaved (pos, uv) vertex buffer with 4 vertices
 * forming a full-screen triangle strip. */

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec2 vUV;

void main()
{
    gl_Position = vec4(inPos, 0.0, 1.0);
    vUV = inUV;
}
