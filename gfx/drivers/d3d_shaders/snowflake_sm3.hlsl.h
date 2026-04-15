
static const char *hlsl_snowflake_program = CG(

   uniform float4x4 modelViewProj;
   uniform float2 OutputSize;
   uniform float time;

   void main_vertex(
      float4 position : POSITION,
      float2 texcoord : TEXCOORD0,
      out float4 oPosition : POSITION,
      out float2 vpos : TEXCOORD0
   )
   {
      oPosition = mul(modelViewProj, position);
      vpos = float2(
         (oPosition.x / oPosition.w * 0.5 + 0.5) * OutputSize.x,
         (0.5 - oPosition.y / oPosition.w * 0.5) * OutputSize.y);
   }

   float rand(float2 co)
   {
      return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
   }

   float rand_float(float x)
   {
      return rand(float2(x, 1.0));
   }

   float snow(float3 pos, float2 uv, float o, float atime)
   {
      float2 d = (pos.xy - uv);
      float a = atan2(d.y, d.x) + sin(atime * 1.0 + o) * 10.0;

      float dist = d.x * d.x + d.y * d.y;

      if (dist < pos.z / 400.0)
      {
         float col = 0.0;
         if (sin(a * 8.0) < 0.0)
         {
            col = 1.0;
         }
         if (dist < pos.z / 800.0)
         {
            col += 1.0;
         }
         return col * pos.z;
      }

      return 0.0;
   }

   float col(float2 c, float atime)
   {
      float color = 0.0;
      for (int i = 1; i < 15; i++)
      {
         float o = rand_float(float(i) / 3.0) * 15.0;
         float z = rand_float(float(i) + 13.0);
         float x = 1.8 - (3.6) * (rand_float(floor((time * ((z + 1.0) / 2.0) + o) / 2.0))
            + sin(time * o / 1000.0) / 10.0);
         float y = 1.0 - fmod((time * ((z + 1.0) / 2.0)) + o, 2.0);

         color += snow(float3(x, y, z), c, o, atime);
      }
      return color;
   }

   struct output
   {
      float4 color : COLOR;
   };

   output main_fragment(float2 vpos : TEXCOORD0)
   {
      output OUT;
      float atime = (time + 1.0) / 4.0;
      float2 uv = vpos.xy / OutputSize.xy;
      uv = uv * 2.0 - 1.0;
      float2 p = uv;
      p.x *= OutputSize.x / OutputSize.y;
      p.y = -p.y;

      float c = col(p, atime);
      OUT.color = float4(c, c, c, c);
      return OUT;
   }
);
