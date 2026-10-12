
#define SRC(...) #__VA_ARGS__
SRC(
   /* Rect: the part of the texture drawn, top-left first. Clamp: its
    * first and last texel centres, so filtering stops at its edge as at
    * a texture's. */
   struct UBO
   {
      float4x4 modelViewProj;
      float4 Rect;
      float4 Clamp;
   };
   uniform UBO global;

   struct PSInput
   {
      float4 position : SV_POSITION;
      float2 texcoord : TEXCOORD0;
   };
   PSInput VSMain(float4 position : POSITION, float2 texcoord : TEXCOORD0, float4 color : COLOR)
   {
      PSInput result;
      result.position = mul(global.modelViewProj, position);
      result.texcoord = lerp(global.Rect.xy, global.Rect.zw, texcoord);
      return result;
   }
   uniform sampler s0;
   uniform Texture2D <float4> t0;

   /* A view of the frame, as the stock chain draws a texture of its own */
   float4 PSRect(PSInput input) : SV_TARGET
   {
      return float4(t0.Sample(s0,
            clamp(input.texcoord, global.Clamp.xy, global.Clamp.zw)).rgb, 1.0);
   }

   /* The UI layer, premultiplied */
   float4 PSLayer(PSInput input) : SV_TARGET
   {
      return t0.Sample(s0, input.texcoord);
   }

   /* The canvas: left eye on the left half, right eye on the right.
    * Dubois least-squares red/cyan matrices. */
   float4 PSAnaglyph(PSInput input) : SV_TARGET
   {
      float2 uv = float2(input.texcoord.x * 0.5, input.texcoord.y);
      float3 l  = t0.Sample(s0, uv).rgb;
      float3 r  = t0.Sample(s0, uv + float2(0.5, 0.0)).rgb;
      float3 c  = float3(
            dot(l, float3( 0.456100,  0.500484,  0.176381))
          + dot(r, float3(-0.0434706, -0.0879388, -0.00155529)),
            dot(l, float3(-0.0400822, -0.0378246, -0.0157589))
          + dot(r, float3( 0.378476,   0.73364,   -0.0184503)),
            dot(l, float3(-0.0152161, -0.0205971, -0.00546856))
          + dot(r, float3(-0.0721527, -0.112961,   1.2264)));
      return float4(saturate(c), 1.0);
   }

   /* The canvas: even rows from the left half, odd from the right */
   float4 PSInterlaced(PSInput input) : SV_TARGET
   {
      uint w, h;
      float row;
      t0.GetDimensions(w, h);
      row = floor(input.texcoord.y * h);
      return float4(t0.Sample(s0, float2(input.texcoord.x * 0.5
            + 0.5 * fmod(row, 2.0), input.texcoord.y)).rgb, 1.0);
   }
)
