#version 310 es

precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform highp sampler2D Source;

#include "hdr_common.glsl"

/* Default color for compatibility */
const vec4 kDefaultColor = vec4(1.0);

/* const definitions */
const float kPi                    = 3.1415926536;
const float kEuler                 = 2.718281828459;
const float kMax                   = 1.0;

const float kBeamWidth             = 0.5;

const uint kChannelMask           = 3u;
const uint kFirstChannelShift     = 2u;
const uint kSecondChannelShift    = 4u;
const uint kThirdChannelShift     = 6u;

const uint kRedId                 = 0u;
const uint kGreenId               = 1u;
const uint kBlueId                = 2u;

const uint kRed                   = (1u | (kRedId << kFirstChannelShift));
const uint kGreen                 = (1u | (kGreenId << kFirstChannelShift));
const uint kBlue                  = (1u | (kBlueId << kFirstChannelShift));
const uint kBlack                 = 0u;

const uint kRGBX                  = ((kRed  << 0u) | (kGreen << 4u) | (kBlue  << 8u) | (kBlack << 12u));
const uint kRBGX                  = ((kRed  << 0u) | (kBlue  << 4u) | (kGreen << 8u) | (kBlack << 12u));  
const uint kBGRX                  = ((kBlue << 0u) | (kGreen << 4u) | (kRed   << 8u) | (kBlack << 12u)); 

const uint k_rgba_axis            = 3u;

const uint k_rgb_mask[k_rgba_axis] = uint[]( kRGBX, kRBGX, kBGRX );

const float k_crt_h_size           = 1.0;
const float k_crt_v_size           = 1.0;
const float k_crt_h_cent           = 0.0;
const float k_crt_v_cent           = 0.0;
const float k_crt_pin_phase        = 0.0;
const float k_crt_pin_amp          = 0.0; 

const float k_crt_bloom_strength   = 0.0f; //0.5f; 

const float k_crt_red_vertical_convergence       = 0.0;
const float k_crt_green_vertical_convergence     = 0.0;
const float k_crt_blue_vertical_convergence      = 0.0;
const float k_crt_red_horizontal_convergence     = 0.0;
const float k_crt_green_horizontal_convergence   = 0.0;
const float k_crt_blue_horizontal_convergence    = 0.0;

const float k_crt_red_scanline_min               = 0.45;
const float k_crt_red_scanline_max               = 0.90;
const float k_crt_red_scanline_attack            = 0.60;
const float k_crt_green_scanline_min             = 0.45;
const float k_crt_green_scanline_max             = 0.90;
const float k_crt_green_scanline_attack          = 0.60;
const float k_crt_blue_scanline_min              = 0.45;
const float k_crt_blue_scanline_max              = 0.90;       
const float k_crt_blue_scanline_attack           = 0.60;

const float k_crt_red_beam_sharpness             = 1.30;
const float k_crt_red_beam_attack                = 1.00;
const float k_crt_green_beam_sharpness           = 1.30;
const float k_crt_green_beam_attack              = 1.00;
const float k_crt_blue_beam_sharpness            = 1.30;
const float k_crt_blue_beam_attack               = 1.00;

const vec3 kRedChannel            = vec3(1.0, 0.0, 0.0);
const vec3 kGreenChannel          = vec3(0.0, 1.0, 0.0);
const vec3 kBlueChannel           = vec3(0.0, 0.0, 1.0);   

const vec3 kColourMask[3] = vec3[]( kRedChannel, kGreenChannel, kBlueChannel );

const vec4 kFallOffControlPoints    = vec4(0.0, 0.0, 0.0, 1.0);
const vec4 kAttackControlPoints     = vec4(0.0, 1.0, 1.0, 1.0);

const mat4 kCubicBezier = mat4(
     1.0, -3.0,  3.0, -1.0,
     0.0,  3.0, -6.0,  3.0,
     0.0,  0.0,  3.0, -3.0,
     0.0,  0.0,  0.0,  1.0
);

vec3 Sample(vec2 texcoord)
{
   vec4 sdr = texture(Source, texcoord);
   vec3 sdr_linear = pow(abs(sdr.rgb), vec3(2.22));
   return sdr_linear;
}

vec4 Sample(vec4 colour, vec2 texcoord)
{
   vec4 sdr = colour * texture(Source, texcoord);
   vec3 sdr_linear = pow(abs(sdr.rgb), vec3(2.22));
   return vec4(sdr_linear, sdr.a);
}

vec3 To2020(const vec3 sdr_linear)
{
   vec3 rec2020;
   
   uint gamut = global.ExpandGamut;
   
   if(gamut == 0u)
   {
      rec2020 = sdr_linear * k709to2020;
   }
   else if(gamut == 1u)
   {
      rec2020 = sdr_linear * kExpanded709to2020;
   }
   else if(gamut == 2u)
   {
      rec2020 = sdr_linear * k709to2020;
      rec2020 = rec2020 * k2020toP3;
   }
   else
   {
      rec2020 = sdr_linear;
   }

   rec2020 = max(rec2020, vec3(0.0f));

   return rec2020;
}

