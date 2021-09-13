
#define SRC(...) #__VA_ARGS__
SRC(
   struct UBO
   {
      float4x4 modelViewProj;
      float contrast;         /* 2.0f;      */ 
      float paper_white_nits; /* 200.0f;    */
      float max_nits;         /* 1000.0f;   */
      float expand_gamut;     /* 1.0f;      */ 
      float inverse_tonemap;
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

   static const float kMaxNitsFor2084     = 10000.0f;  
   static const float kEpsilon            = 0.0001f;
   static const float kLumaChannelRatio   = 0.25f;

   static const float3x3 k709to2020 =
   {
      { 0.6274040f, 0.3292820f, 0.0433136f },
      { 0.0690970f, 0.9195400f, 0.0113612f },
      { 0.0163916f, 0.0880132f, 0.8955950f }
   };

   static const float3x3 kP3to2020 =
   {
      { 0.753845f, 0.198593f, 0.047562f },
      { 0.0457456f, 0.941777f, 0.0124772f },
      { -0.00121055f, 0.0176041f, 0.983607f }
   };

   /* START Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */
   static const float3x3 kExpanded709to2020 =
   {
      { 0.6274040f, 0.3292820f, 0.0433136f },
      { 0.0457456, 0.941777, 0.0124772 },
      { -0.00121055, 0.0176041, 0.983607 }
   };

   float3 LinearToST2084(float3 normalizedLinearValue)
   {
      float3 ST2084 = pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
      return ST2084;  /* Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits */
   }
   /* END Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */

   float3 SRGBToLinear(float3 color)
   {
      float3 scale = color / 12.92f;
      float3 gamma = pow(abs(color + 0.055f) / 1.055f, 2.4f);

      return float3( color.x < 0.04045f ? scale.x : gamma.x, 
                     color.y < 0.04045f ? scale.y : gamma.y,
                     color.z < 0.04045f ? scale.z : gamma.z);
   }

   float4 Hdr(float4 sdr)
   {
      float3 hdr;
      
      if(global.inverse_tonemap)
      {
         sdr.xyz = pow(abs(sdr.xyz), global.contrast / 2.2f );               /* Display Gamma - needs to be determined by calibration screen */

         float luma = dot(sdr.xyz, float3(0.2126, 0.7152, 0.0722));  /* Rec BT.709 luma coefficients - https://en.wikipedia.org/wiki/Luma_(video) */

         /* Inverse reinhard tonemap */
         float maxValue             = (global.max_nits / global.paper_white_nits) + kEpsilon;
         float elbow                = maxValue / (maxValue - 1.0f);                          /* Convert (1.0 + epsilon) to infinite to range 1001 -> 1.0 */ 
         float offset               = 1.0f - ((0.5f * elbow) / (elbow - 0.5f));              /* Convert 1001 to 1.0 to range 0.5 -> 1.0 */
         
         float hdrLumaInvTonemap    = offset + ((luma * elbow) / (elbow - luma));
         float sdrLumaInvTonemap    = luma / ((1.0f + kEpsilon) - luma);                     /* Convert the srd < 0.5 to 0.0 -> 1.0 range */

         float lumaInvTonemap       = (luma > 0.5f) ? hdrLumaInvTonemap : sdrLumaInvTonemap;
         float3 perLuma             = sdr.xyz / (luma + kEpsilon) * lumaInvTonemap;

         float3 hdrInvTonemap       = offset + ((sdr.xyz * elbow) / (elbow - sdr.xyz));         
         float3 sdrInvTonemap       = sdr.xyz / ((1.0f + kEpsilon) - sdr.xyz);               /* Convert the srd < 0.5 to 0.0 -> 1.0 range */

         float3 perChannel          = float3(sdr.x > 0.5f ? hdrInvTonemap.x : sdrInvTonemap.x,
                                             sdr.y > 0.5f ? hdrInvTonemap.y : sdrInvTonemap.y,
                                             sdr.z > 0.5f ? hdrInvTonemap.z : sdrInvTonemap.z);

         hdr = lerp(perLuma, perChannel, kLumaChannelRatio);
      }
      else
      {
         hdr = SRGBToLinear(sdr.xyz);
      }

      /* Now convert into HDR10 */
      float3 rec2020 = mul(k709to2020, hdr);

      if(global.expand_gamut > 0.0f)
      {
         rec2020 = mul( kExpanded709to2020, hdr);
      }

      float3 linearColour  = rec2020 * (global.paper_white_nits / kMaxNitsFor2084);
      float3 hdr10         = LinearToST2084(linearColour);

      return float4(hdr10, sdr.w);
   }

   float4 PSMain(PSInput input) : SV_TARGET
   {
      float4 sdr = input.color * t0.Sample(s0, input.texcoord);
   
      return Hdr(sdr);
   };
)
