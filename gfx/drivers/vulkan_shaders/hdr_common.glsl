layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
   vec4 SourceSize;
   vec4 OutputSize;
   float PaperWhiteNits;
   float MaxNits;
   uint SubpixelLayout;
   float Scanlines;
   float ExpandGamut;
   float InverseTonemap;
   float HDR10;
} global;

/* Tonemapping: conversion from HDR to SDR (and vice-versa) */
const float kMaxNitsFor2084   = 10000.0;
const float kEpsilon          = 0.0001;

/* Rec BT.709 luma coefficients - https://en.wikipedia.org/wiki/Luma_(video) */
const vec3 k709LumaCoeff = vec3(0.2126f, 0.7152f, 0.0722f);
/* Expanded Rec BT.709 luma coefficients - obtained by linear transformation + normalization */
const vec3 kExpanded709LumaCoeff = vec3(0.215796f, 0.702694f, 0.120968f);

vec3 InverseTonemap(const vec3 sdr_linear)
{
   float input_val = max(sdr_linear.r, max(sdr_linear.g, sdr_linear.b));

   if (input_val < kEpsilon) return sdr_linear;

   float peak_ratio = global.MaxNits / global.PaperWhiteNits;

   float numerator = input_val;
   float denominator = 1.0 - input_val * (1.0 - (1.0 / peak_ratio));
   float tonemapped_val = numerator / max(denominator, kEpsilon);

   return sdr_linear * (tonemapped_val / input_val);
}

vec3 Tonemap(const vec3 hdr_linear)
{
    float input_val = max(hdr_linear.r, max(hdr_linear.g, hdr_linear.b));

    if (input_val < kEpsilon) return hdr_linear;

    float peak_ratio = global.MaxNits / global.PaperWhiteNits;
    
    float k = 1.0 - (1.0 / peak_ratio);

    return hdr_linear / (1.0 + input_val * k);
}

/* Colorspace conversions */
/* START Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */

vec3 LinearToST2084(vec3 normalizedLinearValue)
{
   vec3 ST2084 = pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), vec3(0.1593017578f))) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), vec3(0.1593017578f))), vec3(78.84375f));
   return ST2084;  /* Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits */
}

vec3 ST2084ToLinear(vec3 ST2084)
{
   vec3 normalizedLinear = pow(abs(max(pow(abs(ST2084), vec3(1.0f / 78.84375f)) - 0.8359375f, 0.0f) / (18.8515625f - 18.6875f * pow(abs(ST2084), vec3(1.0f / 78.84375f)))), vec3(1.0f / 0.1593017578f));
   return normalizedLinear;
}

/* Color rotation matrix to rotate Rec.709 color primaries into Rec.2020 */
const mat3 k709to2020 = mat3 (
   0.6274040f, 0.3292820f, 0.0433136f,
   0.0690970f, 0.9195400f, 0.0113612f,
   0.0163916f, 0.0880132f, 0.8955950f);

/* Color rotation matrix to rotate Rec.2020 color primaries into Rec.709 */
const mat3 k2020to709 = mat3 (
   1.6604910f, -0.5876411f, -0.0728499f,
   -0.1245505f, 1.1328999f, -0.0083494f,
   -0.0181508f, -0.1005789f, 1.1187297f);

/* Rotation matrix describing a custom color space which is bigger than Rec.709, but a little smaller than P3-D65.
 * This enhances colors, especially in the SDR range, by being a little more saturated. This can be used instead
 * of from709to2020.
 */
const mat3 kExpanded709to2020 = mat3 (
    0.6274040f,  0.3292820f, 0.0433136f,
    0.0457456f,  0.941777f,  0.0124772f,
   -0.00121055f, 0.0176041f, 0.983607f);

/* Rotation matrix from Rec. 2020 color primaries into the custom expanded Rec.709 colorspace described above. */
const mat3 k2020toExpanded709 = mat3 (
    1.63535f,    -0.57057f, -0.0647755f,
   -0.0794803f,   1.0898f,  -0.0103244f,
    0.00343516f, -0.020207f, 1.01677f);

/* Per spec, the max nits for ST.2084 is 10,000 nits. We need to establish what the value of 1.0f means
 * by normalizing the values using the defined nits for paper white. According to SDR specs, paper white
 * is 80 nits, but that is paper white in a cinema with a dark environment, and is perceived as grey on
 * a display in office and living room environments. This value should be tuned according to the nits
 * that the consumer perceives as white in his living room, e.g. 200 nits. As reference, PC monitors is
 * normally in the range 200-300 nits, SDR TVs 150-250 nits.
 */
vec3 NormalizeHDRSceneValue(vec3 hdrSceneValue)
{
    return hdrSceneValue * (global.PaperWhiteNits / kMaxNitsFor2084);
}

/*  Calc the value that the HDR scene has to use to output a certain brightness */
vec3 CalcHDRSceneValue(vec3 nits)
{
    return nits * kMaxNitsFor2084 / global.PaperWhiteNits;
}

/* Converts a linear HDR value in the Rec.709 colorspace to a non-linear HDR10 value in the BT. 2020 colorspace */
vec3 LinearToHDR10(vec3 hdr_linear)
{
   vec3 rec2020 = hdr_linear * k709to2020;
   if (global.ExpandGamut > 0.0f)
      rec2020   = hdr_linear * kExpanded709to2020;
   vec3 linearColour = NormalizeHDRSceneValue(rec2020);

   return LinearToST2084(max(linearColour, 0.0));
}

vec4 LinearToHDR10(vec4 hdr_linear)
{
   vec3 rec2020 = hdr_linear.rgb * k709to2020;
   if (global.ExpandGamut > 0.0f)
      rec2020   = hdr_linear.rgb * kExpanded709to2020;
   vec3 linearColour = NormalizeHDRSceneValue(rec2020);

   vec3 hdr10 = LinearToST2084(max(linearColour, 0.0));

   return vec4(hdr10, hdr_linear.a);
}

/* END Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */

/* Converts a non-linear HDR10 value in the BT. 2020 colorspace to a linear HDR value in the Rec. 709 colorspace */
vec3 HDR10ToLinear(vec3 hdr10)
{
   vec3 normalizedLinear = ST2084ToLinear(hdr10);
   vec3 rec2020 = CalcHDRSceneValue(normalizedLinear);

   vec3 hdr = rec2020 * k2020to709;
   if (global.ExpandGamut > 0.0f)
      hdr   = rec2020 * k2020toExpanded709;

   return hdr;
}
