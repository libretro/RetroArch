
#define SRC(src) #src
SRC(
   struct PSInput
   {
      float4 position : SV_POSITION;
      float2 texcoord : TEXCOORD0;
      float4 color : COLOR;
   };
   uniform float4x4 modelViewProj;
   PSInput VSMain(float4 position : POSITION, float2 texcoord : TEXCOORD0, float4 color : COLOR)
   {
      PSInput result;
      result.position = mul(modelViewProj, position);
      result.texcoord = texcoord;
      result.color = color;
      return result;
   }
   uniform sampler s0;
   uniform Texture2D <float4> t0;
   float4 PSMain(PSInput input) : SV_TARGET
   {
      return input.color * t0.Sample(s0, input.texcoord);
//               return input.color;
   };
)