vec4 To2020(const vec4 sdr_linear)
{
   vec3 rec2020;
   
   uint gamut = global.ExpandGamut;

   if(gamut == 0u)
   {
      rec2020 = sdr_linear.rgb * k709to2020;
   }
   else if(gamut == 1u)
   {
      rec2020 = sdr_linear.rgb * kExpanded709to2020;
   }
   else if(gamut == 2u)
   {
      rec2020 = sdr_linear.rgb * k709to2020;
      rec2020 = rec2020 * k2020toP3;
   }
   else
   {
      rec2020 = sdr_linear.rgb;
   }

   rec2020 = max(rec2020, vec3(0.0f));

   return vec4(rec2020, sdr_linear.a);
}

vec3 HDR(const vec3 sdr_linear)
{
   return InverseTonemap(sdr_linear);
}

vec4 HDR(const vec4 sdr_linear)
{
   vec3 hdr_linear = InverseTonemap(sdr_linear.rgb);
   return vec4(hdr_linear, sdr_linear.a);
}

vec3 LinearToSignal(const vec3 linear_colour)
{
    // Always Encode to Gamma 2.4
    return pow(max(linear_colour.rgb, vec3(0.0f)), vec3(1.0f / 2.4f));
}

vec3 HDR10(const vec3 hdr_linear)
{
   vec3 pq_input  = hdr_linear * vec3(global.PaperWhiteNits / kMaxNitsFor2084);
         
   vec3 hdr10 = LinearToST2084(max(pq_input, vec3(0.0f)));

   return hdr10;
}

vec4 HDR10(const vec4 hdr_linear)
{
   vec3 pq_input  = hdr_linear.rgb * vec3(global.PaperWhiteNits / kMaxNitsFor2084);
         
   vec3 hdr10 = LinearToST2084(max(pq_input, vec3(0.0f)));

   return vec4(hdr10, hdr_linear.a);
}

float Bezier(const float t0, const vec4 control_points)
{
   vec4 t = vec4(1.0, t0, t0*t0, t0*t0*t0);
   return dot(t, kCubicBezier * control_points);
}

vec4 BeamControlPoints(const float beam_attack, const bool falloff)
{
   float inner_attack = clamp(beam_attack, 0.0, 1.0);
   float outer_attack = clamp(beam_attack - 1.0, 0.0, 1.0);

   return falloff ? kFallOffControlPoints + vec4(0.0, outer_attack, inner_attack, 0.0) : kAttackControlPoints - vec4(0.0, inner_attack, outer_attack, 0.0);
}

float ScanlineColour(const uint channel, 
                  const vec2 tex_coord,
                  const vec2 source_size, 
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
   float current_source_position_y  = ((tex_coord.y * source_size.y) - vertical_convergence);

   float center_line                = floor(current_source_position_y) + 0.5 + vertical_bias;
   
   float distance_to_line           = current_source_position_y - center_line;
   
   if (abs(distance_to_line) > 1.5) return 0.0;

   float source_tex_coord_y         = center_line / source_size.y; 

   vec2 tex_coord_0                 = vec2(source_tex_coord_x, source_tex_coord_y);
   vec2 tex_coord_1                 = vec2(source_tex_coord_x + (1.0 / source_size.x), source_tex_coord_y);

   float hdr_channel_0              = LinearToSignal(HDR(To2020(Sample(tex_coord_0))))[channel];
   float hdr_channel_1              = LinearToSignal(HDR(To2020(Sample(tex_coord_1))))[channel];

   float horiz_interp               = Bezier(narrowed_source_pixel_offset, BeamControlPoints(beam_attack, hdr_channel_0 > hdr_channel_1));  
   float hdr_channel                = mix(hdr_channel_0, hdr_channel_1, horiz_interp);

   float physics_signal             = hdr_channel;

   float signal_strength            = clamp(physics_signal, 0.0, 2.5); 

   float beam_width_adjustment      = (kBeamWidth / scanline_size);
   float raw_distance               = abs(distance_to_line) - beam_width_adjustment;
   float distance_adjusted          = max(0.0, raw_distance);

   float effective_distance         = distance_adjusted * 2.0;

   float beam_width                 = mix(scanline_min, scanline_max, min(signal_strength, 1.0));

   if (signal_strength > 1.0)
   {
      beam_width += (signal_strength - 1.0f) * k_crt_bloom_strength; 
   }

   float channel_scanline_distance  = clamp(effective_distance / beam_width, 0.0f, 1.0f);

   vec4 channel_control_points      = vec4(1.0f, 1.0f, min(signal_strength, 1.0f) * scanline_attack, 0.0f);
   float luminance                  = Bezier(channel_scanline_distance, channel_control_points);

   return luminance * hdr_channel;
}

