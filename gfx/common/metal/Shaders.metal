/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - Stuart Carnie
 *  copyright (c) 2011-2021 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* File for Metal kernel and shader functions */

#include <metal_stdlib>
#include <simd/simd.h>

/* Including header shared between this Metal shader code and Swift/C code executing Metal API commands */
#import "metal_shader_types.h"

using namespace metal;

#pragma mark - functions using projected coordinates

vertex ColorInOut basic_vertex_proj_tex(const Vertex in [[ stage_in ]],
                                        const device Uniforms &uniforms [[ buffer(BufferIndexUniforms) ]])
{
    ColorInOut out;
    out.position = uniforms.projectionMatrix * float4(in.position, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 basic_fragment_proj_tex(ColorInOut in [[stage_in]],
                                        constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
                                        texture2d<half> tex          [[ texture(TextureIndexColor) ]],
                                        sampler samp                 [[ sampler(SamplerIndexDraw) ]])
{
    half4 colorSample = tex.sample(samp, in.texCoord.xy);
    return float4(colorSample);
}

#pragma mark - functions for rendering sprites

vertex FontFragmentIn sprite_vertex(const SpriteVertex in [[ stage_in ]], const device Uniforms &uniforms [[ buffer(BufferIndexUniforms) ]])
{
    FontFragmentIn out;
    out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);
    out.texCoord = in.texCoord;
    out.color    = in.color;
    return out;
}

fragment float4 sprite_fragment_a8(FontFragmentIn  in  [[ stage_in ]],
                              texture2d<half> tex [[ texture(TextureIndexColor) ]],
                              sampler samp        [[ sampler(SamplerIndexDraw) ]])
{
    half4 colorSample = tex.sample(samp, in.texCoord.xy);
    return float4(in.color.rgb, in.color.a * colorSample.r);
}

#pragma mark - functions for rendering sprites

vertex FontFragmentIn stock_vertex(const SpriteVertex in [[ stage_in ]], const device Uniforms &uniforms [[ buffer(BufferIndexUniforms) ]])
{
    FontFragmentIn out;
    out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);
    out.texCoord = in.texCoord;
    out.color    = in.color;
    return out;
}

fragment float4 stock_fragment(FontFragmentIn  in  [[ stage_in ]],
                              texture2d<float> tex [[ texture(TextureIndexColor) ]],
                              sampler samp         [[ sampler(SamplerIndexDraw) ]])
{
    float4 colorSample = tex.sample(samp, in.texCoord.xy);
    return colorSample * in.color;
}

fragment half4 stock_fragment_color(FontFragmentIn in [[ stage_in ]])
{
    return half4(in.color);
}

#pragma mark - filter kernels

kernel void convert_bgra4444_to_bgra8888(texture2d<ushort, access::read> in  [[ texture(0) ]],
                                         texture2d<half, access::write>  out [[ texture(1) ]],
                                         uint2                           gid [[ thread_position_in_grid ]])
{
   ushort pix  = in.read(gid).r;
   uchar4 pix2 = uchar4(
                        extract_bits(pix,  4, 4),
                        extract_bits(pix,  8, 4),
                        extract_bits(pix, 12, 4),
                        extract_bits(pix,  0, 4)
                        );

   out.write(half4(pix2) / 15.0, gid);
}

kernel void convert_rgb565_to_bgra8888(texture2d<ushort, access::read> in  [[ texture(0) ]],
                                       texture2d<half, access::write>  out [[ texture(1) ]],
                                       uint2                           gid [[ thread_position_in_grid ]])
{
   ushort pix  = in.read(gid).r;
   uchar4 pix2 = uchar4(
                        extract_bits(pix, 11, 5),
                        extract_bits(pix,  5, 6),
                        extract_bits(pix,  0, 5),
                        0xf
                        );

   out.write(half4(pix2) / half4(0x1f, 0x3f, 0x1f, 0xf), gid);
}

#pragma mark - ribbon simple

namespace ribbon {

float iqhash(float n)
{
  return fract(sin(n) * 43758.5453);
}

float noise(float3 x)
{
  float3 p = floor(x);
  float3 f = fract(x);
  f = f * f * (3.0 - 2.0 * f);
  float n = p.x + p.y * 57.0 + 113.0 * p.z;
  return mix(mix(mix(iqhash(n), iqhash(n + 1.0), f.x),
                 mix(iqhash(n + 57.0), iqhash(n + 58.0), f.x), f.y),
             mix(mix(iqhash(n + 113.0), iqhash(n + 114.0), f.x),
                 mix(iqhash(n + 170.0), iqhash(n + 171.0), f.x), f.y), f.z);
}

float xmb_noise2(float3 x, const device Uniforms &constants)
{
  return cos(x.z * 4.0) * cos(x.z + constants.time / 10.0 + x.x);
}

}

#pragma mark - ribbon simple

