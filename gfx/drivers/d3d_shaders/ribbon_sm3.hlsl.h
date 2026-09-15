
static const char *hlsl_ribbon_program = CG(

   uniform float time;
   uniform float alpha;

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
      return cos(x.z * 4.0) * cos(x.z + time / 10.0 + x.x);
   }

   void main_vertex(
      float3 position : POSITION,
      out float4 oPosition : POSITION,
      out float3 vEC : TEXCOORD0
   )
   {
      float3 v = float3(position.x, 0.0, 1.0 - position.y);
      float3 v2 = v;
      float3 v3 = v;

      v.y = xmb_noise2(v2) / 8.0;

      v3.x -= time / 5.0;
      v3.x /= 4.0;

      v3.z -= time / 10.0;
      v3.y -= time / 100.0;

      v.z -= noise(v3 * 7.0) / 15.0;
      v.y -= noise(v3 * 7.0) / 15.0 + cos(v.x * 2.0 - time / 2.0) / 5.0 - 0.3;
      v.y = -v.y;

      vEC = v;
      oPosition = float4(v.xy, 0.0, 1.0);
   }

   struct output
   {
      float4 color : COLOR;
   };

   output main_fragment(float3 vEC : TEXCOORD0)
   {
      output OUT;
      float3 up = float3(0.0, 0.0, 1.0);
      float3 x = ddx(vEC);
      float3 y = ddy(vEC);
      float3 normal = normalize(cross(x, y));
      float c = 1.0 - dot(normal, up);
      c = (1.0 - cos(c * c)) / 13.0;
      OUT.color = float4(c, c, c, alpha);
      return OUT;
   }
);
