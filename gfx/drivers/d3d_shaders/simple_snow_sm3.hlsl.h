
static const char *hlsl_simple_snow_program = CG(

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
      /* Pass screen position via texcoord since SM3 pixel shaders
       * cannot reliably read VPOS on all hardware. Map from
       * clip [-1,1] to screen [0,OutputSize]. */
      vpos = float2(
         (oPosition.x / oPosition.w * 0.5 + 0.5) * OutputSize.x,
         (0.5 - oPosition.y / oPosition.w * 0.5) * OutputSize.y);
   }





   float rand(float2 co)
   {
      return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
   }

   float dist_func(float2 distv)
   {
      float dist = sqrt((distv.x * distv.x) + (distv.y * distv.y)) * (40.0 / 1.25);
      dist = clamp(dist, 0.0, 1.0);
      return cos(dist * (3.14159265358 * 0.5)) * 0.5;
   }

   float random_dots(float2 co)
   {
      float part = 1.0 / 20.0;
      float2 cd = floor(co / part);
      float p = rand(cd);

      if (p > 0.005 * (0.5 * 40.0))
         return 0.0;

      float2 dpos = (float2(frac(p * 2.0) , p) + float2(2.0, 2.0)) * 0.25;
      float2 cellpos = frac(co / part);
      float2 distv = (cellpos - dpos);

      return dist_func(distv);
   }

   float snow(float2 pos, float t, float scale)
   {
      pos.x += cos(pos.y * 1.2 + t * 3.14159 * 2.0 + 1.0 / scale) / (8.0 / scale) * 4.0;
      float2 scroll = t * scale * float2(-0.5, 1.0) * 4.0;
      pos += frac(scroll / 0.05) * 0.05;
      return random_dots(pos / scale) * (scale * 0.5 + 0.5);
   }

   struct output
   {
      float4 color : COLOR;
   };

   output main_fragment(float2 vpos : TEXCOORD0)
   {
      output OUT;
      float tim = time * 0.4 * 0.15;
      float2 pos = vpos.xy / OutputSize.xx;
      pos.y = 1.0 - pos.y;
      float a = 0.0;
      a += snow(pos, tim, 1.0);
      a += snow(pos, tim, 0.7);
      a += snow(pos, tim, 0.6);
      a += snow(pos, tim, 0.5);
      a += snow(pos, tim, 0.4);
      a += snow(pos, tim, 0.3);
      a += snow(pos, tim, 0.25);
      a += snow(pos, tim, 0.125);
      a = a * min(pos.y * 4.0, 1.0);
      OUT.color = float4(1.0, 1.0, 1.0, a);
      return OUT;
   }
);