vertex FontFragmentIn ribbon_simple_vertex(const SpriteVertex in [[ stage_in ]], const device Uniforms &constants [[ buffer(BufferIndexUniforms) ]])
{
   float4 t = (constants.projectionMatrix * float4(in.position, 0, 1));

   float3 v = float3(t.x, 0.0, 1.0-t.y);
   float3 v2 = v;

   v2.x = v2.x + constants.time / 2.0;
   v2.z = v.z * 3.0;
   v.y = cos((v.x + v.z / 3.0 + constants.time) * 2.0) / 10.0 + ribbon::noise(v2.xyz) / 4.0;
   v.y = -v.y;

   FontFragmentIn out;
   out.position = float4(v, 1.0);
   return out;
}

fragment float4 ribbon_simple_fragment()
{
   return float4(0.05, 0.05, 0.05, 1.0);
}

#pragma mark - ribbon

typedef struct
{
   vector_float4 position [[position]];
   vector_float3 vEC;
} RibbonOutIn;

vertex RibbonOutIn ribbon_vertex(const SpriteVertex in [[ stage_in ]], const device Uniforms &constants [[ buffer(BufferIndexUniforms) ]])
{
   float4 t = (constants.projectionMatrix * float4(in.position, 0, 1));

   float3 v = float3(t.x, 0.0, 1.0-t.y);
   float3 v2 = v;
   float3 v3 = v;

   v.y = ribbon::xmb_noise2(v2, constants) / 8.0;

   v3.x -= constants.time / 5.0;
   v3.x /= 4.0;

   v3.z -= constants.time / 10.0;
   v3.y -= constants.time / 100.0;

   v.z -= ribbon::noise(v3 * 7.0) / 15.0;
   v.y -= ribbon::noise(v3 * 7.0) / 15.0 + cos(v.x * 2.0 - constants.time / 2.0) / 5.0 - 0.3;
   v.y = -v.y;

   RibbonOutIn out;
   out.vEC = v;
   out.position = float4(v, 1.0);
   return out;
}

fragment float4 ribbon_fragment(RibbonOutIn in [[ stage_in ]])
{
   const float3 up = float3(0.0, 0.0, 1.0);
   float3 x = dfdx(in.vEC);
   float3 y = dfdy(in.vEC);
   y = -y; /* Flip Y derivative to match Vulkan's yflip */
   float3 normal = normalize(cross(x, y));
   float c = 1.0 - dot(normal, up);
   c = (1.0 - cos(c * c)) / 3.0;
   return float4(c, c, c, 1.0);
}

#pragma mark - snow constants

constant float snowBaseScale [[ function_constant(0) ]]; //  [1.0  .. 10.0]
constant float snowDensity   [[ function_constant(1) ]]; //  [0.01 ..  1.0]
constant float snowSpeed     [[ function_constant(2) ]]; //  [0.1  ..  1.0]

#pragma mark - snow simple

