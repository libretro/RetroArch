layout(std140, set = 0, binding = 0) uniform UBO
{
	mat4 MVP;
   float contrast;         /* 2.0f;      */ 
   float paper_white_nits; /* 200.0f;    */
   float max_nits;         /* 1000.0f;   */
   float expand_gamut;     /* 1.0f;      */ 
   float inverse_tonemap;
   float hdr10;
} settings;

/* Tonemapping: conversion from HDR to SDR (and vice-versa) */
const float kMaxNitsFor2084  = 10000.0f;
const float kEpsilon         = 0.0001f;
const float kLumaChannelRatio = 0.25f;

/* Rec BT.709 luma coefficients - https://en.wikipedia.org/wiki/Luma_(video) */
const vec3 k709LumaCoeff = vec3(0.2126f, 0.7152f, 0.0722f);
/* Expanded Rec BT.709 luma coefficients - obtained by linear transformation + normalization */
const vec3 kExpanded709LumaCoeff = vec3(0.215796f, 0.702694f, 0.120968f);

vec3 InverseTonemap(vec3 sdr)
{
   /* Display Gamma - needs to be determined by calibration screen */
   sdr = pow(abs(sdr), vec3(settings.contrast / 2.2f));
   float luma = dot(sdr, k709LumaCoeff);

   /* Apply inverse tonemap to luma and color channels separately.
    * SDR values in the range 0..0.5 are mapped to the range 0..1 with a simple inverse reinhard tonemap.
    * SDR values higher than 0.5 are mapped to the range 1..infinity with an inverse reinhard scaled according to settings.
    */
   float maxValue             = (settings.max_nits / settings.paper_white_nits) + kEpsilon;
   float elbow                = maxValue / (maxValue - 1.0f);                          
   float offset               = 1.0f - ((0.5f * elbow) / (elbow - 0.5f));              

   float hdrLumaInvTonemap    = offset + ((luma * elbow) / (elbow - luma));
   float sdrLumaInvTonemap    = luma / (1.0f + kEpsilon - luma);

   float lumaInvTonemap       = (luma > 0.5f) ? hdrLumaInvTonemap : sdrLumaInvTonemap;
   vec3 perLuma               = sdr / (luma + kEpsilon) * lumaInvTonemap;

   vec3 hdrInvTonemap         = offset + ((sdr * elbow) / (elbow - sdr));         
   vec3 sdrInvTonemap         = sdr / ((1.0f + kEpsilon) - sdr);

   vec3 perChannel            = vec3(sdr.x > 0.5f ? hdrInvTonemap.x : sdrInvTonemap.x,
                                     sdr.y > 0.5f ? hdrInvTonemap.y : sdrInvTonemap.y,
                                     sdr.z > 0.5f ? hdrInvTonemap.z : sdrInvTonemap.z);

   return mix(perLuma, perChannel, vec3(kLumaChannelRatio));
}

vec3 Tonemap(vec3 hdr)
{
   float luma = dot(hdr, k709LumaCoeff);
   if (settings.expand_gamut > 0.0f)
         luma = dot(hdr, kExpanded709LumaCoeff);

   float maxValue             = (settings.max_nits / settings.paper_white_nits) + kEpsilon;
   float elbow                = maxValue / (maxValue - 1.0f);                          
   float offset               = 1.0f - ((0.5f * elbow) / (elbow - 0.5f));     

   /* Tonemap luma and individual channels separately, and combine them afterwards*/
   float hdrLumaTonemap       = elbow * (offset - luma) / (offset - luma - elbow + kEpsilon);
   float sdrLumaTonemap       = luma / (luma + 1.0f);
   float lumaTonemap          = luma >= 1.0f ? hdrLumaTonemap : sdrLumaTonemap;
   vec3 perLuma               = hdr / (luma + kEpsilon) * lumaTonemap;

   vec3 hdrTonemap            = elbow * (offset - hdr) / (offset - hdr - elbow + kEpsilon);
   vec3 sdrTonemap            = hdr / (hdr + 1.0f);
   vec3 perChannel            = vec3(hdr.x > 1.0f ? hdrTonemap.x : sdrTonemap.x,
                                     hdr.y > 1.0f ? hdrTonemap.y : sdrTonemap.y,
                                     hdr.z > 1.0f ? hdrTonemap.z : sdrTonemap.z);

   vec3 tonemap = clamp(mix(perLuma, perChannel, vec3(kLumaChannelRatio)), 0.0f, 1.0f);
   /* Display Gamma - needs to be determined by calibration screen */
   return pow(tonemap, vec3(2.2f / settings.contrast));
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
    return hdrSceneValue * settings.paper_white_nits / kMaxNitsFor2084;
}

/*  Calc the value that the HDR scene has to use to output a certain brightness */
vec3 CalcHDRSceneValue(vec3 nits)
{
    return nits * kMaxNitsFor2084 / settings.paper_white_nits;
}

/* Converts a linear HDR value in the Rec.709 colorspace to a non-linear HDR10 value in the BT. 2020 colorspace */
vec3 ConvertLinearToHDR10(vec3 hdr)
{
   vec3 rec2020 = hdr * k709to2020;
   if (settings.expand_gamut > 0.0f)
      rec2020   = hdr * kExpanded709to2020;

   vec3 linearColour = NormalizeHDRSceneValue(rec2020);
   return LinearToST2084(linearColour);
}

/* END Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */

/* Converts a non-linear HDR10 value in the BT. 2020 colorspace to a linear HDR value in the Rec. 709 colorspace */
vec3 ConvertHDR10ToLinear(vec3 hdr10)
{
   vec3 normalizedLinear = ST2084ToLinear(hdr10);
   vec3 rec2020 = CalcHDRSceneValue(normalizedLinear);

   vec3 hdr = rec2020 * k2020to709;
   if (settings.expand_gamut > 0.0f)
      hdr   = rec2020 * k2020toExpanded709;

   return hdr;
}