float GenerateScanline( const uint channel, 
                        const vec2 tex_coord,
                        const vec2 source_size, 
                        const float scanline_size, 
                        const float horizontal_convergence, 
                        const float vertical_convergence, 
                        const float beam_sharpness, 
                        const float beam_attack, 
                        const float scanline_min, 
                        const float scanline_max, 
                        const float scanline_attack)
{
   float current_source_position_x      = (tex_coord.x * source_size.x) - horizontal_convergence;
   float current_source_center_x        = floor(current_source_position_x) + 0.5; 
   float source_tex_coord_x             = current_source_center_x / source_size.x; 
   float source_pixel_offset            = fract(current_source_position_x);
   float narrowed_source_pixel_offset   = clamp(((source_pixel_offset - 0.5) * beam_sharpness) + 0.5, 0.0, 1.0);

   float total_light = ScanlineColour( channel, tex_coord, source_size, scanline_size, source_tex_coord_x, 
                                       narrowed_source_pixel_offset, vertical_convergence, beam_attack, 
                                       scanline_min, scanline_max, scanline_attack, 
                                       0.0); 

   if(k_crt_bloom_strength > 0.0f)
   {
	   total_light += ScanlineColour( channel, tex_coord, source_size, scanline_size, source_tex_coord_x, 
									 narrowed_source_pixel_offset, vertical_convergence, beam_attack, 
									 scanline_min, scanline_max, scanline_attack, 
									 1.0);

	   total_light += ScanlineColour( channel, tex_coord, source_size, scanline_size, source_tex_coord_x, 
									 narrowed_source_pixel_offset, vertical_convergence, beam_attack, 
									 scanline_min, scanline_max, scanline_attack, 
									 -1.0);
   }

   return total_light;
} 

vec3 Scanlines(vec2 texcoord)
{
   vec2 source_size         = global.SourceSize.xy;
   vec2 output_size         = global.OutputSize.xy;

   vec2 scanline_coord            = texcoord - vec2(0.5, 0.5);
   scanline_coord                 = scanline_coord * vec2(1.0 + (k_crt_pin_phase * scanline_coord.y), 1.0);
   scanline_coord                 = scanline_coord * vec2(k_crt_h_size, k_crt_v_size);
   scanline_coord                 = scanline_coord + vec2(0.5, 0.5);
   scanline_coord                 = scanline_coord + (vec2(k_crt_h_cent, k_crt_v_cent) / output_size); 

   vec2 current_position    = texcoord * output_size;

   uint mask = uint(floor(mod(current_position.x, 4.0)));
   /* Fixed the typo 0xFju -> 0xFu here */
   uint colour_mask = (k_rgb_mask[global.SubpixelLayout] >> (mask * 4u)) & 0xFu;   

   float scanline_size              = output_size.y / source_size.y;

   vec3 horizontal_convergence   = vec3(k_crt_red_horizontal_convergence, k_crt_green_horizontal_convergence, k_crt_blue_horizontal_convergence);
   vec3 vertical_convergence     = vec3(k_crt_red_vertical_convergence, k_crt_green_vertical_convergence, k_crt_blue_vertical_convergence);
   vec3 beam_sharpness           = vec3(k_crt_red_beam_sharpness, k_crt_green_beam_sharpness, k_crt_blue_beam_sharpness);
   vec3 beam_attack              = vec3(k_crt_red_beam_attack, k_crt_green_beam_attack, k_crt_blue_beam_attack);
   vec3 scanline_min             = vec3(k_crt_red_scanline_min, k_crt_green_scanline_min, k_crt_blue_scanline_min);
   vec3 scanline_max             = vec3(k_crt_red_scanline_max, k_crt_green_scanline_max, k_crt_blue_scanline_max);
   vec3 scanline_attack          = vec3(k_crt_red_scanline_attack, k_crt_green_scanline_attack, k_crt_blue_scanline_attack);

   uint channel_count            = colour_mask & 3u;

   vec3 scanline_colour = vec3(0.0, 0.0, 0.0);

   if(channel_count > 0u)
   {
      uint channel_0             = (colour_mask >> kFirstChannelShift) & 3u;

      float scanline_channel_0   = GenerateScanline(  channel_0,
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

   vec3 linear_colour = pow(max(scanline_colour, vec3(0.0f)), vec3(2.4f));

   return HDR10(linear_colour);
}

void main()
{
   if((global.InverseTonemap > 0.0) && (global.HDR10 > 0.0))
   {
      if((global.Scanlines > 0.0) && (global.OutputSize.y > 240.0 * 4.0))
      {
         FragColor = vec4(Scanlines(vTexCoord), 1.0);
      }
      else
      {
         FragColor = HDR10(HDR(To2020(Sample(kDefaultColor, vTexCoord))));
      }
   }
   else if(global.InverseTonemap > 0.0)
   {
      FragColor = HDR(To2020(Sample(kDefaultColor, vTexCoord)));
   }
   else if(global.HDR10 > 0.0)
   {
      FragColor = HDR10(To2020(Sample(kDefaultColor, vTexCoord)));
   }
   else
   {
      FragColor = Sample(kDefaultColor, vTexCoord);
   }
}