/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_resampler_int16.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* The integer counterparts, in their own unit: a target that never
 * asks for one links none of the int16 drivers. */

#include <string.h>

#include <string/stdstring.h>

#include <audio/audio_resampler.h>
#include <audio/sinc_resampler_int16.h>
#ifdef HAVE_NEAREST_RESAMPLER
#include <audio/nearest_resampler_int16.h>
#endif
#ifdef HAVE_CC_RESAMPLER
#include <audio/cc_resampler_int16.h>
#endif

/* The shared quality enum onto the integer sinc driver's own, which
 * orders its tiers differently. */
static enum sinc_int16_quality resampler_sinc_int16_quality(
      enum resampler_quality q)
{
   switch (q)
   {
      case RESAMPLER_QUALITY_LOWEST:  return SINC_INT16_QUALITY_LOWEST;
      case RESAMPLER_QUALITY_LOWER:   return SINC_INT16_QUALITY_LOWER;
      case RESAMPLER_QUALITY_HIGHER:  return SINC_INT16_QUALITY_HIGHER;
      case RESAMPLER_QUALITY_HIGHEST: return SINC_INT16_QUALITY_HIGHEST;
      case RESAMPLER_QUALITY_NORMAL:
      case RESAMPLER_QUALITY_DONTCARE:
      default:                        return SINC_INT16_QUALITY_NORMAL;
   }
}

bool retro_resampler_int16_new(retro_resampler_int16_t *out,
      const char *short_ident, enum resampler_quality quality,
      double bw_ratio, bool hq_oversampling)
{
   const retro_resampler_t *drv;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   /* The same lookup retro_resampler_realloc() uses, so a NULL or
    * unknown name lands on the fallback here too. */
   if (!(drv = audio_resampler_driver_find(short_ident)) || !drv->short_ident)
      return false;
   short_ident = drv->short_ident;

   if (string_is_equal(short_ident, "sinc"))
   {
      out->data    = sinc_resampler_int16_init_hq(bw_ratio,
            resampler_sinc_int16_quality(quality), hq_oversampling);
      out->process = sinc_resampler_int16_process;
      out->reset   = sinc_resampler_int16_reset;
      out->free    = sinc_resampler_int16_free;
   }
#ifdef HAVE_NEAREST_RESAMPLER
   else if (string_is_equal(short_ident, "nearest"))
   {
      out->data    = nearest_resampler_int16_init();
      out->process = nearest_resampler_int16_process;
      out->reset   = nearest_resampler_int16_reset;
      out->free    = nearest_resampler_int16_free;
   }
#endif
#ifdef HAVE_CC_RESAMPLER
   else if (string_is_equal(short_ident, "cc"))
   {
      out->data    = cc_resampler_int16_init(bw_ratio);
      out->process = cc_resampler_int16_process;
      out->reset   = cc_resampler_int16_reset;
      out->free    = cc_resampler_int16_free;
   }
#endif
   else
      return false;

   if (out->data)
      return true;
   memset(out, 0, sizeof(*out));
   return false;
}
