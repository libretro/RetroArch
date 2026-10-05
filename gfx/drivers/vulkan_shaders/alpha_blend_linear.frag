#version 310 es

precision highp float;

/* An RGBA16F texture is linear scRGB: 1.0 is 80 nits, the 709
 * primaries. Drawn into the HDR UI layer, which the composite (hdr.frag)
 * shows as To2020(v^2.4) at BrightnessNits, scRGB and HDR10 alike - the
 * menu nits for the layer. This writes the exact inverse of that, so the
 * composite shows the texture at the luminance it was graded at; above
 * menu white v is above 1.0, which the FP16 layer keeps. The composite's
 * pow(abs()) keeps no sign, so a colour outside the chosen gamut is
 * clamped to it. Params.x is the menu nits, Params.y the composite's
 * ExpandGamut. */

/* hdr_common.glsl's constants, which this must match; it is not
 * included because it declares the composite's own UBO block. */
const float kscRGBWhiteNits = 80.0;
const mat3 k709to2020 = mat3 (
   0.6274040, 0.3292820, 0.0433136,
   0.0690970, 0.9195400, 0.0113612,
   0.0163916, 0.0880132, 0.8955950);
const mat3 k2020to709 = mat3 (
   1.6604910, -0.5876411, -0.0728499,
   -0.1245505, 1.1328999, -0.0083494,
   -0.0181508, -0.1005789, 1.1187297);
const mat3 kP3to2020 = mat3 (
    0.753833,  0.198597,  0.047570,
    0.045744,  0.941777,  0.012479,
   -0.001210,  0.017602,  0.983609);
const mat3 kExpanded709to2020 = mat3 (
    0.6274040,  0.3292820, 0.0433136,
    0.0457456,  0.941777,  0.0124772,
   -0.00121055, 0.0176041, 0.983607);

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0, std140) uniform UBO
{
   mat4 MVP;
   vec4 Params;
} global;

layout(set = 0, binding = 1) uniform highp sampler2D uTex;

/* The inverse of hdr.frag's To2020 for the same ExpandGamut */
vec3 From2020(const vec3 rgb, const float gamut)
{
   if (gamut < 0.5)
      return rgb * k2020to709;
   else if (gamut < 1.5)
      return rgb * inverse(kExpanded709to2020);
   else if (gamut < 2.5)
      return rgb * inverse(kP3to2020);
   return rgb;
}

void main()
{
   vec4  s   = texture(uTex, vTexCoord);
   vec3  lin = vColor.rgb * s.rgb;
   float a   = vColor.a * s.a;
   vec3  x   = From2020((lin * (kscRGBWhiteNits
         / max(global.Params.x, 1.0))) * k709to2020, global.Params.y);
   FragColor = vec4(pow(max(x, vec3(0.0)), vec3(1.0 / 2.4)), a);
}
