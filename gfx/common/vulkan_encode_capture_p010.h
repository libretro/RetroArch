/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_CAPTURE_P010_H
#define RARCH_VULKAN_ENCODE_CAPTURE_P010_H
static const char *const vec_p010_direct_source[] =
{
   "#version 450\n"
   "layout(local_size_x=8,local_size_y=8) in;\n"
   "layout(set=0,binding=0) uniform sampler2D source_image;\n"
   "layout(set=0,binding=1,r16) writeonly uniform image2D y_plane;\n"
   "layout(set=0,binding=2,rg16) writeonly uniform image2D uv_plane;\n"
   "layout(push_constant) uniform Capture {\n"
   "   uint width;\n"
   "   uint height;\n"
   "   uint crop_x;\n"
   "   uint crop_y;\n"
   "   uint valid_width;\n"
   "   uint valid_height;\n",

   "   uint limited_range;\n"
   "   uint flags;\n"
   "} capture;\n"
   "vec3 capture_signal(vec3 rgb)\n"
   "{\n"
   "   if ((capture.flags&4u)==0u)\n"
   "      return rgb;\n"
   "   vec3 linear2020=vec3(dot(rgb,vec3(0.6274040,0.3292820,0.0433136)),\n"
   "      dot(rgb,vec3(0.0690970,0.9195400,0.0113612)),\n"
   "      dot(rgb,vec3(0.0163916,0.0880132,0.8955950)))*0.008;\n"
   "   vec3 p=pow(clamp(linear2020,vec3(0.0),vec3(1.0)),vec3(2610.0/16384.0));\n",

   "   return pow((vec3(3424.0/4096.0)+(2413.0/128.0)*p)/\n"
   "      (vec3(1.0)+(2392.0/128.0)*p),vec3(2523.0/32.0));\n"
   "}\n"
   "float capture_luma(vec3 rgb)\n"
   "{\n"
   "   if ((capture.flags&6u)!=0u)\n"
   "      return dot(rgb,vec3(0.2627,0.6780,0.0593));\n"
   "   return dot(rgb,vec3(0.2126,0.7152,0.0722));\n"
   "}\n"
   "vec3 read_rgb(uvec2 pixel)\n"
   "{\n"
   "   if (pixel.x>=capture.valid_width || pixel.y>=capture.valid_height)\n"
   "      return vec3(0.0);\n",

   "   uvec2 source_pixel=pixel+uvec2(capture.crop_x,capture.crop_y);\n"
   "   if ((capture.flags&1u)!=0u)\n"
   "      source_pixel.y=uint(textureSize(source_image,0).y)-1u-source_pixel.y;\n"
   "   return capture_signal(texelFetch(source_image,ivec2(source_pixel),0).rgb);\n"
   "}\n"
   "float write_luma(uvec2 pixel,vec3 rgb)\n"
   "{\n"
   "   float y=capture_luma(rgb);\n"
   "   float value=capture.limited_range!=0u ? 64.0+876.0*y : 1023.0*y;\n",

   "   float output_y=round(clamp(value,0.0,1023.0))*(64.0/65535.0);\n"
   "   imageStore(y_plane,ivec2(pixel),vec4(output_y,0.0,0.0,1.0));\n"
   "   return y;\n"
   "}\n"
   "void main()\n"
   "{\n"
   "   uvec2 uv=gl_GlobalInvocationID.xy;\n"
   "   uvec2 pixel=uv*2u;\n"
   "   if (pixel.x>=capture.width || pixel.y>=capture.height)\n"
   "      return;\n"
   "   vec3 a=read_rgb(pixel);\n"
   "   vec3 b=read_rgb(pixel+uvec2(1u,0u));\n"
   "   vec3 c=read_rgb(pixel+uvec2(0u,1u));\n",

   "   vec3 d=read_rgb(pixel+uvec2(1u,1u));\n"
   "   float ya=write_luma(pixel,a);\n"
   "   float yb=write_luma(pixel+uvec2(1u,0u),b);\n"
   "   float yc=write_luma(pixel+uvec2(0u,1u),c);\n"
   "   float yd=write_luma(pixel+uvec2(1u,1u),d);\n"
   "   vec3 average=(a+b+c+d)*0.25;\n"
   "   float luma=(ya+yb+yc+yd)*0.25;\n"
   "   vec2 divisor=vec2(1.8556,1.5748);\n"
   "   if ((capture.flags&6u)!=0u)\n"
   "      divisor=vec2(1.8814,1.4746);\n",

   "   vec2 chroma=vec2(average.b-luma,average.r-luma)/divisor;\n"
   "   float scale=capture.limited_range!=0u ? 896.0 : 1023.0;\n"
   "   vec2 output_uv=round(clamp(vec2(512.0)+chroma*scale,0.0,1023.0))*(64.0/65535.0);\n"
   "   imageStore(uv_plane,ivec2(uv),vec4(output_uv,0.0,1.0));\n"
   "}\n"
};

