
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
         float2 params : PARAMS;
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
         /* params.x is consumed in the VS (scaling, above);
          * forward params.y (rotation, in radians) to the GS. */
         output.params = input.params;
         return output;
      }

      [maxvertexcount(4)]
      void GSMain(point GSInput input[1], inout TriangleStream<PSInput> triStream)
      {
         /* Apply rotation around the sprite centre.
          *
          * input[0].position.xy = top-left corner of the quad (clip space)
          * input[0].position.zw = quad size               (clip space)
          *
          * The four corners are at `position.xy + position.zw * c`
          * with `c` in {(0,0), (1,0), (0,1), (1,1)}.  We rewrite as
          * `centre + position.zw * (c - 0.5)` and rotate the
          * `(c - 0.5)`-scaled offset by `params.y` radians around
          * the centre.  The rotation is computed in pixel space and
          * then mapped back to clip space via the per-axis `.zw`
          * sizes; this keeps a square pixel-space icon square on
          * screen even on a non-square viewport, because position.z
          * and position.w both carry the same pixel-space length W
          * (z = 2W/vw, w = 2W/vh).  Non-square icons would skew, but
          * the only caller of rotation today (the task-message
          * hourglass) draws a square. */
         PSInput output;
         float  cosT = cos(input[0].params.y);
         float  sinT = sin(input[0].params.y);
         float2 centre = input[0].position.xy + input[0].position.zw * 0.5;

         /* Corner sign ordering matches the four triStream.Append
          * calls below; (sx, sy) ∈ {±1} are the corner-relative-to-
          * centre signs in pixel space. */
         float2 corner_signs[4] = {
            float2( 1.0, -1.0),  /* top-right    -> color0 */
            float2(-1.0, -1.0),  /* top-left     -> color1 */
            float2( 1.0,  1.0),  /* bottom-right -> color2 */
            float2(-1.0,  1.0),  /* bottom-left  -> color3 */
         };
         float2 corner_uv[4] = {
            float2(1.0, 0.0),
            float2(0.0, 0.0),
            float2(1.0, 1.0),
            float2(0.0, 1.0),
         };
         float4 corner_colour[4] = {
            input[0].color0, input[0].color1,
            input[0].color2, input[0].color3,
         };

         output.position.zw = float2(0.0f, 1.0f);

         [unroll]
         for (int i = 0; i < 4; i++)
         {
            float sx = corner_signs[i].x;
            float sy = corner_signs[i].y;
            /* Rotate the half-extent corner offset, then scale back
             * by the per-axis clip-space half-size.  See block
             * comment above for why this preserves square icons. */
            float dx = (sx * cosT - sy * sinT) * input[0].position.z * 0.5;
            float dy = (sx * sinT + sy * cosT) * input[0].position.w * 0.5;
            output.position.xy = centre + float2(dx, dy);
            output.texcoord    = input[0].texcoord.xy
               + input[0].texcoord.zw * corner_uv[i];
            output.color       = corner_colour[i];
            triStream.Append(output);
         }
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

      /* ---- HDR-aware sprite entries ----------------------------------
       * Used for UI (widgets / OSD text) drawn DIRECTLY into an HDR
       * swapchain, i.e. when the back-buffer composite pass is not
       * active.  The math replicates the SDR-source branches of the
       * hdr_sm5 composite shader exactly, so notifications rendered
       * this way match the appearance of the same UI when it goes
       * through the menu composite.  All of it is SM4-clean ALU.
       *
       * sprite_hdr_mode: 0 = passthrough (identical to PSMain /
       * PSMainA8; used while the composite pass will encode later),
       * 1 = HDR10 (PQ, Rec.2020), 2 = scRGB (linear, 1.0 = 80 nits).
       */
      cbuffer SpriteHDR : register(b1)
      {
         float sprite_hdr_mode;
         float sprite_paper_white_nits;
         float sprite_expand_gamut;
         float sprite_hdr_pad;
      };

      static const float kSprMaxNitsFor2084 = 10000.0f;
      static const float kSprscRGBWhiteNits = 80.0f;

      static const float3x3 kSpr709to2020 =
      {
         { 0.6274040f, 0.3292820f, 0.0433136f },
         { 0.0690970f, 0.9195400f, 0.0113612f },
         { 0.0163916f, 0.0880132f, 0.8955950f }
      };
      static const float3x3 kSprExpanded709to2020 =
      {
         { 0.6274040f,  0.3292820f, 0.0433136f },
         { 0.0457456f,  0.9417770f, 0.0124772f },
         {-0.00121055f, 0.0176041f, 0.9836070f }
      };
      static const float3x3 kSprP3to2020 =
      {
         { 0.753833f,  0.198597f,  0.047570f },
         { 0.045744f,  0.941777f,  0.012479f },
         {-0.001210f,  0.017602f,  0.983609f }
      };
      static const float3x3 kSpr2020to709 =
      {
         {  1.6604910f, -0.5876411f, -0.0728499f },
         { -0.1245505f,  1.1328999f, -0.0083494f },
         { -0.0181508f, -0.1005789f,  1.1187297f }
      };

      float3 SpriteLinearToST2084(float3 normalizedLinearValue)
      {
         return pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
      }

      float3 SpriteTo2020(const float3 rgb)
      {
         float3 result;
         if (sprite_expand_gamut < 0.5f)
            result = mul(kSpr709to2020, rgb);
         else if (sprite_expand_gamut < 1.5f)
            result = mul(kSprExpanded709to2020, rgb);
         else if (sprite_expand_gamut < 2.5f)
            result = mul(kSprP3to2020, rgb);
         else
            result = rgb;
         return max(result, float3(0.0f, 0.0f, 0.0f));
      }

      float4 SpriteEncodeHDR(float4 sdr)
      {
         if (sprite_hdr_mode > 1.5f)
         {
            /* scRGB: mirror of the composite's mode-2 SDR branch */
            float3 linear_col = SpriteTo2020(pow(abs(sdr.rgb), 2.4f));
            linear_col = mul(kSpr2020to709, linear_col);
            linear_col *= sprite_paper_white_nits / kSprscRGBWhiteNits;
            return float4(linear_col, sdr.a);
         }
         else if (sprite_hdr_mode > 0.5f)
         {
            /* HDR10: mirror of the composite's hdr10 SDR branch */
            float3 pq_input = SpriteTo2020(sdr.rgb)
                  * (sprite_paper_white_nits / kSprMaxNitsFor2084);
            return float4(SpriteLinearToST2084(max(pq_input,
                  float3(0.0f, 0.0f, 0.0f))), sdr.a);
         }
         return sdr; /* mode 0: passthrough */
      }

      float4 PSMainHDR(PSInput input) : SV_TARGET
      {
         return SpriteEncodeHDR(input.color * t0.Sample(s0, input.texcoord));
      };
      float4 PSMainA8HDR(PSInput input) : SV_TARGET
      {
         return SpriteEncodeHDR(float4(input.color.rgb,
               input.color.a * t0.Sample(s0, input.texcoord).a));
      };
      /* ---- Linear scRGB sources ---------------------------------------
       * An RGBA16F texture is HDR content in linear scRGB: 1.0 is 80
       * nits, the 709 primaries. Mode 0 writes it into the UI layer as
       * the value the menu composite's SDR branch takes back onto it:
       * that branch shows To2020(v^2.4) * paper_white nits, scRGB and
       * HDR10 alike, so v is the exact inverse at sprite_paper_white_nits
       * (the menu nits), above 1.0 for anything brighter than menu white
       * - the layer is FP16 once such a texture is drawn. Modes 2 and 1,
       * straight into the swapchain, are scRGB as it is and absolute PQ.
       * The composite's pow(abs()) keeps no sign, so in mode 0 a colour
       * outside the chosen gamut is clamped to it. */
      static const float3x3 kSprExpanded2020to709 =
      {
         { 1.635346f,  -0.5705700f, -0.0647755f },
         {-0.0794803f,  1.0898049f, -0.0103244f },
         { 0.0034352f, -0.0202070f,  1.0167713f }
      };
      static const float3x3 kSpr2020toP3 =
      {
         { 1.3435784f, -0.2821792f, -0.0613991f },
         {-0.0652977f,  1.0757882f, -0.0104905f },
         { 0.0028213f, -0.0195987f,  1.0167763f }
      };

      /* The inverse of SpriteTo2020 for the same sprite_expand_gamut. */
      float3 SpriteFrom2020(const float3 rgb)
      {
         if (sprite_expand_gamut < 0.5f)
            return mul(kSpr2020to709, rgb);
         else if (sprite_expand_gamut < 1.5f)
            return mul(kSprExpanded2020to709, rgb);
         else if (sprite_expand_gamut < 2.5f)
            return mul(kSpr2020toP3, rgb);
         return rgb;
      }

      float4 PSMainLinearHDR(PSInput input) : SV_TARGET
      {
         float4 s   = t0.Sample(s0, input.texcoord);
         float3 lin = input.color.rgb * s.rgb;
         float  a   = input.color.a * s.a;
         if (sprite_hdr_mode > 1.5f)
            return float4(lin, a);
         if (sprite_hdr_mode > 0.5f)
            return float4(SpriteLinearToST2084(max(mul(kSpr709to2020, lin)
                  * (kSprscRGBWhiteNits / kSprMaxNitsFor2084), 0.0f)), a);
         {
            float3 x = SpriteFrom2020(mul(kSpr709to2020, lin
                  * (kSprscRGBWhiteNits / max(sprite_paper_white_nits, 1.0f))));
            return float4(pow(max(x, 0.0f), 1.0f / 2.4f), a);
         }
      };

      /* 16-bit coverage font atlas (R16_UNORM): D3D11 SRVs have no
       * component swizzle, so the wider atlas is sampled from .r
       * explicitly. Same encode / mode-0 passthrough behavior as
       * PSMainA8HDR. */
      float4 PSMainA16HDR(PSInput input) : SV_TARGET
      {
         return SpriteEncodeHDR(float4(input.color.rgb,
               input.color.a * t0.Sample(s0, input.texcoord).r));
      };

)
