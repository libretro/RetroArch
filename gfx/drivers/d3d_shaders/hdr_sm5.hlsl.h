
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

/* START Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */
static const float3x3 kExpanded709to2020 =
{
   { 0.6274040,  0.3292820, 0.0433136 },
   { 0.0457456,  0.941777,  0.0124772 },
   {-0.00121055, 0.0176041, 0.983607 }
};

float3 LinearToST2084(float3 normalizedLinearValue)
{
   float3 ST2084 = pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
   return ST2084;  /* Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits */
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

   const float3 sdr_linear = pow(abs(sdr.rgb), 2.22f);

   return sdr_linear;
}

float4 Sample(float4 colour, float2 texcoord)
{
   const float4 sdr = colour * t0.Sample(s0, texcoord);

   const float3 sdr_linear = pow(abs(sdr.rgb), 2.22f);

   return float4(sdr_linear, sdr.a);
}

float3 To2020(const float3 sdr_linear)
{
   float3 rec2020;
   
   if(global.expand_gamut == 0)
   {
      rec2020 = mul(k709to2020, sdr_linear);
   }
   else if(global.expand_gamut == 1)
   {
      rec2020 = mul( kExpanded709to2020, sdr_linear);
   }
   else if(global.expand_gamut == 2)
   {
      rec2020 = mul(k709to2020, sdr_linear);
      rec2020 = mul(k2020toP3, rec2020);
   }
   else
   {
      rec2020 = sdr_linear;
   }

   rec2020 = max(rec2020, float3(0.0f, 0.0f, 0.0f));

   return rec2020;
}

