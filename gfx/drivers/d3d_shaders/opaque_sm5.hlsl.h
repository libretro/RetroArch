
#define SRC(...) #__VA_ARGS__
SRC(
   struct UBO
   {
      float4x4 modelViewProj;
      float2 Outputsize;
      float time;
   };
   uniform UBO global;

   struct PSInput
   {
      float4 position : SV_POSITION;
      float2 texcoord : TEXCOORD0;
      float4 color : COLOR;
   };
   PSInput VSMain(float4 position : POSITION, float2 texcoord : TEXCOORD0, float4 color : COLOR)
   {
      PSInput result;
      result.position = mul(global.modelViewProj, position);
      result.texcoord = texcoord;
      result.color = color;
      return result;
   }
   uniform sampler s0;
   uniform Texture2D <float4> t0;
   float4 PSMain(PSInput input) : SV_TARGET
   {
      return input.color * t0.Sample(s0, input.texcoord);
   };
)
