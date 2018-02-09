
#define SRC(...) #__VA_ARGS__
SRC(

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

      float4 VSMain(float2 position : POSITION) : SV_POSITION
      {
         float3 v = float3(position.x, 0.0, position.y);
         float3 v2 = v;
         v2.x = v2.x + global.time / 2.0;
         v2.z = v.z * 3.0;
         v.y = cos((v.x + v.z / 3.0 + global.time) * 2.0) / 10.0 + noise(v2.xyz) / 4.0;
         v.y = -v.y;

         return float4(v.xy, 0.0, 1.0);
      }

      float4 PSMain() : SV_TARGET
      {
         return float4(0.05, 0.05, 0.05, 1.0);
      };
)