float4 To2020(const float4 sdr_linear)
{
   float3 rec2020;
   
   if(global.expand_gamut == 0)
   {
      rec2020 = mul(k709to2020, sdr_linear.rgb);
   }
   else if(global.expand_gamut == 1)
   {
      rec2020 = mul( kExpanded709to2020, sdr_linear.rgb);
   }
   else if(global.expand_gamut == 2)
   {
      rec2020 = mul(k709to2020, sdr_linear.rgb);
      rec2020 = mul(k2020toP3, rec2020);
   }
   else
   {
      rec2020 = sdr_linear.rgb;
   }

   rec2020 = max(rec2020, float3(0.0f, 0.0f, 0.0f));

   return float4(rec2020, sdr_linear.a);
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

float ScanlineColour(const uint channel, 
                  const float2 tex_coord,
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
   
   /* if (fmod(floor(center_line), 3.0f) != 0.0f) return 0.0f; */

   const float distance_to_line           = current_source_position_y - center_line;
   
   if (abs(distance_to_line) > 1.5f) return 0.0f;

   const float source_tex_coord_y         = center_line / source_size.y; 

   const float2 tex_coord_0               = float2(source_tex_coord_x, source_tex_coord_y);
   const float2 tex_coord_1               = float2(source_tex_coord_x + (1.0f / source_size.x), source_tex_coord_y);

   const float hdr_channel_0              = LinearToSignal(HDR(To2020(Sample(tex_coord_0))))[channel];
   const float hdr_channel_1              = LinearToSignal(HDR(To2020(Sample(tex_coord_1))))[channel];

   const float horiz_interp               = Bezier(narrowed_source_pixel_offset, BeamControlPoints(beam_attack, hdr_channel_0 > hdr_channel_1));  
   const float hdr_channel                = lerp(hdr_channel_0, hdr_channel_1, horiz_interp);

   const float physics_signal             = hdr_channel;

   const float signal_strength            = clamp(physics_signal, 0.0f, 2.5f); 

   const float beam_width_adjustment      = (kBeamWidth / scanline_size);
   const float raw_distance               = abs(distance_to_line) - beam_width_adjustment;
   const float distance_adjusted          = max(0.0f, raw_distance);
   const float effective_distance         = distance_adjusted * 2.0f;

   float beam_width                       = lerp(scanline_min, scanline_max, min(signal_strength, 1.0f));

   if (signal_strength > 1.0f)
   {
      beam_width += (signal_strength - 1.0f) * k_crt_bloom_strength; 
   }

   const float channel_scanline_distance  = clamp(effective_distance / beam_width, 0.0f, 1.0f);

   const float4 channel_control_points    = float4(1.0f, 1.0f, min(signal_strength, 1.0f) * scanline_attack, 0.0f);
   const float luminance                  = Bezier(channel_scanline_distance, channel_control_points);

   return luminance * hdr_channel;
}

float GenerateScanline( const uint channel, 
                        const float2 tex_coord,
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

   float total_light  = 0.0f;

   total_light += ScanlineColour( channel, tex_coord, source_size, scanline_size, source_tex_coord_x, 
                                       narrowed_source_pixel_offset, vertical_convergence, beam_attack, 
                                       scanline_min, scanline_max, scanline_attack, 
                                       0.0f); 

   if(k_crt_bloom_strength > 0.0f)
   {
      total_light += ScanlineColour( channel, tex_coord, source_size, scanline_size, source_tex_coord_x, 
                                    narrowed_source_pixel_offset, vertical_convergence, beam_attack, 
                                    scanline_min, scanline_max, scanline_attack, 
                                    1.0f);

      total_light += ScanlineColour( channel, tex_coord, source_size, scanline_size, source_tex_coord_x, 
                                    narrowed_source_pixel_offset, vertical_convergence, beam_attack, 
                                    scanline_min, scanline_max, scanline_attack, 
                                    -1.0f);
   }

   return total_light;
} 

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

   uint mask = uint(floor(fmod(current_position.x, 4.0f)));
   uint colour_mask = (k_rgb_mask[global.subpixel_layout] >> (mask * 4)) & 0xF;   

   const float scanline_size              = output_size.y / source_size.y;

   const float3 horizontal_convergence   = float3(k_crt_red_horizontal_convergence, k_crt_green_horizontal_convergence, k_crt_blue_horizontal_convergence);
   const float3 vertical_convergence     = float3(k_crt_red_vertical_convergence, k_crt_green_vertical_convergence, k_crt_blue_vertical_convergence);
   const float3 beam_sharpness           = float3(k_crt_red_beam_sharpness, k_crt_green_beam_sharpness, k_crt_blue_beam_sharpness);
   const float3 beam_attack              = float3(k_crt_red_beam_attack, k_crt_green_beam_attack, k_crt_blue_beam_attack);
   const float3 scanline_min             = float3(k_crt_red_scanline_min, k_crt_green_scanline_min, k_crt_blue_scanline_min);
   const float3 scanline_max             = float3(k_crt_red_scanline_max, k_crt_green_scanline_max, k_crt_blue_scanline_max);
   const float3 scanline_attack          = float3(k_crt_red_scanline_attack, k_crt_green_scanline_attack, k_crt_blue_scanline_attack);

   const uint channel_count            = colour_mask & 3;

   float3 scanline_colour = float3(0.0f, 0.0f, 0.0f);

   if(channel_count > 0)
   {
      const uint channel_0             = (colour_mask >> kFirstChannelShift) & 3;

      const float scanline_channel_0   = GenerateScanline(  channel_0,
                                                            scanline_coord,
                                                            source_size.xy, 
                                                            scanline_size, 
                                                            horizontal_convergence[channel_0], 
                                                            vertical_convergence[channel_0], 
                                                            beam_sharpness[channel_0], 
                                                            beam_attack[channel_0], 
                                                            scanline_min[channel_0], 
                                                            scanline_max[channel_0], 
                                                            scanline_attack[channel_0]);

      scanline_colour =  scanline_channel_0 * kColourMask[channel_0];
   }

   float3 linear_colour = pow(max(scanline_colour, float3(0.0f, 0.0f, 0.0f)), 2.4f);

   return HDR10(linear_colour);
}

float4 PSMain(PSInput input) : SV_TARGET
{
   if((global.inverse_tonemap > 0.0f) && (global.hdr10 > 0.0f))
   {
      if((global.scanlines > 0.0f) && (global.OutputSize.y > 240.0f * 4.0f))
      {
         return float4(Scanlines(input.texcoord), 1.0f);
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
      return Sample(input.color, input.texcoord);
   }
};
)
