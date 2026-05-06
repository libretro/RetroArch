
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

)
