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

/* The only two frontend symbols gfx/gfx_animation.c reaches for.
 *
 * The smooth ticker takes either a font_data_t or, when that is NULL,
 * a fixed glyph_width; the test passes NULL so the width path is the
 * one exercised and these are never entered.  They still have to
 * resolve, and their signatures are copied from gfx/font_driver.h
 * rather than guessed. */

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>

int font_driver_get_message_width(void *font, const char *msg,
      size_t len, float scale)
{
   (void)font; (void)msg; (void)len; (void)scale;
   return 0;
}

unsigned font_driver_get_generation(void *font)
{
   (void)font;
   return 0;
}
