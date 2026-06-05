
static const char *hlsl_bokeh_program = CG(

   uniform float4x4 modelViewProj;
   uniform float2 OutputSize;
   uniform float time;

   void main_vertex(
      float4 position : POSITION,
      float2 texcoord : TEXCOORD0,
      out float4 oPosition : POSITION,
      out float4 vdata0 : TEXCOORD0,
      out float4 vdata1 : TEXCOORD1
   )
   {
      oPosition = mul(modelViewProj, position);
      float2 vpos = float2(
         (oPosition.x / oPosition.w * 0.5 + 0.5) * OutputSize.x,
         (0.5 - oPosition.y / oPosition.w * 0.5) * OutputSize.y);
      /* Pack vpos, OutputSize, and time into two float4 varyings */
      vdata0 = float4(vpos, OutputSize);
      vdata1 = float4(time, 0.0, 0.0, 0.0);
   }

   struct output
   {
      float4 color : COLOR;
   };

   output main_fragment(float4 vdata0 : TEXCOORD0, float4 vdata1 : TEXCOORD1)
   {
      output OUT;
      float2 vpos = vdata0.xy;
      float2 osize = vdata0.zw;
      float speed = vdata1.x * 4.0;
      float2 uv = -1.0 + 2.0 * vpos.xy / osize;
      uv.x *= osize.x / osize.y;
      float3 color = float3(0.0, 0.0, 0.0);

      for (int i = 0; i < 8; i++)
      {
         float pha = sin(float(i) * 546.13 + 1.0) * 0.5 + 0.5;
         float siz = pow(sin(float(i) * 651.74 + 5.0) * 0.5 + 0.5, 4.0);
         float pox = sin(float(i) * 321.55 + 4.1) * osize.x / osize.y;
         float rad = 0.1 + 0.5 * siz + sin(pha + siz) / 4.0;
         float2 pos = float2(pox + sin(speed / 15. + pha + siz),
            -1.0 - rad + (2.0 + 2.0 * rad) * frac(pha + 0.3 * (speed / 7.) * (0.2 + 0.8 * siz)));
         float dis = length(uv - pos);
         if (dis < rad)
         {
            float3 col = lerp(
               float3(0.194 * sin(speed / 6.0) + 0.3, 0.2, 0.3 * pha),
               float3(1.1 * sin(speed / 9.0) + 0.3, 0.2 * pha, 0.4),
               0.5 + 0.5 * sin(float(i)));
            color += col.zyx * (1.0 - smoothstep(rad * 0.15, rad, dis));
         }
      }
      color *= sqrt(1.5 - 0.5 * length(uv));
      OUT.color = float4(color.r, color.g, color.b, 0.5);
      return OUT;
   }
);
