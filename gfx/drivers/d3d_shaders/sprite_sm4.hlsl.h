
#define SRC(...) #__VA_ARGS__
SRC(

      struct VSInput
      {
         float4 position : POSITION;
         float4 texcoord : TEXCOORD0;
         float4 color0 : COLOR0;
         float4 color1 : COLOR1;
         float4 color2 : COLOR2;
         float4 color3 : COLOR3;
         float2 params : PARAMS;
      };
      struct GSInput
      {
         float4 position : POSITION;
         float4 texcoord : TEXCOORD0;
         float4 color0 : COLOR0;
         float4 color1 : COLOR1;
         float4 color2 : COLOR2;
         float4 color3 : COLOR3;
      };
      struct PSInput
      {
         float4 position : SV_POSITION;
         float2 texcoord : TEXCOORD0;
         float4 color : COLOR;
      };

      GSInput VSMain(VSInput input)
      {
         GSInput output;
         output.position = input.position * 2.0;
         output.position.xy += output.position.zw * (1.0 - input.params.x) * 0.5f;
         output.position.zw *= input.params.x;
         output.position.w *= -1.0;
         output.position.x = output.position.x - 1.0;
         output.position.y = 1.0 - output.position.y;
         output.texcoord = input.texcoord;
         output.color0 = input.color0;
         output.color1 = input.color1;
         output.color2 = input.color2;
         output.color3 = input.color3;
         return output;
      }

      [maxvertexcount(4)]
      void GSMain(point GSInput input[1], inout TriangleStream<PSInput> triStream)
      {
         PSInput output;
         output.position.zw = float2(0.0f, 1.0f);

         output.position.xy = input[0].position.xy +  input[0].position.zw * float2(1.0, 0.0);
         output.texcoord = input[0].texcoord.xy + input[0].texcoord.zw * float2(1.0, 0.0);
         output.color = input[0].color0;
         triStream.Append(output);

         output.position.xy = input[0].position.xy +  input[0].position.zw * float2(0.0, 0.0);
         output.texcoord = input[0].texcoord.xy + input[0].texcoord.zw * float2(0.0, 0.0);
         output.color = input[0].color1;
         triStream.Append(output);

         output.position.xy = input[0].position.xy +  input[0].position.zw * float2(1.0, 1.0);
         output.texcoord = input[0].texcoord.xy + input[0].texcoord.zw * float2(1.0, 1.0);
         output.color = input[0].color2;
         triStream.Append(output);

         output.position.xy = input[0].position.xy +  input[0].position.zw * float2(0.0, 1.0);
         output.texcoord = input[0].texcoord.xy + input[0].texcoord.zw * float2(0.0, 1.0);
         output.color = input[0].color3;
         triStream.Append(output);
      }

      uniform sampler s0;
      uniform Texture2D <float4> t0;
      float4 PSMain(PSInput input) : SV_TARGET
      {
         return input.color * t0.Sample(s0, input.texcoord);
      };
      float4 PSMainA8(PSInput input) : SV_TARGET
      {
         return float4(input.color.rgb  , input.color.a * t0.Sample(s0, input.texcoord).a);
      };

)
