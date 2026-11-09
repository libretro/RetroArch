/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Plane shader acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gfx/shaders/rslc.h>
static unsigned instruction_count(const struct rslc_result *result,
                                  unsigned opcode, unsigned operand,
                                  unsigned value)
{
   size_t offset = 5;
   unsigned count = 0;
   assert(result->spirv_len >= 5 && result->spirv[0] == 0x07230203);
   while (offset < result->spirv_len)
   {
      uint32_t word = result->spirv[offset];
      size_t length = word >> 16;
      assert(length && length <= result->spirv_len - offset);
      if ((word & 65535) == opcode)
      {
         assert(length > operand);
         if (result->spirv[offset + operand] == value)
            count++;
      }
      offset += length;
   }
   assert(offset == result->spirv_len);
   return count;
}
static void save(const char *prefix, const char *suffix, const void *data,
                 size_t size)
{
   size_t length = strlen(prefix) + strlen(suffix) + 1;
   char *name = (char *)malloc(length);
   FILE *output;
   assert(name);
   strcpy(name, prefix);
   strcat(name, suffix);
   output = fopen(name, "wb");
   free(name);
   assert(output);
   assert(fwrite(data, 1, size, output) == size && !fclose(output));
}
int main(int argc, char **argv)
{
   static const char ordinary[] =
       "#version 450\nlayout(local_size_x=1) in;\n"
       "layout(rgba8,set=0,binding=0) writeonly uniform image2D output_image;\n"
       "void main(){imageStore(output_image,ivec2(0),vec4(1.0));}\n";
   static const char vertex[] =
       "#version 450\nvoid main(){gl_Position=vec4(0.0);}\n";
   struct rslc_opts opts;
   struct rslc_result result, before;
   static const char source[] =
       "#version 450\nlayout(local_size_x=1) in;\n"
       "layout(r8,set=0,binding=0) writeonly uniform image2D y_plane;\n"
       "layout(rg8,set=0,binding=1) writeonly uniform image2D uv_plane;\n"
       "void main(){imageStore(y_plane,ivec2(0),vec4(0.5));"
       "imageStore(uv_plane,ivec2(0),vec4(0.25,0.75,0.0,1.0));}\n";
   char error[256];
   assert(argc == 1 || argc == 2);
   memset(&opts, 0, sizeof(opts));
   memset(&result, 0, sizeof(result));
   opts.dst = RSLC_DST_SPIRV;
   opts.version = 450;
   opts.err = error;
   opts.err_cap = sizeof(error);
   assert(rslc_compile(source, sizeof(source) - 1, RSLC_STAGE_COMPUTE, &opts,
                       &result));
   assert(instruction_count(&result, 17, 1, 49) == 1);
   assert(instruction_count(&result, 25, 8, 15) == 1);
   assert(instruction_count(&result, 25, 8, 13) == 1);
   if (argc == 2)
      save(argv[1], ".spv", result.spirv, result.spirv_len * sizeof(uint32_t));
   rslc_result_free(&result, &opts);
   opts.dst = RSLC_DST_GLSL;
   assert(rslc_compile(source, sizeof(source) - 1, RSLC_STAGE_COMPUTE, &opts,
                       &result));
   assert(result.text && strstr(result.text, "r8") &&
          strstr(result.text, "rg8"));
   if (argc == 2)
      save(argv[1], ".glsl", result.text, result.text_len);
   rslc_result_free(&result, &opts);
   opts.dst = RSLC_DST_GLSL_ES;
   opts.version = 310;
   assert(!rslc_compile(source, sizeof(source) - 1, RSLC_STAGE_COMPUTE,
         &opts, &result));
   assert(strstr(error, "no r8 or rg8 storage image format"));
   opts.dst = RSLC_DST_HLSL;
   opts.version = 50;
   assert(!rslc_compile(source, sizeof(source) - 1, RSLC_STAGE_COMPUTE,
         &opts, &result));
   assert(strstr(error, "only four-component storage images"));
   opts.dst = RSLC_DST_MSL;
   opts.version = 23;
   assert(rslc_compile(source, sizeof(source) - 1, RSLC_STAGE_COMPUTE,
         &opts, &result));
   assert(strstr(result.text, "texture2d<float, access::write>"));
   rslc_result_free(&result, &opts);
   opts.dst = RSLC_DST_SPIRV;
   opts.version = 450;
   assert(rslc_compile(ordinary, sizeof(ordinary) - 1, RSLC_STAGE_COMPUTE,
                       &opts, &result));
   assert(!instruction_count(&result, 17, 1, 49) &&
          instruction_count(&result, 25, 8, 4) == 1);
   rslc_result_free(&result, &opts);
   memset(&result, 0xa5, sizeof(result));
   memcpy(&before, &result, sizeof(before));
   opts.dst = RSLC_DST_DXBC;
   opts.version = 50;
   assert(!rslc_compile(vertex, sizeof(vertex) - 1, RSLC_STAGE_VERTEX, &opts,
                        &result));
   assert(strstr(error, "without the Direct3D bytecode backend") &&
          !memcmp(&before, &result, sizeof(result)));
   puts(
       "RSLC plane formats/capability/GLSL and optional bytecode backend PASS");
   return 0;
}
