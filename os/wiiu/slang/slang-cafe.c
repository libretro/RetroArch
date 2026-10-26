/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (slang-cafe.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/* Splits a .slang into the two GLSL files Nintendo's CafeGLSL compiler
 * takes, as the Wii U lane always has: includes expanded, the stage
 * selected by '#pragma stage', #version 150, the uniform block and the
 * push block as the two uniform blocks the GX2 driver fills by member
 * name, a location on every in and out so the fetch shader's streams
 * land, samplers as plain uniforms under their source names.  The text
 * is rslc's print of its IR (RSLC_DST_GLSL_CAFE), so what the Wii U
 * compiles is what every other driver compiles; the include expansion
 * and the stage split are RetroArch's own (glslang_util.c).
 *
 *   slang-cafe <in.slang> <out.vert> <out.frag>
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gfx/shaders/rslc.h>
#include "gfx/drivers_shader/glslang_util.h"

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void RARCH_DBG(const char *fmt, ...)
{
   (void)fmt;
}

/* ---- the source hooks glslang_util.c expands includes through ---- */

bool video_shader_source_read(const char *ident, char **buf, int64_t *len)
{
   FILE *f;
   long  n;
   char *data;
   if (!ident || !*ident || !buf)
      return false;
   *buf = NULL;
   if (len)
      *len = 0;
   if (!(f = fopen(ident, "rb")))
      return false;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (n <= 0 || !(data = (char*)malloc((size_t)n + 1)))
   {
      fclose(f);
      return false;
   }
   if (fread(data, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(data);
      return false;
   }
   fclose(f);
   data[n] = '\0';
   *buf    = data;
   if (len)
      *len = n;
   return true;
}

/* name relative to parent's directory; an absolute name stands */
bool video_shader_source_resolve(const char *parent, const char *name,
      char *s, size_t len)
{
   const char *slash;
   size_t      dir;
   if (!name || !*name || !s || !len)
      return false;
   s[0] = '\0';
   if (!parent || !*parent || name[0] == '/' || name[0] == '\\'
         || (name[0] && name[1] == ':'))
   {
      if (strlen(name) >= len)
         return false;
      strcpy(s, name);
      return true;
   }
   slash = strrchr(parent, '/');
   {
      const char *bs = strrchr(parent, '\\');
      if (bs && (!slash || bs > slash))
         slash = bs;
   }
   dir = slash ? (size_t)(slash - parent) + 1 : 0;
   if (dir + strlen(name) >= len)
      return false;
   memcpy(s, parent, dir);
   strcpy(s + dir, name);
   return true;
}

const char *video_shader_source_ident_name(const char *ident)
{
   const char *slash;
   if (!ident || !*ident)
      return NULL;
   slash = strrchr(ident, '/');
   {
      const char *bs = strrchr(ident, '\\');
      if (bs && (!slash || bs > slash))
         slash = bs;
   }
   return slash ? slash + 1 : ident;
}

bool video_shader_source_ident_is_slang(const char *ident)
{
   size_t n = ident ? strlen(ident) : 0;
   return n > 6 && !strcmp(ident + n - 6, ".slang");
}

/* ---- the tool ---- */

static int print_stage(const char *source, enum rslc_stage stage,
      const char *out_path, const char *in_path)
{
   struct rslc_opts   opts;
   struct rslc_result res;
   char               err[256];
   FILE              *f;
   int                ok;

   memset(&opts, 0, sizeof(opts));
   opts.dst     = RSLC_DST_GLSL_CAFE;
   opts.err     = err;
   opts.err_cap = sizeof(err);
   err[0]       = '\0';
   if (!rslc_compile(source, strlen(source), stage, &opts, &res))
   {
      fprintf(stderr, "%s: %s: %s\n", in_path,
            stage == RSLC_STAGE_VERTEX ? "vertex" : "fragment", err);
      return 1;
   }
   f  = fopen(out_path, "wb");
   ok = f && fwrite(res.text, 1, res.text_len, f) == res.text_len;
   if (f)
      fclose(f);
   rslc_result_free(&res, &opts);
   if (!ok)
   {
      fprintf(stderr, "cannot write %s\n", out_path);
      return 1;
   }
   return 0;
}

int main(int argc, char **argv)
{
   struct shader_line_buf lines;
   char *vs, *fs;
   int   rc;

   if (argc != 4)
   {
      fprintf(stderr, "usage: slang-cafe <in.slang> <out.vert> <out.frag>\n");
      return 2;
   }
   if (!shader_line_buf_init(&lines))
      return 1;
   if (!glslang_read_shader_file(argv[1], &lines, true, false))
   {
      fprintf(stderr, "%s: cannot read\n", argv[1]);
      shader_line_buf_free(&lines);
      return 1;
   }
   vs = glslang_build_stage_source(&lines, "vertex");
   fs = glslang_build_stage_source(&lines, "fragment");
   shader_line_buf_free(&lines);
   if (!vs || !fs)
   {
      free(vs);
      free(fs);
      return 1;
   }
   rc = print_stage(vs, RSLC_STAGE_VERTEX, argv[2], argv[1]);
   if (!rc)
      rc = print_stage(fs, RSLC_STAGE_FRAGMENT, argv[3], argv[1]);
   free(vs);
   free(fs);
   return rc;
}
