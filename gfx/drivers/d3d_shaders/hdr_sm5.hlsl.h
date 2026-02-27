
#define SRC(...) #__VA_ARGS__
SRC(
   struct UBO
{
   float4x4 modelViewProj;
   float2 SourceSize;
   float2 OutputSize;
   float paper_white_nits;   /* 200.0f   */
   float max_nits;           /* 1000.0f  */
   uint subpixel_layout;     /* 0        */
   float scanlines;          /* 1.0f     */
   uint  expand_gamut;       /* 0        */
   float inverse_tonemap;
   float hdr10;
   uint  hdr_mode;          /* 0 = off, 1 = HDR10, 2 = scRGB, 3 = PQ->scRGB */
};
uniform UBO global;

struct PSInput
{
   float4 position : SV_POSITION;
   float2 texcoord : TEXCOORD0;
   float4 color : COLOR;
};
PSInput VSMain(float4 position : POSITION, float2 texcoord : TEXCOORD0, float4 color : COLOR)
{
   PSInput result;
   result.position = mul(global.modelViewProj, position);
   result.texcoord = texcoord;
   result.color = color;
   return result;
}
uniform sampler s0;
uniform Texture2D <float4> t0;


/* const definitions */
static const float kMaxNitsFor2084        = 10000.0f;
static const float kscRGBWhiteNits        = 80.0f;

static const float kPi                    = 3.1415926536f;
static const float kEuler                 = 2.718281828459f;
static const float kMax                   = 1.0f;

static const float kBeamWidth             = 0.5f;

static const uint kChannelMask           = 3;
static const uint kFirstChannelShift     = 2;
static const uint kSecondChannelShift    = 4;
static const uint kThirdChannelShift     = 6;

static const uint kRedId                 = 0;
static const uint kGreenId               = 1;
static const uint kBlueId                = 2;

static const uint kRed                   = (1 | (kRedId << kFirstChannelShift));
static const uint kGreen                 = (1 | (kGreenId << kFirstChannelShift));
static const uint kBlue                  = (1 | (kBlueId << kFirstChannelShift));
static const uint kBlack                 = 0;

static const uint kRGBX                  = ((kRed  << 0) | (kGreen << 4) | (kBlue  << 8) | (kBlack << 12));
static const uint kRBGX                  = ((kRed  << 0) | (kBlue  << 4) | (kGreen << 8) | (kBlack << 12));  
static const uint kBGRX                  = ((kBlue << 0) | (kGreen << 4) | (kRed   << 8) | (kBlack << 12)); 

static const uint k_rgba_axis            = 3;

static const uint k_rgb_mask[k_rgba_axis] = { kRGBX, kRBGX, kBGRX };

static const float k_crt_h_size           = 1.0f;
static const float k_crt_v_size           = 1.0f;
static const float k_crt_h_cent           = 0.0f;
static const float k_crt_v_cent           = 0.0f;
static const float k_crt_pin_phase        = 0.0f;
static const float k_crt_pin_amp          = 0.0f; 

static const float k_crt_bloom_strength   = 0.0f; //0.5f; 

static const float k_crt_red_vertical_convergence       = 0.0f;
static const float k_crt_green_vertical_convergence     = 0.0f;
static const float k_crt_blue_vertical_convergence      = 0.0f;
static const float k_crt_red_horizontal_convergence     = 0.0f;
static const float k_crt_green_horizontal_convergence   = 0.0f;
static const float k_crt_blue_horizontal_convergence    = 0.0f;

static const float k_crt_red_scanline_min               = 0.45f;
static const float k_crt_red_scanline_max               = 0.90f;
static const float k_crt_red_scanline_attack            = 0.60f;
static const float k_crt_green_scanline_min             = 0.45f;
static const float k_crt_green_scanline_max             = 0.90f;
static const float k_crt_green_scanline_attack          = 0.60f;
static const float k_crt_blue_scanline_min              = 0.45f;
static const float k_crt_blue_scanline_max              = 0.90f;       
static const float k_crt_blue_scanline_attack           = 0.60f;

static const float k_crt_red_beam_sharpness             = 1.30f;
static const float k_crt_red_beam_attack                = 1.00f;
static const float k_crt_green_beam_sharpness           = 1.30f;
static const float k_crt_green_beam_attack              = 1.00f;
static const float k_crt_blue_beam_sharpness            = 1.30f;
static const float k_crt_blue_beam_attack               = 1.00f;

static const float3 kRedChannel            = float3(1.0f, 0.0f, 0.0f);
static const float3 kGreenChannel          = float3(0.0f, 1.0f, 0.0f);
static const float3 kBlueChannel           = float3(0.0f, 0.0f, 1.0f);   

static const float3 kColourMask[3] = { kRedChannel, kGreenChannel, kBlueChannel };

static const float3x3 k709to2020 =
{
   { 0.6274040f, 0.3292820f, 0.0433136f },
   { 0.0690970f, 0.9195400f, 0.0113612f },
   { 0.0163916f, 0.0880132f, 0.8955950f }
};

static const float3x3 kP3to2020 =
{
   { 0.753833f,  0.198597f,  0.047570f },
   { 0.045744f,  0.941777f,  0.012479f },
   {-0.001210f,  0.017602f,  0.983609f }
};

static const float3x3 k2020toP3 = 
{
   { 1.343578f, -0.282180f, -0.061399f },
   {-0.065297f,  1.075788f, -0.010490f },
   { 0.002822f, -0.019598f,  1.016777f }
};

static const float4 kFallOffControlPoints    = float4(0.0f, 0.0f, 0.0f, 1.0f);
static const float4 kAttackControlPoints     = float4(0.0f, 1.0f, 1.0f, 1.0f);

static const float4x4 kCubicBezier = float4x4( 1.0f,  0.0f,  0.0f,  0.0f,
                                              -3.0f,  3.0f,  0.0f,  0.0f,
                                               3.0f, -6.0f,  3.0f,  0.0f,
                                              -1.0f,  3.0f, -3.0f,  1.0f );

/* Color rotation matrix to rotate Rec.709 color primaries into DCI-P3 (= k709to2020 * k2020toP3) */
static const float3x3 k709toP3 =
{
   { 0.8215873f,  0.1763479f,  0.0020641f },
   { 0.0328261f,  0.9695096f, -0.0023367f },
   { 0.0188038f,  0.0725063f,  0.9086907f }
};

/* START Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */
static const float3x3 kExpanded709to2020 =
{
   { 0.6274040,  0.3292820, 0.0433136 },
   { 0.0457456,  0.941777,  0.0124772 },
   {-0.00121055, 0.0176041, 0.983607 }
};

/* Color rotation matrix to rotate Rec.709 color primaries into expanded Rec.709 (= k709to2020 * k2020toExpanded709) */
static const float3x3 k709toExpanded709 =
{
   { 1.0000025f, -0.0000016f, -0.0000001f },
   { 0.0399515f,  0.9624604f, -0.0024178f },
   { 0.0228872f,  0.0684669f,  0.9086437f }
};

float3 LinearToST2084(float3 normalizedLinearValue)
{
   float3 ST2084 = pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
   return ST2084;  /* Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits */
}

float3 ST2084ToLinear(float3 ST2084)
{
   float3 Np  = pow(abs(ST2084), 1.0f / 78.84375f);
   float3 num = max(Np - 0.8359375f, 0.0f);
   float3 den = 18.8515625f - 18.6875f * Np;
   return pow(num / den, 1.0f / 0.1593017578f);
}

/* Color rotation matrix to rotate Rec.2020 color primaries into Rec.709 */
static const float3x3 k2020to709 =
{
   {  1.6604910f, -0.5876411f, -0.0728499f },
   { -0.1245505f,  1.1328999f, -0.0083494f },
   { -0.0181508f, -0.1005789f,  1.1187297f }
};

/* Converts a non-linear HDR10 PQ value in BT.2020 to scRGB linear.
 * scRGB uses Rec.709 primaries with 1.0 = 80 nits.
 * HDR10 PQ: 1.0 normalised linear = 10,000 nits, scalar = 10000/80 = 125. */
float3 HDR10ToscRGB(float3 hdr10Color)
{
   float3 linear10k = ST2084ToLinear(hdr10Color);
   float3 linear709 = mul(k2020to709, linear10k);
   return linear709 * (kMaxNitsFor2084 / kscRGBWhiteNits);
}
/* END Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */

float3 InverseTonemap(const float3 sdr_linear, const float max_nits, const float paper_white_nits)
{
   const float input_val = max(sdr_linear.r, max(sdr_linear.g, sdr_linear.b));

   if (input_val < 0.0001f) return sdr_linear;

   const float peak_ratio = max_nits / paper_white_nits;

   const float numerator = input_val;
   const float denominator = 1.0f - input_val * (1.0f - (1.0f / peak_ratio));
   const float tonemapped_val = numerator / max(denominator, 0.0001f);

   return sdr_linear * (tonemapped_val / input_val);
}

float3 Sample(float2 texcoord)
{
   const float4 sdr = t0.Sample(s0, texcoord);

   const float3 sdr_linear = pow(abs(sdr.rgb), 2.4f);

   return sdr_linear;
}

float4 Sample(float4 colour, float2 texcoord)
{
   const float4 sdr = colour * t0.Sample(s0, texcoord);

   const float3 sdr_linear = pow(abs(sdr.rgb), 2.4f);

   return float4(sdr_linear, sdr.a);
}

/* Convert Rec.709 input to the target colour space for ExpandGamut.
 * Accurate (0): Rec.709 -> Rec.2020 (proper conversion, no boost)
 * Expanded (1): Rec.709 -> Expanded709 (slightly wider than 709)
 * Wide (2):     Rec.709 -> DCI-P3
 * Super (3):    passthrough (stays Rec.709)
 * k2020to709 is always applied after this for scRGB output — the mismatch
 * between the target space and Rec.2020 creates the colour boost. */
float3 To2020(const float3 sdr_linear)
{
   float3 result;

   if(global.expand_gamut == 0)
   {
      result = mul(k709to2020, sdr_linear);
   }
   else if(global.expand_gamut == 1)
   {
      result = mul(kExpanded709to2020, sdr_linear);
   }
   else if(global.expand_gamut == 2)
   {
      result = mul(kP3to2020, sdr_linear);
   }
   else
   {
      result = sdr_linear;
   }

   result = max(result, float3(0.0f, 0.0f, 0.0f));

   return result;
}

float4 To2020(const float4 sdr_linear)
{
   float3 result;

   if(global.expand_gamut == 0)
   {
      result = mul(k709to2020, sdr_linear.rgb);
   }
   else if(global.expand_gamut == 1)
   {
      result = mul(kExpanded709to2020, sdr_linear.rgb);
   }
   else if(global.expand_gamut == 2)
   {
      result = mul(kP3to2020, sdr_linear.rgb);
   }
   else
   {
      result = sdr_linear.rgb;
   }

   result = max(result, float3(0.0f, 0.0f, 0.0f));

   return float4(result, sdr_linear.a);
}

float3 HDR(const float3 sdr_linear)
{
   return InverseTonemap(sdr_linear, global.max_nits, global.paper_white_nits);
}

float4 HDR(const float4 sdr_linear)
{
   const float3 hdr_linear = InverseTonemap(sdr_linear.rgb, global.max_nits, global.paper_white_nits);

   return float4(hdr_linear, sdr_linear.a);
}

float3 LinearToSignal(const float3 linear_colour)
{
    // Always Encode to Gamma 2.4
    return pow(max(linear_colour.rgb, float3(0.0f, 0.0f, 0.0f)), 1.0f / 2.4f);
}

float3 HDR10(const float3 hdr_linear)
{
   const float3 pq_input  = hdr_linear * (global.paper_white_nits / kMaxNitsFor2084);
         
   const float3 hdr10 = LinearToST2084(max(pq_input, 0.0f));

   return hdr10;
}

float4 HDR10(const float4 hdr_linear)
{
   const float3 pq_input  = hdr_linear.rgb * (global.paper_white_nits / kMaxNitsFor2084);
         
   const float3 hdr10 = LinearToST2084(max(pq_input, 0.0f));

   return float4(hdr10, hdr_linear.a);
}

float Bezier(const float t0, const float4 control_points)
{
   float4 t = float4(1.0, t0, t0*t0, t0*t0*t0);
   return dot(t, mul(kCubicBezier, control_points));
}

float4 BeamControlPoints(const float beam_attack, const bool falloff)
{
   const float inner_attack = clamp(beam_attack, 0.0f, 1.0);
   const float outer_attack = clamp(beam_attack - 1.0f, 0.0f, 1.0);

   return falloff ? kFallOffControlPoints + float4(0.0f, outer_attack, inner_attack, 0.0f) : kAttackControlPoints - float4(0.0f, inner_attack, outer_attack, 0.0f);
}

/* Scanline generation in linear Rec.709 space.
 * Works on all 3 channels at once (returns float3) so that pure Rec.709
 * primaries have only one non-zero channel — beam width differences
 * between channels cannot shift chromaticity. */
float3 ScanlineColour(const float2 tex_coord,
                  const float2 source_size,
                  const float scanline_size,
                  const float source_tex_coord_x,
                  const float narrowed_source_pixel_offset,
                  const float vertical_convergence,
                  const float beam_attack,
                  const float scanline_min,
                  const float scanline_max,
                  const float scanline_attack,
                  const float vertical_bias)
{
   const float current_source_position_y  = ((tex_coord.y * source_size.y) - vertical_convergence);

   const float center_line                = floor(current_source_position_y) + 0.5f + vertical_bias;

   const float distance_to_line           = current_source_position_y - center_line;

   if (abs(distance_to_line) > 1.5f) return float3(0.0f, 0.0f, 0.0f);

   const float source_tex_coord_y         = center_line / source_size.y;

   const float2 tex_coord_0               = float2(source_tex_coord_x, source_tex_coord_y);
   const float2 tex_coord_1               = float2(source_tex_coord_x + (1.0f / source_size.x), source_tex_coord_y);

   /* Sample in linear Rec.709 — beam width driven by linear signal level */
   const float3 signal_0                  = Sample(tex_coord_0);
   const float3 signal_1                  = Sample(tex_coord_1);

   float3 result = float3(0.0f, 0.0f, 0.0f);

   [unroll]
   for(int ch = 0; ch < 3; ch++)
   {
      const float horiz_interp               = Bezier(narrowed_source_pixel_offset, BeamControlPoints(beam_attack, signal_0[ch] > signal_1[ch]));
      const float signal_channel             = lerp(signal_0[ch], signal_1[ch], horiz_interp);

      const float signal_strength            = clamp(signal_channel, 0.0f, 1.0f);

      const float beam_width_adjustment      = (kBeamWidth / scanline_size);
      const float raw_distance               = abs(distance_to_line) - beam_width_adjustment;
      const float distance_adjusted          = max(0.0f, raw_distance);
      const float effective_distance         = distance_adjusted * 2.0f;

      float beam_width                       = lerp(scanline_min, scanline_max, signal_strength);

      const float channel_scanline_distance  = clamp(effective_distance / beam_width, 0.0f, 1.0f);

      const float4 channel_control_points    = float4(1.0f, 1.0f, signal_strength * scanline_attack, 0.0f);
      const float luminance                  = Bezier(channel_scanline_distance, channel_control_points);

      result[ch] = luminance * signal_channel;
   }

   return result;
}

float3 GenerateScanline( const float2 tex_coord,
                         const float2 source_size,
                         const float scanline_size,
                         const float horizontal_convergence,
                         const float vertical_convergence,
                         const float beam_sharpness,
                         const float beam_attack,
                         const float scanline_min,
                         const float scanline_max,
                         const float scanline_attack)
{
   const float current_source_position_x      = (tex_coord.x * source_size.x) - horizontal_convergence;
   const float current_source_center_x        = floor(current_source_position_x) + 0.5f;
   const float source_tex_coord_x             = current_source_center_x / source_size.x;
   const float source_pixel_offset            = frac(current_source_position_x);
   const float narrowed_source_pixel_offset   = clamp(((source_pixel_offset - 0.5f) * beam_sharpness) + 0.5f, 0.0f, 1.0f);

   float3 total_light = ScanlineColour( tex_coord, source_size, scanline_size, source_tex_coord_x,
                                        narrowed_source_pixel_offset, vertical_convergence, beam_attack,
                                        scanline_min, scanline_max, scanline_attack,
                                        0.0f);

   if(k_crt_bloom_strength > 0.0f)
   {
      total_light += ScanlineColour( tex_coord, source_size, scanline_size, source_tex_coord_x,
                                     narrowed_source_pixel_offset, vertical_convergence, beam_attack,
                                     scanline_min, scanline_max, scanline_attack,
                                     1.0f);

      total_light += ScanlineColour( tex_coord, source_size, scanline_size, source_tex_coord_x,
                                     narrowed_source_pixel_offset, vertical_convergence, beam_attack,
                                     scanline_min, scanline_max, scanline_attack,
                                     -1.0f);
   }

   return total_light;
}

/* Scanlines: generate scanline in linear Rec.709, then convert to output space and apply mask.
 * HDR10 path: mask in Rec.2020 (output is BT.2020)
 * scRGB path: mask in Rec.709 (output is scRGB/Rec.709)
 * Returns fully processed linear colour with mask applied. */
float3 Scanlines(float2 texcoord)
{
   const float2 source_size         = global.SourceSize;
   const float2 output_size         = global.OutputSize;

   float2 scanline_coord            = texcoord - float2(0.5f, 0.5f);
   scanline_coord                   = scanline_coord * float2(1.0f + (k_crt_pin_phase * scanline_coord.y), 1.0f);
   scanline_coord                   = scanline_coord * float2(k_crt_h_size, k_crt_v_size);
   scanline_coord                   = scanline_coord + float2(0.5f, 0.5f);
   scanline_coord                   = scanline_coord + (float2(k_crt_h_cent, k_crt_v_cent) / output_size);

   const float2 current_position    = texcoord * output_size;

   uint mask_idx = uint(floor(fmod(current_position.x, 4.0f)));
   uint colour_mask = (k_rgb_mask[global.subpixel_layout] >> (mask_idx * 4)) & 0xF;

   const float scanline_size              = output_size.y / source_size.y;

   /* Single GenerateScanline call — all 3 channels at once in linear Rec.709 */
   float3 scanline_colour = GenerateScanline( scanline_coord,
                                              source_size.xy,
                                              scanline_size,
                                              k_crt_red_horizontal_convergence,
                                              k_crt_red_vertical_convergence,
                                              k_crt_red_beam_sharpness,
                                              k_crt_red_beam_attack,
                                              k_crt_red_scanline_min,
                                              k_crt_red_scanline_max,
                                              k_crt_red_scanline_attack);

   /* Already linear Rec.709 — just clamp negatives */
   float3 linear_709 = max(scanline_colour, float3(0.0f, 0.0f, 0.0f));

   /* Build mask from subpixel layout bitfield */
   const uint channel_count = colour_mask & 3;
   float3 mask = float3(0.0f, 0.0f, 0.0f);
   if(channel_count > 0)
   {
      const uint ch0 = (colour_mask >> kFirstChannelShift) & 3;
      mask = kColourMask[ch0];
   }

   if(global.hdr_mode == 2)
   {
      /* scRGB: convert to Rec.2020 with ExpandGamut, then back to Rec.709 for scRGB,
       * apply mask in Rec.709 (the output colour space) */
      float3 linear_2020 = To2020(linear_709);
      float3 linear_scrgb = mul(k2020to709, linear_2020);
      return linear_scrgb * mask;
   }
   else
   {
      /* HDR10: To2020 → InverseTonemap → mask in Rec.2020 (the output colour space) */
      float3 linear_2020 = To2020(linear_709);
      float3 hdr_2020 = HDR(linear_2020);
      return hdr_2020 * mask;
   }
}

float4 PSMain(PSInput input) : SV_TARGET
{
   if(global.hdr_mode == 3)
   {
      /* PQ HDR10 to scRGB conversion.
       * Source is PQ-encoded Rec.2020 from the shader's output.
       * Decode PQ to linear, convert gamut to Rec.709, scale for scRGB. */
      float4 pq = t0.Sample(s0, input.texcoord);
      return float4(HDR10ToscRGB(pq.rgb), pq.a);
   }
   else if(global.hdr_mode == 2)
   {
      /* scRGB mode: sRGB to linear for scRGB / HDR16 swapchain.
       * Scale by MaxNits / kscRGBWhiteNits because scRGB 1.0 = 80 nits. */
      if((global.scanlines > 0.0f) && (global.OutputSize.y > 240.0f * 4.0f))
      {
         /* Scanlines() returns linear Rec.709 with mask already applied in Rec.709 space.
          * scRGB units: 1.0 = 80 nits. */
         float3 linear_col = Scanlines(input.texcoord);

         return float4(linear_col * (global.paper_white_nits / kscRGBWhiteNits), 1.0f);
      }
      else
      {
         /* Convert to Rec.2020 with ExpandGamut colour boost,
          * then always back to Rec.709 for scRGB output.
          * For Super (gamut 3), To2020 passes through Rec.709 data —
          * k2020to709 then "interprets" it as Rec.2020, giving maximum boost. */
         float4 sdr = input.color * t0.Sample(s0, input.texcoord);
         float3 linear_col = To2020(pow(abs(sdr.rgb), 2.4f));

         linear_col = mul(k2020to709, linear_col);

         linear_col *= global.max_nits / kscRGBWhiteNits;
         return float4(linear_col, sdr.a);
      }
   }
   else if((global.inverse_tonemap > 0.0f) && (global.hdr10 > 0.0f))
   {
      if((global.scanlines > 0.0f) && (global.OutputSize.y > 240.0f * 4.0f))
      {
         /* Scanlines() returns linear Rec.2020 with InverseTonemap and mask applied */
         return float4(HDR10(Scanlines(input.texcoord)), 1.0f);
      }
      else
      {
         return HDR10(HDR(To2020(Sample(input.color, input.texcoord))));
      }
   }
   else if(global.inverse_tonemap > 0.0f)
   {
      return HDR(To2020(Sample(input.color, input.texcoord)));
   }
   else if(global.hdr10 > 0.0f)
   {
      return HDR10(To2020(Sample(input.color, input.texcoord)));
   }
   else
   {
      // Pure passthrough
      return t0.Sample(s0, input.texcoord);
   }
};
)
