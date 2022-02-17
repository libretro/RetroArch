#version 310 es
precision highp float;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform highp sampler2D Source;

layout(std140, set = 0, binding = 0) uniform UBO
{
	mat4 MVP;
   float contrast;         /* 2.0f;      */ 
   float paper_white_nits; /* 200.0f;    */
   float max_nits;         /* 1000.0f;   */
   float expand_gamut;     /* 1.0f;      */ 
   float inverse_tonemap;
   float hdr10;
} global;

/* Inverse Tonemap section */
#define kMaxNitsFor2084     10000.0f
#define kEpsilon            0.0001f
#define kLumaChannelRatio   0.25f

vec3 InverseTonemap(vec3 sdr)
{
   vec3 hdr;
      
   if(global.inverse_tonemap > 0.0f)
   { 
      sdr = pow(abs(sdr), vec3(global.contrast / 2.2f));       /* Display Gamma - needs to be determined by calibration screen */

      float luma = dot(sdr, vec3(0.2126, 0.7152, 0.0722));  /* Rec BT.709 luma coefficients - https://en.wikipedia.org/wiki/Luma_(video) */

      /* Inverse reinhard tonemap */
      float maxValue             = (global.max_nits / global.paper_white_nits) + kEpsilon;
      float elbow                = maxValue / (maxValue - 1.0f);                          
      float offset               = 1.0f - ((0.5f * elbow) / (elbow - 0.5f));              
   
      float hdrLumaInvTonemap    = offset + ((luma * elbow) / (elbow - luma));
      float sdrLumaInvTonemap    = luma / ((1.0f + kEpsilon) - luma);                     /* Convert the srd < 0.5 to 0.0 -> 1.0 range */

      float lumaInvTonemap       = (luma > 0.5f) ? hdrLumaInvTonemap : sdrLumaInvTonemap;
      vec3 perLuma               = sdr / (luma + kEpsilon) * lumaInvTonemap;

      vec3 hdrInvTonemap         = offset + ((sdr * elbow) / (elbow - sdr));         
      vec3 sdrInvTonemap         = sdr / ((1.0f + kEpsilon) - sdr);                       /* Convert the srd < 0.5 to 0.0 -> 1.0 range */

      vec3 perChannel            = vec3(sdr.x > 0.5f ? hdrInvTonemap.x : sdrInvTonemap.x,
                                     sdr.y > 0.5f ? hdrInvTonemap.y : sdrInvTonemap.y,
                                     sdr.z > 0.5f ? hdrInvTonemap.z : sdrInvTonemap.z);

      hdr = mix(perLuma, perChannel, vec3(kLumaChannelRatio));
   }
   else
   {
      hdr = sdr;
   }

   return hdr;
}

/* HDR10 section */
#define kMaxNitsFor2084     10000.0f

const mat3 k709to2020 = mat3 (
   0.6274040f, 0.3292820f, 0.0433136f,
   0.0690970f, 0.9195400f, 0.0113612f,
   0.0163916f, 0.0880132f, 0.8955950f);

/* START Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */
const mat3 kExpanded709to2020 = mat3 (
    0.6274040f, 0.3292820f, 0.0433136f,
    0.0457456,  0.941777,   0.0124772,
   -0.00121055, 0.0176041,  0.983607);

vec3 LinearToST2084(vec3 normalizedLinearValue)
{
   vec3 ST2084 = pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), vec3(0.1593017578f))) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), vec3(0.1593017578f))), vec3(78.84375f));
   return ST2084;  /* Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits */
}
/* END Converted from (Copyright (c) Microsoft Corporation - Licensed under the MIT License.)  https://github.com/microsoft/Xbox-ATG-Samples/tree/master/Kits/ATGTK/HDR */

vec3 Hdr10(vec3 hdr)
{
   vec3 hdr10;

   if(global.hdr10 > 0.0f)
   {
      /* Now convert into HDR10 */
      vec3 rec2020 = hdr * k709to2020;

      if(global.expand_gamut > 0.0f)
      {
         rec2020 = hdr * kExpanded709to2020;
      }

      vec3 linearColour = rec2020 * (global.paper_white_nits / kMaxNitsFor2084);
      hdr10             = LinearToST2084(linearColour);
   }
   else
   {
      hdr10 = hdr;
   }

   return hdr10;
}

void main()
{
   vec4 sdr = texture(Source, vTexCoord);
   vec4 hdr = vec4(InverseTonemap(sdr.rgb), sdr.a);    
   FragColor = vec4(Hdr10(hdr.rgb), hdr.a); 
}
