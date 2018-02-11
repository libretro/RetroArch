
#define SRC(...) #__VA_ARGS__
SRC(

      struct PSInput
      {
         float4 position : SV_POSITION;
         float3 vEC      : TEXCOORD;
      };

      struct UBO
      {
         float4x4 modelViewProj;
         float2 Outputsize;
         float time;
      };
      uniform UBO global;

      float iqhash(float n)
      {
         return frac(sin(n) * 43758.5453);
      }

      float noise(float3 x)
      {
         float3 p = floor(x);
         float3 f = frac(x);
         f = f * f * (3.0 - 2.0 * f);
         float n = p.x + p.y * 57.0 + 113.0 * p.z;
         return lerp(lerp(lerp(iqhash(n), iqhash(n + 1.0), f.x),
                    lerp(iqhash(n + 57.0), iqhash(n + 58.0), f.x), f.y),
                    lerp(lerp(iqhash(n + 113.0), iqhash(n + 114.0), f.x),
                    lerp(iqhash(n + 170.0), iqhash(n + 171.0), f.x), f.y), f.z);
      }

      float xmb_noise2(float3 x)
      {
         return cos(x.z * 4.0) * cos(x.z + global.time / 10.0 + x.x);
      }

      PSInput VSMain(float2 position : POSITION)
      {
         float3 v = float3(position.x, 0.0, 1.0-position.y);
         float3 v2 = v;
         float3 v3 = v;

         v.y = xmb_noise2(v2) / 8.0;

         v3.x -= global.time / 5.0;
         v3.x /= 4.0;

         v3.z -= global.time / 10.0;
         v3.y -= global.time / 100.0;

         v.z -= noise(v3 * 7.0) / 15.0;
         v.y -= noise(v3 * 7.0) / 15.0 + cos(v.x * 2.0 - global.time / 2.0) / 5.0 - 0.3;
         v.y = -v.y;

         PSInput output;
         output.position = float4(v.xy, 0.0, 1.0);
         output.vEC = v;
         return output;
      }

      float4 PSMain(PSInput input) : SV_TARGET
      {
         const float3 up = float3(0.0, 0.0, 1.0);
         float3 x = ddx(input.vEC);
         float3 y = ddy(input.vEC);
         float3 normal = normalize(cross(x, y));
         float c = 1.0 - dot(normal, up);
         c = (1.0 - cos(c * c)) / 13.0;
         return  float4(c, c, c, 1.0);
   //      return  float4(c, c, c, c);
   //      return float4(1.0, 1.0, 1.0, c);
   //      return float4(1.0, 0.0, 1.0, 1.0);
      };
)