namespace snow
{

float rand(float2 co)
{
   return fract(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float dist_func(float2 distv)
{
   float dist = sqrt((distv.x * distv.x) + (distv.y * distv.y)) * (40.0 / snowBaseScale);
   dist = clamp(dist, 0.0, 1.0);
   return cos(dist * (3.14159265358 * 0.5)) * 0.5;
}

float random_dots(float2 co)
{
   float part = 1.0 / 20.0;
   float2 cd = floor(co / part);
   float p = rand(cd);

   if (p > 0.005 * (snowDensity * 40.0))
      return 0.0;

   float2 dpos = (float2(fract(p * 2.0) , p) + float2(2.0, 2.0)) * 0.25;

   float2 cellpos = fract(co / part);
   float2 distv = (cellpos - dpos);

   return dist_func(distv);
}

float snow(float2 pos, float time, float scale)
{
   // add wobble
   pos.x += cos(pos.y * 1.2 + time * 3.14159 * 2.0 + 1.0 / scale) / (8.0 / scale) * 4.0;
   // add gravity
   pos += time * scale * float2(-0.5, 1.0) * 4.0;
   return random_dots(pos / scale) * (scale * 0.5 + 0.5);
}

}

fragment float4 snow_fragment(FontFragmentIn        in         [[ stage_in ]],
                              const device Uniforms &constants [[ buffer(BufferIndexUniforms) ]])
{
   float tim = constants.time * 0.4 * snowSpeed;
   float2 pos = in.position.xy / constants.outputSize.xx;
   pos.y = 1.0 - pos.y; // Flip Y
   float a = 0.0;
   // Each of these is a layer of snow
   // Remove some for better performance
   // Changing the scale (3rd value) will mess with the looping
   a += snow::snow(pos, tim, 1.0);
   a += snow::snow(pos, tim, 0.7);
   a += snow::snow(pos, tim, 0.6);
   a += snow::snow(pos, tim, 0.5);
   a += snow::snow(pos, tim, 0.4);
   a += snow::snow(pos, tim, 0.3);
   a += snow::snow(pos, tim, 0.25);
   a += snow::snow(pos, tim, 0.125);
   a = a * min(pos.y * 4.0, 1.0);
   return float4(1.0, 1.0, 1.0, a);
}

fragment float4 bokeh_fragment(FontFragmentIn        in         [[ stage_in ]],
                               const device Uniforms &constants [[ buffer(BufferIndexUniforms) ]])
{
    float speed = constants.time * 4.0;
    float2 uv = -1.0 + 2.0 * in.position.xy / constants.outputSize;
    uv.y = -uv.y; /* Flip Y to match Vulkan's yflip */
    uv.x *= constants.outputSize.x / constants.outputSize.y;
    float3 color = float3(0.0);

    for( int i=0; i < 8; i++ )
    {
        float  pha = sin(float(i) * 546.13 + 1.0) * 0.5 + 0.5;
        float  siz = pow(sin(float(i) * 651.74 + 5.0) * 0.5 + 0.5, 4.0);
        float  pox = sin(float(i) * 321.55 + 4.1) * constants.outputSize.x / constants.outputSize.y;
        float  rad = 0.1 + 0.5 * siz + sin(pha + siz) / 4.0;
        float2 pos = float2(pox + sin(speed / 15. + pha + siz), - 1.0 - rad + (2.0 + 2.0 * rad) * fract(pha + 0.3 * (speed / 7.) * (0.2 + 0.8 * siz)));
        float  dis = length(uv - pos);
        if (dis < rad)
        {
            float3 col = mix(float3(0.194 * sin(speed / 6.0) + 0.3, 0.2, 0.3 * pha), float3(1.1 * sin(speed / 9.0) + 0.3, 0.2 * pha, 0.4), 0.5 + 0.5 * sin(float(i)));
            color +=  col.zyx * (1.0 - smoothstep(rad * 0.15, rad, dis));
        }
    }
    color *= sqrt(1.5 - 0.5 * length(uv));
    return float4(color.r, color.g, color.b , 0.5);
}

namespace snowflake {

float rand_float(float x)
{
   return snow::rand(float2(x, 1.0));
}

float snow(float3 pos, float2 uv, float o, float atime)
{
   float2 d   = (pos.xy - uv);
   float a    = atan(d.y / d.x) + sin(atime*1.0 + o) * 10.0;

   float dist = d.x*d.x + d.y*d.y;

   if (dist < pos.z/400.0)
   {
      float col = 0.0;
      if (sin(a * 8.0) < 0.0)
         col = 1.0;
      if (dist < pos.z/800.0)
         col += 1.0;
      return col * pos.z;
   }

   return 0.0;
}

float col(float2 c, const device Uniforms &constants)
{
   float color = 0.0;
   float atime = (constants.time + 1.0) / 4.0;

   for (int i = 1; i < 15; i++)
   {
      float o = rand_float(float(i) / 3.0) * 15.0;
      float z = rand_float(float(i) + 13.0);
      float x = 1.8 - (3.6) * (rand_float(floor((constants.time*((z + 1.0) / 2.0) +o) / 2.0)) + sin(constants.time * o /1000.0) / 10.0);
      float y = 1.0 - fmod((constants.time * ((z + 1.0)/2.0)) + o, 2.0);

      color += snow(float3(x,y,z), c, o, atime);
   }

   return color;
}

}

fragment float4 snowflake_fragment(FontFragmentIn        in         [[ stage_in ]],
                                   const device Uniforms &constants [[ buffer(BufferIndexUniforms) ]])
{
   float2 uv = in.position.xy / constants.outputSize.xy;
   uv = uv * 2.0 - 1.0;
   float2 p = uv;
   p.x *= constants.outputSize.x / constants.outputSize.y;
   p.y = -p.y; /* Flip Y so snowflakes fall down */

   float c = snowflake::col(p, constants);
   return float4(c,c,c,c);
}

#pragma mark - HDR composite pass
/* Ported from gfx/drivers/vulkan_shaders/hdr_common.glsl + hdr.frag + hdr_tonemap.frag.
 * Keep the math identical to the Vulkan path so shader presets that were
 * authored and tuned against the Vulkan HDR pipeline look the same on Metal. */

namespace hdr
{
   /* Matches HDRUniforms in metal_shader_types.h. MSL lays out constant buffers
    * with std140-compatible rules here (float4 / matrix aligned to 16 bytes),
    * so the fields line up with the CPU-side struct without manual padding
    * beyond what metal_shader_types.h already declares. */

   constant float kMaxNitsFor2084  = 10000.0f;
   constant float kscRGBWhiteNits  = 80.0f;
   constant float kEpsilon         = 0.0001f;

   /* Expanded Rec.709 -> Rec.2020 colour rotations.
    * Note: MSL stores matrices column-major, but we are multiplying as
    * (mat * vec) using the GLSL-native (vec * mat) convention from the
    * reference shader.  To keep the result bit-identical to the Vulkan
    * path, we feed the transposed data into the MSL matrix — that way
    * `v * M` in GLSL maps to `M * v` here with the same result. */
   constant float3x3 k709to2020 = float3x3(
      float3(0.6274040f, 0.0690970f, 0.0163916f),
      float3(0.3292820f, 0.9195400f, 0.0880132f),
      float3(0.0433136f, 0.0113612f, 0.8955950f));

   constant float3x3 k2020to709 = float3x3(
      float3( 1.6604910f, -0.1245505f, -0.0181508f),
      float3(-0.5876411f,  1.1328999f, -0.1005789f),
      float3(-0.0728499f, -0.0083494f,  1.1187297f));

   constant float3x3 kP3to2020 = float3x3(
      float3( 0.753833f,  0.045744f, -0.001210f),
      float3( 0.198597f,  0.941777f,  0.017602f),
      float3( 0.047570f,  0.012479f,  0.983609f));

   constant float3x3 kExpanded709to2020 = float3x3(
      float3( 0.6274040f,  0.0457456f, -0.00121055f),
      float3( 0.3292820f,  0.9417770f,  0.01760410f),
      float3( 0.0433136f,  0.0124772f,  0.98360700f));

   constant float3x3 k2020toExpanded709 = float3x3(
      float3( 1.6353500f, -0.0794803f,  0.00343516f),
      float3(-0.5705700f,  1.0898000f, -0.02020700f),
      float3(-0.0647755f, -0.0103244f,  1.01677000f));

   inline float3 LinearToST2084(float3 v)
   {
      float3 a = pow(abs(v), float3(0.1593017578f));
      return pow((0.8359375f + 18.8515625f * a) / (1.0f + 18.6875f * a),
                 float3(78.84375f));
   }

   inline float3 ST2084ToLinear(float3 v)
   {
      float3 a = pow(abs(v), float3(1.0f / 78.84375f));
      return pow(abs(max(a - 0.8359375f, 0.0f)
                     / (18.8515625f - 18.6875f * a)),
                 float3(1.0f / 0.1593017578f));
   }

   /* Normalised linear -> display-nits scene value. */
   inline float3 CalcHDRSceneValue(float3 nits, float brightnessNits)
   {
      return nits * (kMaxNitsFor2084 / brightnessNits);
   }

   /* HDR10 PQ -> linear BT.709 SDR (used by the tonemap/readback path). */
   inline float3 HDR10ToLinear(float3 hdr10,
                               float brightnessNits,
                               uint expandGamut)
   {
      float3 normalisedLinear = ST2084ToLinear(hdr10);
      float3 rec2020          = CalcHDRSceneValue(normalisedLinear, brightnessNits);
      float3 hdr              = k2020to709 * rec2020;
      if (expandGamut == 1u)
         hdr = k2020toExpanded709 * rec2020;
      return hdr;
   }

   /* HDR10 PQ (BT.2020) -> scRGB (BT.709, 1.0 = 80 nits). */
   inline float3 HDR10ToscRGB(float3 hdr10)
   {
      float3 linear10k = ST2084ToLinear(hdr10);
      float3 linear709 = k2020to709 * linear10k;
      return linear709 * (kMaxNitsFor2084 / kscRGBWhiteNits);
   }

   /* Forward (SDR -> HDR) inverse tonemap, Reinhard-like.  Used when the
    * incoming picture is SDR-linear and we need to lift it into a higher
    * display range.  Matches hdr_common.glsl::InverseTonemap. */
   inline float3 InverseTonemap(float3 sdrLinear,
                                float maxNits,
                                float paperWhiteNits)
   {
      float inputVal = max(sdrLinear.r, max(sdrLinear.g, sdrLinear.b));
      if (inputVal < kEpsilon)
         return sdrLinear;
      float peakRatio    = maxNits / paperWhiteNits;
      float denom        = max(1.0f - inputVal * (1.0f - (1.0f / peakRatio)),
                               kEpsilon);
      float tonemapped   = inputVal / denom;
      return sdrLinear * (tonemapped / inputVal);
   }

   /* Forward HDR->SDR tonemap (used for the readback path). */
   inline float3 Tonemap(float3 hdrLinear,
                         float maxNits,
                         float paperWhiteNits)
   {
      float inputVal = max(hdrLinear.r, max(hdrLinear.g, hdrLinear.b));
      if (inputVal < kEpsilon)
         return hdrLinear;
      float peakRatio = maxNits / paperWhiteNits;
      float k         = 1.0f - (1.0f / peakRatio);
      return hdrLinear / (1.0f + inputVal * k);
   }

   /* sRGB EOTF / OETF. */
   inline float3 sRGBToLinear(float3 c)
   {
      return pow(max(c, float3(0.0f)), float3(2.4f));
   }

   inline float3 LinearToSRGB(float3 c)
   {
      float3 clamped = clamp(c, float3(0.0f), float3(1.0f));
      float3 lo      = clamped * 12.92f;
      float3 hi      = 1.055f * pow(clamped, float3(1.0f / 2.4f)) - 0.055f;
      return select(hi, lo, clamped <= float3(0.0031308f));
   }

   /* Gamut rotation for the forward HDR path.  Mirrors hdr.frag::To2020.
    *    0 accurate   -> true Rec.709 -> Rec.2020
    *    1 expanded   -> slightly wider than 709, gentle boost
    *    2 wide       -> DCI-P3 -> Rec.2020
    *    3 super      -> pass through (interpreted as 2020 later -> big boost) */
   inline float3 To2020(float3 sdrLinear, uint gamut)
   {
      float3 result;
      if (gamut == 0u)
         result = k709to2020          * sdrLinear;
      else if (gamut == 1u)
         result = kExpanded709to2020 * sdrLinear;
      else if (gamut == 2u)
         result = kP3to2020           * sdrLinear;
      else
         result = sdrLinear;
      return max(result, float3(0.0f));
   }

   inline float3 HDR10Encode(float3 hdrLinear, float brightnessNits)
   {
      float3 pq = hdrLinear * (brightnessNits / kMaxNitsFor2084);
      return LinearToST2084(max(pq, float3(0.0f)));
   }
} /* namespace hdr */

/* CRT scanline + subpixel mask generator.
 * Ported from hdr.frag::{ScanlineColour, GenerateScanline, Scanlines}.
 * Used when OutputSize.y is large enough and Scanlines uniform is set,
 * giving shader presets a CRT-mask path that lives in HDR space. */
namespace hdr_crt
{
   constant float  kBeamWidth            = 0.5f;
   constant uint   kChannelMask          = 3u;
   constant uint   kFirstChannelShift    = 2u;

   constant uint   kRedId                = 0u;
   constant uint   kGreenId              = 1u;
   constant uint   kBlueId               = 2u;

   constant uint   kRed                  = (1u | (kRedId   << kFirstChannelShift));
   constant uint   kGreen                = (1u | (kGreenId << kFirstChannelShift));
   constant uint   kBlue                 = (1u | (kBlueId  << kFirstChannelShift));
   constant uint   kBlack                = 0u;

   constant uint   kRGBX                 = ((kRed  << 0u)
                                           | (kGreen << 4u)
                                           | (kBlue  << 8u)
                                           | (kBlack << 12u));
   constant uint   kRBGX                 = ((kRed  << 0u)
                                           | (kBlue  << 4u)
                                           | (kGreen << 8u)
                                           | (kBlack << 12u));
   constant uint   kBGRX                 = ((kBlue << 0u)
                                           | (kGreen << 4u)
                                           | (kRed   << 8u)
                                           | (kBlack << 12u));

   /* Fixed CRT-geometry constants — match hdr.frag. */
   constant float  k_crt_h_size                        = 1.0f;
   constant float  k_crt_v_size                        = 1.0f;
   constant float  k_crt_h_cent                        = 0.0f;
   constant float  k_crt_v_cent                        = 0.0f;
   constant float  k_crt_pin_phase                     = 0.0f;

   constant float  k_crt_red_vertical_convergence      = 0.0f;
   constant float  k_crt_red_horizontal_convergence    = 0.0f;
   constant float  k_crt_red_scanline_min              = 0.45f;
   constant float  k_crt_red_scanline_max              = 0.90f;
   constant float  k_crt_red_scanline_attack           = 0.60f;
   constant float  k_crt_red_beam_sharpness            = 1.30f;
   constant float  k_crt_red_beam_attack               = 1.00f;

   constant float4 kFallOffControlPoints  = float4(0.0f, 0.0f, 0.0f, 1.0f);
   constant float4 kAttackControlPoints   = float4(0.0f, 1.0f, 1.0f, 1.0f);

   constant float4x4 kCubicBezier = float4x4(
      float4( 1.0f,  0.0f,  0.0f,  0.0f),
      float4(-3.0f,  3.0f,  0.0f,  0.0f),
      float4( 3.0f, -6.0f,  3.0f,  0.0f),
      float4(-1.0f,  3.0f, -3.0f,  1.0f));

   constant float3 kColourMask[3] = {
      float3(1.0f, 0.0f, 0.0f),
      float3(0.0f, 1.0f, 0.0f),
      float3(0.0f, 0.0f, 1.0f)
   };

   inline float3 SampleLinear(texture2d<float> src, sampler samp, float2 uv)
   {
      float4 sdr = src.sample(samp, uv);
      return pow(abs(sdr.rgb), float3(2.4f));
   }

   inline float Bezier(float t0, float4 cp)
   {
      float4 t = float4(1.0f, t0, t0 * t0, t0 * t0 * t0);
      return dot(t, kCubicBezier * cp);
   }

   inline float4 BeamControlPoints(float beamAttack, bool falloff)
   {
      float innerAttack = clamp(beamAttack, 0.0f, 1.0f);
      float outerAttack = clamp(beamAttack - 1.0f, 0.0f, 1.0f);
      return falloff
         ? kFallOffControlPoints + float4(0.0f, outerAttack, innerAttack, 0.0f)
         : kAttackControlPoints   - float4(0.0f, innerAttack, outerAttack, 0.0f);
   }

   /* Linear-Rec.709 beam trace for a single scanline pair. */
   inline float3 ScanlineColour(
         texture2d<float> src, sampler samp,
         float2 texCoord, float2 sourceSize, float scanlineSize,
         float sourceTexCoordX, float narrowedSourcePixelOffset,
         float verticalConvergence, float beamAttack,
         float scanlineMin, float scanlineMax, float scanlineAttack,
         float verticalBias)
   {
      float currentY   = (texCoord.y * sourceSize.y) - verticalConvergence;
      float centerLine = floor(currentY) + 0.5f + verticalBias;
      float distToLine = currentY - centerLine;

      if (fabs(distToLine) > 1.5f)
         return float3(0.0f);

      float  sourceY = centerLine / sourceSize.y;
      float2 uv0     = float2(sourceTexCoordX,                         sourceY);
      float2 uv1     = float2(sourceTexCoordX + (1.0f / sourceSize.x), sourceY);

      float3 signal0 = SampleLinear(src, samp, uv0);
      float3 signal1 = SampleLinear(src, samp, uv1);

      float3 result  = float3(0.0f);

      float beamWidthAdj    = kBeamWidth / scanlineSize;
      float rawDist         = fabs(distToLine) - beamWidthAdj;
      float distAdjusted    = max(0.0f, rawDist);
      float effectiveDist   = distAdjusted * 2.0f;

      for (int ch = 0; ch < 3; ++ch)
      {
         float horizInterp     = Bezier(
            narrowedSourcePixelOffset,
            BeamControlPoints(beamAttack, signal0[ch] > signal1[ch]));

         float signalChannel   = mix(signal0[ch], signal1[ch], horizInterp);
         float signalStrength  = clamp(signalChannel, 0.0f, 1.0f);

         float beamWidth       = mix(scanlineMin, scanlineMax, signalStrength);
         float chanDist        = clamp(effectiveDist / beamWidth, 0.0f, 1.0f);

         float4 chanCP         = float4(1.0f, 1.0f,
                                        signalStrength * scanlineAttack, 0.0f);
         float  luminance      = Bezier(chanDist, chanCP);

         result[ch] = luminance * signalChannel;
      }

      return result;
   }

   inline float3 GenerateScanline(
         texture2d<float> src, sampler samp,
         float2 texCoord, float2 sourceSize, float scanlineSize,
         float horizontalConvergence, float verticalConvergence,
         float beamSharpness, float beamAttack,
         float scanlineMin, float scanlineMax, float scanlineAttack)
   {
      float currentX           = (texCoord.x * sourceSize.x) - horizontalConvergence;
      float currentCenterX     = floor(currentX) + 0.5f;
      float sourceTexCoordX    = currentCenterX / sourceSize.x;
      float sourcePixelOffset  = fract(currentX);
      float narrowedOffset     = clamp(((sourcePixelOffset - 0.5f) * beamSharpness) + 0.5f,
                                       0.0f, 1.0f);

      return ScanlineColour(src, samp,
                            texCoord, sourceSize, scanlineSize, sourceTexCoordX,
                            narrowedOffset, verticalConvergence, beamAttack,
                            scanlineMin, scanlineMax, scanlineAttack,
                            0.0f);
   }

   /* Returns linear-Rec.709 scanline colour with the subpixel mask already
    * applied in the output colour-space.  Caller takes care of the final
    * PQ / scRGB encode. */
   inline float3 Scanlines(texture2d<float> src, sampler samp,
                           float2 texcoord,
                           constant HDRUniforms &u)
   {
      float2 sourceSize   = u.SourceSize.xy;
      float2 outputSize   = u.OutputSize.xy;

      float2 scanlineCoord = texcoord - float2(0.5f);
      scanlineCoord        = scanlineCoord
         * float2(1.0f + (k_crt_pin_phase * scanlineCoord.y), 1.0f);
      scanlineCoord        = scanlineCoord * float2(k_crt_h_size, k_crt_v_size);
      scanlineCoord        = scanlineCoord + float2(0.5f);
      scanlineCoord        = scanlineCoord
         + float2(k_crt_h_cent, k_crt_v_cent) / outputSize;

      float2 currentPos   = texcoord * outputSize;
      uint   mask_idx     = uint(floor(fmod(currentPos.x, 4.0f)));
      uint   rgbMask      = (u.SubpixelLayout == 0u) ? kRGBX
                          : (u.SubpixelLayout == 1u) ? kRBGX
                          : kBGRX;
      uint   colour_mask  = (rgbMask >> (mask_idx * 4u)) & 0xFu;

      float  scanlineSize = outputSize.y / sourceSize.y;

      float3 scanlineColour = GenerateScanline(
         src, samp,
         scanlineCoord,
         sourceSize,
         scanlineSize,
         k_crt_red_horizontal_convergence,
         k_crt_red_vertical_convergence,
         k_crt_red_beam_sharpness,
         k_crt_red_beam_attack,
         k_crt_red_scanline_min,
         k_crt_red_scanline_max,
         k_crt_red_scanline_attack);

      float3 linear_709 = max(scanlineColour, float3(0.0f));

      uint   channelCount = colour_mask & 3u;
      float3 mask         = float3(0.0f);
      if (channelCount > 0u)
      {
         uint ch0 = (colour_mask >> kFirstChannelShift) & 3u;
         mask = kColourMask[ch0];
      }

      if (u.HDRMode == 2u)
      {
         /* scRGB output — mask in linear Rec.709. */
         float3 linear_2020 = hdr::To2020(linear_709, u.ExpandGamut);
         float3 linear_scrgb = hdr::k2020to709 * linear_2020;
         return linear_scrgb * mask;
      }
      /* HDR10 output — mask in Rec.2020 post-inverse-tonemap. */
      float3 linear_2020 = hdr::To2020(linear_709, u.ExpandGamut);
      float3 hdr_2020    = hdr::InverseTonemap(linear_2020,
                                               u.BrightnessNits,
                                               u.BrightnessNits);
      return hdr_2020 * mask;
   }
} /* namespace hdr_crt */

/* Full-screen quad vertex shader for the composite/tonemap passes.  Takes a
 * bare 4-vertex triangle strip in clip-space and emits UVs; no MVP needed. */
typedef struct
{
   float4 position [[position]];
   float2 texCoord;
} HDRVertexOut;

constant float4 kHDRQuadPositions[4] = {
   float4(-1.0f,  1.0f, 0.0f, 1.0f),
   float4( 1.0f,  1.0f, 0.0f, 1.0f),
   float4(-1.0f, -1.0f, 0.0f, 1.0f),
   float4( 1.0f, -1.0f, 0.0f, 1.0f)
};

constant float2 kHDRQuadUVs[4] = {
   float2(0.0f, 0.0f),
   float2(1.0f, 0.0f),
   float2(0.0f, 1.0f),
   float2(1.0f, 1.0f)
};

vertex HDRVertexOut hdr_composite_vertex(uint vid [[vertex_id]])
{
   HDRVertexOut out;
   out.position = kHDRQuadPositions[vid];
   out.texCoord = kHDRQuadUVs[vid];
   return out;
}

/* Sample helper: texture is assumed to carry SDR in sRGB-gamma space (2.4
 * approximation matches the Vulkan shader).  Only used when the shader
 * chain produced SDR output, i.e. not on the shader-emitted-HDR paths. */
inline float4 hdr_sample_sdr_linear(texture2d<float> src,
                                    sampler          samp,
                                    float2           uv)
{
   float4 sdr        = src.sample(samp, uv);
   float3 sdr_linear = pow(abs(sdr.rgb), float3(2.4f));
   return float4(sdr_linear, sdr.a);
}

/* Forward HDR composite.
 *
 * HDRMode == 3  : Source is PQ HDR10 (shader emitted), convert to scRGB.
 * HDRMode == 2  : scRGB output.  Source is either SDR or FP16 HDR.
 * HDRMode == 1  : HDR10 output.  Source is either SDR or already-PQ.
 * HDRMode == 0  : Passthrough (bypass path, rarely used since the composite
 *                 is skipped when HDR is disabled). */
fragment float4 hdr_composite_fragment(
      HDRVertexOut            in       [[ stage_in ]],
      constant HDRUniforms    &u       [[ buffer(0) ]],
      texture2d<float>        src      [[ texture(0) ]],
      sampler                 samp     [[ sampler(0) ]])
{
   if (u.HDRMode == 3u)
   {
      /* Shader chain emitted PQ HDR10, swapchain is scRGB -> convert. */
      float4 pq = src.sample(samp, in.texCoord);
      return float4(hdr::HDR10ToscRGB(pq.rgb), pq.a);
   }

   if (u.HDRMode == 2u)
   {
      /* scRGB swapchain.  Either HDR16 (shader emits linear float) or SDR.
       * For HDR16 we expect the shader output to already be in linear BT.709
       * 1.0 = 80 nits, so just pass through.  For SDR, apply gamut rotation
       * + scale by BrightnessNits / 80. */
      if (u.InverseTonemap <= 0.0f && u.HDR10 <= 0.0f)
      {
         /* Shader already emits scRGB-compatible linear HDR (HDR16 path). */
         float4 linear = src.sample(samp, in.texCoord);
         return linear;
      }

      /* High-res SDR with scanlines requested: generate CRT mask in HDR.
       * Scanlines() returns linear Rec.709 already masked in Rec.709 space;
       * scRGB units are 1.0 = 80 nits so scale by BrightnessNits/80. */
      if (u.Scanlines > 0.0f && u.OutputSize.y > (240.0f * 4.0f))
      {
         float3 linear = hdr_crt::Scanlines(src, samp, in.texCoord, u);
         return float4(linear * (u.BrightnessNits / hdr::kscRGBWhiteNits), 1.0f);
      }

      float4 linear  = float4(hdr::To2020(
                                hdr_sample_sdr_linear(src, samp, in.texCoord).rgb,
                                u.ExpandGamut),
                              1.0f);
      linear.rgb     = hdr::k2020to709 * linear.rgb;
      linear.rgb    *= u.BrightnessNits / hdr::kscRGBWhiteNits;
      return linear;
   }

   /* HDRMode == 1: HDR10 output. */

   /* Shader already emitted PQ or FP16 HDR content -> pass through, the
    * shader's output is authoritative.  We rely on set_hdr10()/set_hdr16()
    * clearing InverseTonemap + HDR10 in the driver wiring. */
   if (u.InverseTonemap <= 0.0f && u.HDR10 <= 0.0f)
      return src.sample(samp, in.texCoord);

   /* SDR input, both inverse-tonemap and HDR10 encode requested. */
   if (u.InverseTonemap > 0.0f && u.HDR10 > 0.0f)
   {
      if (u.Scanlines > 0.0f && u.OutputSize.y > (240.0f * 4.0f))
      {
         /* Scanlines() returns linear Rec.2020 with inverse-tonemap + mask
          * baked in, so we only need the PQ encode. */
         float3 hdr_2020 = hdr_crt::Scanlines(src, samp, in.texCoord, u);
         float3 pq       = hdr::HDR10Encode(hdr_2020, u.BrightnessNits);
         return float4(pq, 1.0f);
      }

      float4 linear    = hdr_sample_sdr_linear(src, samp, in.texCoord);
      float3 rec2020   = hdr::To2020(linear.rgb, u.ExpandGamut);
      float3 hdr2020   = hdr::InverseTonemap(rec2020,
                                             u.BrightnessNits,
                                             u.BrightnessNits);
      float3 pq        = hdr::HDR10Encode(hdr2020, u.BrightnessNits);
      return float4(pq, linear.a);
   }

   if (u.InverseTonemap > 0.0f)
   {
      float4 linear  = hdr_sample_sdr_linear(src, samp, in.texCoord);
      float3 rec2020 = hdr::To2020(linear.rgb, u.ExpandGamut);
      float3 hdr2020 = hdr::InverseTonemap(rec2020,
                                           u.BrightnessNits,
                                           u.BrightnessNits);
      return float4(hdr2020, linear.a);
   }

   /* HDR10 only */
   float4 linear    = hdr_sample_sdr_linear(src, samp, in.texCoord);
   float3 rec2020   = hdr::To2020(linear.rgb, u.ExpandGamut);
   float3 pq        = hdr::HDR10Encode(rec2020, u.BrightnessNits);
   return float4(pq, linear.a);
}

/* HDR -> SDR screenshot/recording path.
 *
 * Called from Context::readBackBuffer when the active swapchain is HDR.
 * The backbuffer is sampled and mapped back to BGRA8 linear+sRGB-encoded
 * output suitable for PNG encoding.
 *
 * HDRMode semantics here match the Vulkan hdr_tonemap.frag:
 *   1 = source is HDR10 PQ  (10-bit BT.2020 ST.2084)
 *   2 = source is scRGB     (FP16 BT.709, 1.0 = 80 nits)
 *   other = passthrough (SDR, shouldn't normally hit this path). */
fragment float4 hdr_tonemap_fragment(
      HDRVertexOut            in       [[ stage_in ]],
      constant HDRUniforms    &u       [[ buffer(0) ]],
      texture2d<float>        src      [[ texture(0) ]],
      sampler                 samp     [[ sampler(0) ]])
{
   float4 source     = src.sample(samp, in.texCoord);
   float3 sdr_linear;

   if (u.HDRMode == 1u)
   {
      float3 hdr_linear = hdr::HDR10ToLinear(source.rgb,
                                             u.BrightnessNits,
                                             u.ExpandGamut);
      sdr_linear        = hdr::Tonemap(hdr_linear,
                                       u.BrightnessNits,
                                       u.BrightnessNits);
   }
   else if (u.HDRMode == 2u)
   {
      /* scRGB 1.0 == 80 nits, SDR paper white sits at BrightnessNits —
       * undo the scale so SDR paper white maps back to 1.0. */
      sdr_linear = source.rgb * (hdr::kscRGBWhiteNits / u.BrightnessNits);
   }
   else
   {
      sdr_linear = source.rgb;
   }

   return float4(hdr::LinearToSRGB(sdr_linear), 1.0f);
}
