//
//  pipeline_ribbon.metal
//  RetroArch
//
//  Created by Stuart Carnie on 6/30/18.
//

#include <metal_stdlib>

#import "ShaderTypes.h"

using namespace metal;

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
   float3 normal = normalize(cross(x, y));
   float c = 1.0 - dot(normal, up);
   c = (1.0 - cos(c * c)) / 13.0;
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
        if(dis < rad)
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
   float2 d = (pos.xy - uv);
   float a = atan(d.y / d.x) + sin(atime*1.0 + o) * 10.0;

   float dist = d.x*d.x + d.y*d.y;

   if(dist < pos.z/400.0)
   {
      float col = 0.0;
      if(sin(a * 8.0) < 0.0)
      {
         col=1.0;
      }
      if(dist < pos.z/800.0)
      {
         col+=1.0;
      }
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
   //p.y  = -p.y;

   float c = snowflake::col(p, constants);
   return float4(c,c,c,c);
}
