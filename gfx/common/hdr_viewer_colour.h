#ifndef HDR_VIEWER_COLOUR_H
#define HDR_VIEWER_COLOUR_H

/* Inverse of the HDR UI compositor: scRGB to signed display gamma. */
static void hdr_viewer_colour_rows(float *rows, bool enabled,
      float menu_nits, unsigned gamut)
{
   static const float matrices[4][9] = {
      { 1.000000332f, -1.45245068e-06f, 1.52872178e-06f,
        -3.5753674e-07f, 0.9999996789f, -1.30908592e-06f,
        1.8497402e-07f, -7.259956001e-08f, 0.9999998522f },
      { 0.9855362041f, 0.008126982364f, 0.006337775049f,
        0.02526673893f, 0.9750390779f, -0.0003075527563f,
        0.01742549886f, 0.07203929281f, 0.9105344781f },
      { 0.8224622769f, 0.1775371738f, 5.934548327e-07f,
        0.03319375294f, 0.9668056215f, -1.272129238e-06f,
        0.01708250221f, 0.0723969661f, 0.9105193457f },
      { 0.627404f, 0.329282f, 0.0433136f,
        0.069097f, 0.91954f, 0.0113612f,
        0.0163916f, 0.0880132f, 0.895595f }
   };
   unsigned i, j;
   float scale = 80.0f / (menu_nits > 1.0f ? menu_nits : 1.0f);
   if (gamut > 3) gamut = 3;
   for (i = 0; i < 3; i++)
      for (j = 0; j < 3; j++)
         rows[i * 4 + j] = matrices[gamut][i * 3 + j] * scale;
   rows[3] = enabled ? 1.0f : 0.0f;
   rows[7] = 1.0f / 2.4f;
   rows[11] = 0.0f;
}

#endif