static const char *const vec_p010_buffer_source[] =
{
   "#version 450\n"
   "layout(local_size_x=8,local_size_y=8) in;\n"
   "layout(set=0,binding=0) uniform sampler2D source_image;\n"
   "layout(set=0,binding=1,std430) writeonly buffer Output { uint words[]; } output_data;\n"
   "layout(push_constant) uniform Capture {\n"
   "   uint width;\n"
   "   uint height;\n"
   "   uint crop_x;\n"
   "   uint crop_y;\n"
   "   uint valid_width;\n"
   "   uint valid_height;\n"
   "   uint limited_range;\n"
   "   uint flags;\n"
   "} capture;\n",

   "vec3 capture_signal(vec3 rgb)\n"
   "{\n"
   "   if ((capture.flags&4u)==0u)\n"
   "      return rgb;\n"
   "   vec3 linear2020=vec3(dot(rgb,vec3(0.6274040,0.3292820,0.0433136)),\n"
   "      dot(rgb,vec3(0.0690970,0.9195400,0.0113612)),\n"
   "      dot(rgb,vec3(0.0163916,0.0880132,0.8955950)))*0.008;\n"
   "   vec3 p=pow(clamp(linear2020,vec3(0.0),vec3(1.0)),vec3(2610.0/16384.0));\n"
   "   return pow((vec3(3424.0/4096.0)+(2413.0/128.0)*p)/\n",

   "      (vec3(1.0)+(2392.0/128.0)*p),vec3(2523.0/32.0));\n"
   "}\n"
   "float capture_luma(vec3 rgb)\n"
   "{\n"
   "   if ((capture.flags&6u)!=0u)\n"
   "      return dot(rgb,vec3(0.2627,0.6780,0.0593));\n"
   "   return dot(rgb,vec3(0.2126,0.7152,0.0722));\n"
   "}\n"
   "vec3 read_rgb(uvec2 pixel)\n"
   "{\n"
   "   if (pixel.x>=capture.valid_width || pixel.y>=capture.valid_height)\n"
   "      return vec3(0.0);\n",

   "   uvec2 source_pixel=pixel+uvec2(capture.crop_x,capture.crop_y);\n"
   "   if ((capture.flags&1u)!=0u)\n"
   "      source_pixel.y=uint(textureSize(source_image,0).y)-1u-source_pixel.y;\n"
   "   return capture_signal(texelFetch(source_image,ivec2(source_pixel),0).rgb);\n"
   "}\n"
   "uint y_sample(float y)\n"
   "{\n"
   "   float value=capture.limited_range!=0u ? 64.0+876.0*y : 1023.0*y;\n"
   "   return uint(round(clamp(value,0.0,1023.0)))<<6u;\n"
   "}\n",

   "uvec3 convert_block(uvec2 pixel)\n"
   "{\n"
   "   vec3 a=read_rgb(pixel);\n"
   "   vec3 b=read_rgb(pixel+uvec2(1u,0u));\n"
   "   vec3 c=read_rgb(pixel+uvec2(0u,1u));\n"
   "   vec3 d=read_rgb(pixel+uvec2(1u,1u));\n"
   "   float ya=capture_luma(a);\n"
   "   float yb=capture_luma(b);\n"
   "   float yc=capture_luma(c);\n"
   "   float yd=capture_luma(d);\n"
   "   vec3 average=(a+b+c+d)*0.25;\n"
   "   float luma=(ya+yb+yc+yd)*0.25;\n",

   "   vec2 divisor=vec2(1.8556,1.5748);\n"
   "   if ((capture.flags&6u)!=0u)\n"
   "      divisor=vec2(1.8814,1.4746);\n"
   "   vec2 chroma=vec2(average.b-luma,average.r-luma)/divisor;\n"
   "   float scale=capture.limited_range!=0u ? 896.0 : 1023.0;\n"
   "   uvec2 uv=uvec2(round(clamp(vec2(512.0)+chroma*scale,0.0,1023.0)));\n"
   "   uv.x=uv.x<<6u;\n"
   "   uv.y=uv.y<<6u;\n",

   "   return uvec3(y_sample(ya)|(y_sample(yb)<<16u),y_sample(yc)|(y_sample(yd)<<16u),uv.x|(uv.y<<16u));\n"
   "}\n"
   "void main()\n"
   "{\n"
   "   uvec2 pixel=gl_GlobalInvocationID.xy*uvec2(2u,2u);\n"
   "   if (pixel.x>=capture.width || pixel.y>=capture.height)\n"
   "      return;\n"
   "   uvec3 a=convert_block(pixel);\n"
   "   output_data.words[(pixel.y*capture.width+pixel.x)/2u]=a.x;\n",

   "   output_data.words[((pixel.y+1u)*capture.width+pixel.x)/2u]=a.y;\n"
   "   output_data.words[(capture.width*capture.height+(pixel.y/2u)*capture.width+pixel.x)/2u]=a.z;\n"
   "}\n"
};

#endif
