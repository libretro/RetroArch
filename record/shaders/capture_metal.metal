/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <metal_stdlib>
using namespace metal;
struct CaptureMetal
{
   uint2 origin, size;
   uint mode, depth;
   uint2 reserved;
};
float3 capture_pq(float3 light)
{
   float3 p=pow(clamp(light,0.0f,1.0f),2610.0f/16384.0f);
   return pow((3424.0f/4096.0f+(2413.0f/128.0f)*p)
         /(1.0f+(2392.0f/128.0f)*p),2523.0f/32.0f);
}
float3 capture_yuv(texture2d<float,access::read> image,
      constant CaptureMetal &p, uint2 at)
{
   float3 rgb=image.read(p.origin+min(at,p.size-uint2(1))).rgb;
   if (p.mode==2)
   {
      float3 wide=float3(dot(rgb,float3(.627404f,.329283f,.043313f)),
            dot(rgb,float3(.069097f,.919540f,.011362f)),
            dot(rgb,float3(.016391f,.088013f,.895595f)));
      rgb=capture_pq(wide*(80.0f/10000.0f));
   }
   rgb=clamp(rgb,0.0f,1.0f);
   float y=dot(rgb,p.mode?float3(.2627f,.6780f,.0593f):float3(.2126f,.7152f,.0722f));
   return float3(y,(rgb.b-y)/(p.mode?1.8814f:1.8556f),(rgb.r-y)/(p.mode?1.4746f:1.5748f));
}
kernel void capture_metal(texture2d<float,access::read> image [[texture(0)]],
      texture2d<float,access::write> luma [[texture(1)]],
      texture2d<float,access::write> chroma [[texture(2)]],
      constant CaptureMetal &p [[buffer(0)]], uint2 id [[thread_position_in_grid]])
{
   uint2 pos=id*2;
   if (any(pos>=uint2(luma.get_width(),luma.get_height()))) return;
   float3 a=capture_yuv(image,p,pos), b=capture_yuv(image,p,pos+uint2(1,0));
   float3 c=capture_yuv(image,p,pos+uint2(0,1)), d=capture_yuv(image,p,pos+uint2(1,1));
   float scale=p.depth==10?876.0f:219.0f;
   float black=p.depth==10?64.0f:16.0f;
   float pack=p.depth==10?64.0f/65535.0f:1.0f/255.0f;
   luma.write(float4(round(black+scale*a.x)*pack,0,0,1),pos);
   luma.write(float4(round(black+scale*b.x)*pack,0,0,1),pos+uint2(1,0));
   luma.write(float4(round(black+scale*c.x)*pack,0,0,1),pos+uint2(0,1));
   luma.write(float4(round(black+scale*d.x)*pack,0,0,1),pos+uint2(1,1));
   /* Encoder pools expose right/bottom extension beyond the active image.
    * Replicate luma pixels and complete chroma samples into that region. */
   uint2 edge=min(pos,p.size-uint2(2));
   if (any(edge!=pos))
   {
      a=capture_yuv(image,p,edge); b=capture_yuv(image,p,edge+uint2(1,0));
      c=capture_yuv(image,p,edge+uint2(0,1)); d=capture_yuv(image,p,edge+uint2(1,1));
   }
   uint left=edge.x?edge.x-1:0;
   float3 e=capture_yuv(image,p,uint2(left,edge.y));
   float3 f=capture_yuv(image,p,uint2(left,edge.y+1));
   float2 uv=(e.yz+f.yz+2.0f*(a.yz+c.yz)+b.yz+d.yz)*.125f;
   float center=p.depth==10?512.0f:128.0f;
   float range=p.depth==10?896.0f:224.0f;
   float ceiling=p.depth==10?1023.0f:255.0f;
   chroma.write(float4(clamp(round(center+range*uv),0.0f,ceiling)*pack,0,1),id);
}
