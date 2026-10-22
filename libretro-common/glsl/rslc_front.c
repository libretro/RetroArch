/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_front.c).
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

/* rslc TU 1: preprocessor, lexer, parser, typecheck.
 *
 * Owns rslc_compile().  Reads the stage source as a pointer and a
 * length; never copies it, never NUL-terminates it, never reads past
 * len.  Everything it builds goes in the arena. */

#include <string.h>

#include "rslc_share.h"

bool rslc_front_compile(struct rslc_ctx *c, const char *src, size_t len)
{
   size_t i = 0;

   /* The spec requires #version on the first line; nothing else is
    * accepted ahead of it. */
   while (i < len && (src[i] == ' ' || src[i] == '\t' || src[i] == '\r'
            || src[i] == '\n'))
      i++;
   if (len - i < 8 || memcmp(src + i, "#version", 8))
   {
      rslc_err(c, "first line must be #version", NULL, 1);
      return false;
   }

   rslc_err(c, "front end not implemented", NULL, 0);
   return false;
}

bool rslc_compile(const char *glsl, size_t len,
      enum rslc_stage stage,
      const struct rslc_opts *opts, struct rslc_result *result)
{
   struct rslc_ctx c;
   bool            ok;

   if (!glsl || !opts || !result)
      return false;
   if (!rslc_ctx_init(&c, opts, stage))
      return false;

   ok = rslc_front_compile(&c, glsl, len);
   if (ok)
      ok = rslc_emit_dst(&c, result);

   rslc_ctx_close(&c);
   return ok;
}
