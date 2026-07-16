#version 460

/* deko3d blit fragment shader for HW libretro path.
 * Samples from the core's display texture at interpolated UV
 * and writes the color directly (no blend, no modulation). */

layout (location = 0) in vec2 vUV;

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = texture(tex, vUV);
}
