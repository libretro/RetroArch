#!/usr/bin/env python3
"""Execute the shipping MSL fragment with portable vector/texture operations."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/metal.m").read_text(encoding="utf-8")
start = source.index('      "fragment float4 views_media(')
end = source.index('      "fragment float4 views_layer(', start)
fragment = "".join(json.loads(line.strip()) for line in source[start:end].splitlines())
fragment = re.sub(r"\s*\[\[[^\]]+\]\]", "", fragment).replace("fragment ", "").replace("constant ", "const ")
fragment = fragment.replace("u.clamp.xy", "float2(u.clamp.rgb.x,u.clamp.rgb.y)").replace("u.clamp.zw", "float2(u.clamp.rgb.z,u.clamp.w)")
fragment = fragment.replace("c[3].x", "c[3].rgb.x").replace(".xyz", ".rgb")
fixture = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include "HDR_ROWS"
struct float2 { float x,y; float2(float a,float b):x(a),y(b){} };
struct float3 { float x,y,z; float3(float a,float b,float c):x(a),y(b),z(c){} };
struct float4 { float3 rgb; float w; float4(float a,float b,float c,float d):rgb(a,b,c),w(d){} float4(float3 a,float d):rgb(a),w(d){} };
static float3 operator-(float3 a,float b) { return float3(a.x-b,a.y-b,a.z-b); }
static float3 operator-(float a,float3 b) { return float3(a-b.x,a-b.y,a-b.z); }
static float3 operator*(float3 a,float b) { return float3(a.x*b,a.y*b,a.z*b); }
static float3 operator*(float a,float3 b) { return b*a; }
static float3 operator/(float3 a,float3 b) { return float3(a.x/b.x,a.y/b.y,a.z/b.z); }
static float3 max(float3 a,float b) { return float3(a.x>b?a.x:b,a.y>b?a.y:b,a.z>b?a.z:b); }
static float3 abs(float3 a) { return float3(std::fabs(a.x),std::fabs(a.y),std::fabs(a.z)); }
static float3 pow(float3 a,float b) { return float3(std::pow(a.x,b),std::pow(a.y,b),std::pow(a.z,b)); }
static float sg(float v) { return v>0?1.0f:v<0?-1.0f:0.0f; }
static float3 sign(float3 a) { return float3(sg(a.x),sg(a.y),sg(a.z)); }
static float3 operator*(float3 a,float3 b) { return float3(a.x*b.x,a.y*b.y,a.z*b.z); }
static float dot(float3 a,float3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static float2 clamp(float2 a,float2 lo,float2 hi)
{ return float2(a.x<lo.x?lo.x:a.x>hi.x?hi.x:a.x,a.y<lo.y?lo.y:a.y>hi.y?hi.y:a.y); }
struct ViewsOut { float2 texcoord; ViewsOut():texcoord(.5f,.5f){} };
struct ViewsUBO { float4 clamp; ViewsUBO():clamp(0,0,1,1){} };
struct sampler {};
template<class T> struct texture2d {
   float4 value;
   texture2d(float4 v):value(v){}
   float4 sample(sampler,float2 p) const { assert(p.x>=0 && p.x<=1 && p.y>=0 && p.y<=1);return value; }
};
FRAGMENT
int main()
{
   const double colors[8][3]={{0,0,0},{1,1,1},{1,0,0},{0,1,0},{0,0,1},{0,1,1},{1,0,1},{.23,.58,.91}};
   const float white[4]={20,80,203,1000};
   const double to2020[4][9]={
      {.627404,.329282,.0433136,.069097,.91954,.0113612,.0163916,.0880132,.895595},
      {.627404,.329282,.0433136,.0457456,.941777,.0124772,-.00121055,.0176041,.983607},
      {.753833,.198597,.047570,.045744,.941777,.012479,-.001210,.017602,.983609},
      {1,0,0,0,1,0,0,0,1}};
   const double from2020[9]={1.660491,-.5876411,-.0728499,-.1245505,1.1328999,-.0083494,-.0181508,-.1005789,1.1187297};
   unsigned failures=0;
   for(unsigned mode=1;mode<=2;mode++)
   for(unsigned gamut=0;gamut<4;gamut++)
   for(unsigned w=0;w<4;w++)
   for(unsigned pixel=0;pixel<8;pixel++)
   {
      float rows[12];double expected[3]={0,0,0},input[3];
      float4 c[4]={float4(0,0,0,0),float4(0,0,0,0),float4(0,0,0,0),float4((float)mode,0,0,0)};
      hdr_viewer_colour_rows(rows,true,white[w],gamut);
      for(unsigned i=0;i<3;i++)
      {
         c[i]=float4(rows[4*i],rows[4*i+1],rows[4*i+2],rows[4*i+3]);
         for(unsigned j=0;j<3;j++) expected[i]+=from2020[3*i+j]*colors[pixel][j]*12.5;
         double p=std::pow(colors[pixel][i]*.1,2610.0/16384.0);
         input[i]=mode==1 ? std::pow((3424.0/4096.0+2413.0/128.0*p)/(1+2392.0/128.0*p),2523.0/32.0) : expected[i];
      }
      float alpha=(float)(pixel%4+1)/4;
      texture2d<float> tex(float4((float)input[0],(float)input[1],(float)input[2],alpha));
      float4 v=views_media(ViewsOut(),ViewsUBO(),c,tex,sampler());
      double encoded[3]={v.rgb.x,v.rgb.y,v.rgb.z},linear[3],rgb2020[3]={0,0,0};
      for(unsigned i=0;i<3;i++) linear[i]=(encoded[i]<0?-1.0:1.0)*std::pow(std::fabs(encoded[i]),2.4);
      for(unsigned i=0;i<3;i++) for(unsigned j=0;j<3;j++) rgb2020[i]+=to2020[gamut][3*i+j]*linear[j]*white[w]/80;
      for(unsigned i=0;i<3;i++)
      {
         double actual=0;
         for(unsigned j=0;j<3;j++) actual+=from2020[3*i+j]*rgb2020[j];
         if(!std::isfinite(actual) || std::fabs(actual-expected[i])>.003+.003*std::fabs(expected[i])) failures++;
      }
      assert(v.w==1);
   }
   float4 c[4]={float4(0,0,0,0),float4(0,0,0,0),float4(0,0,0,0),float4(0,0,0,0)};
   float4 v=views_media(ViewsOut(),ViewsUBO(),c,texture2d<float>(float4(-.2f,.5f,1.5f,.25f)),sampler());
   assert(v.rgb.x==-.2f && v.rgb.y==.5f && v.rgb.z==1.5f && v.w==1);
   std::printf("Metal viewer fragment: 256 linear/PQ vectors, all gamuts/white levels, opaque alpha and SDR, %u failures\n",failures);
   return failures?1:0;
}
'''.replace("HDR_ROWS", (root / "gfx/common/hdr_viewer_colour.h").as_posix()).replace("FRAGMENT", fragment)
cxx = shlex.split(os.environ.get("CXX", "c++"))
with tempfile.TemporaryDirectory(prefix="metal-viewer-pixels-") as directory:
    if "--native" in sys.argv:
        begin = source.index('   static NSString *src = @', source.index('static void metal_views_init('))
        end = source.index('   NSError *err      = nil;', begin)
        metal = "".join(json.loads(line.strip().removesuffix(";"))
            for line in source[begin:end].splitlines()[1:] if line.strip())
        shader = Path(directory) / "views.metal"
        shader.write_bytes(metal.encode("utf-8"))
        native = Path(directory) / "native"
        subprocess.run(shlex.split(os.environ.get("OBJC", "clang")) + [
            "-fobjc-arc", "-I" + str(root / "gfx/common"),
            str(Path(__file__).with_name("metal_viewer_native.m")),
            "-o", str(native), "-framework", "Foundation", "-framework", "Metal"], check=True)
        result = subprocess.run([str(native), str(shader)])
        if result.returncode == 77:
            print("SKIP native Metal viewer: no Metal device")
        else:
            result.check_returncode()
    unit = Path(directory) / "test.cpp"
    exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in " ".join(cxx) else "test")
    unit.write_bytes(fixture.encode("utf-8"))
    subprocess.run(cxx + shlex.split(os.environ.get("CXXFLAGS", "")) + [
        "-std=c++11", "-Wall", "-Wextra", "-Werror", str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
