/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stubs_retroarch.c).
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

/* Frontend symbols used by the horizontal and vertical tickers. */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>
#include <string/stdstring.h>

int font_driver_get_message_width(void *font, const char *msg,
      size_t len, float scale)
{
   if (!font || !len)
      return 0;
   return (int)(((unsigned char)msg[0] < 128 ? 8 : 16) * scale);
}

const char *animation_test_wideglyph;

const char *msg_hash_get_wideglyph_str(void)
{
   return animation_test_wideglyph;
}

unsigned animation_test_font_generation;
unsigned animation_test_wrap_calls;
unsigned animation_test_allocations;
bool animation_test_fail_malloc;

unsigned font_driver_get_generation(void)
{
   return animation_test_font_generation;
}

size_t animation_test_word_wrap(char *dst, size_t len, const char *src,
      size_t src_len, int width, int wide_width, unsigned max_lines)
{
   animation_test_wrap_calls++;
   return word_wrap(dst, len, src, src_len, width, wide_width, max_lines);
}

size_t animation_test_word_wrap_wideglyph(char *dst, size_t len, const char *src,
      size_t src_len, int width, int wide_width, unsigned max_lines)
{
   animation_test_wrap_calls++;
   return word_wrap_wideglyph(dst, len, src, src_len,
         width, wide_width, max_lines);
}

void *animation_test_malloc(size_t size)
{
   animation_test_allocations++;
   if (animation_test_fail_malloc)
   {
      animation_test_fail_malloc = false;
      return NULL;
   }
   return malloc(size);
}

void *animation_test_realloc(void *ptr, size_t size)
{
   animation_test_allocations++;
   return realloc(ptr, size);
}
