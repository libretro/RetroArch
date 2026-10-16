float video_hdr_hable(float x)
{
   return ((x * (0.15 * x + 0.05) + 0.004)
         / (x * (0.15 * x + 0.50) + 0.06)) - 0.02 / 0.30;
}

float video_hdr_light(float x, float transfer)
{
   float light;
   x = clamp(x, 0.0, 1.0);
   if (transfer == 16.0)
   {
      float ep = pow(x, 1.0 / 78.84375);
      light = pow(max(ep - 0.8359375, 0.0) / (18.8515625 - 18.6875 * ep),
            1.0 / 0.1593017578125) * (10000.0 / 203.0);
   }
   else
   {
      float scene;
      if (x <= 0.5)
         scene = x * x / 3.0;
      else
         scene = (exp((x - 0.55991073) / 0.17883277) + 0.28466892) / 12.0;
      light = pow(scene, 1.2) * (1000.0 / 203.0);
   }
   return light;
}

float video_hdr_srgb(float x)
{
   x = clamp(x, 0.0, 1.0);
   if (x <= 0.0031308)
      return 12.92 * x;
   return 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

float video_hdr_channel(float x, float4 hdr)
{
   return clamp(video_hdr_hable(hdr.z * video_hdr_light(x, hdr.x)) / hdr.w,
         0.0, 1.0);
}

float video_hdr_linear_srgb(float x)
{
   x = clamp(x, 0.0, 1.0);
   if (x <= 0.04045)
      return x / 12.92;
   return pow((x + 0.055) / 1.055, 2.4);
}

float3 video_hdr_convert(float3 rgb, float4 hdr)
{
   if (hdr.x != 16.0 && hdr.x != 18.0)
   {
      if (hdr.w < 0.0)
         return float3(video_hdr_linear_srgb(rgb.r),
               video_hdr_linear_srgb(rgb.g), video_hdr_linear_srgb(rgb.b));
      return clamp(rgb, 0.0, 1.0);
   }
   if (hdr.w < 0.0)
      rgb = float3(video_hdr_light(rgb.r, hdr.x), video_hdr_light(rgb.g, hdr.x),
            video_hdr_light(rgb.b, hdr.x)) * (203.0 / 80.0);
   else
      rgb = float3(video_hdr_channel(rgb.r, hdr), video_hdr_channel(rgb.g, hdr),
            video_hdr_channel(rgb.b, hdr));
   if (hdr.y != 0.0)
      rgb = float3(dot(rgb, float3(6801.0, -2407.0, -298.0)),
            dot(rgb, float3(-510.0, 4640.0, -34.0)),
            dot(rgb, float3(-75.0, -412.0, 4583.0))) / 4096.0;
   if (hdr.w < 0.0)
      return rgb;
   return float3(video_hdr_srgb(rgb.r), video_hdr_srgb(rgb.g), video_hdr_srgb(rgb.b));
}
