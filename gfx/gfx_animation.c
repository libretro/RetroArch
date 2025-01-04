/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Jean-André Santoni
 *  Copyright (C) 2011-2018 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <encodings/utf.h>
#include <retro_math.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>
#include <array/rbuf.h>

#include "gfx_animation.h"
#include "../performance_counters.h"

#define TICKER_SLOW_SPEED  1666666

static gfx_animation_t anim_st = {
   0,      /* ticker_idx            */
   0,      /* ticker_slow_idx       */
   0,      /* ticker_pixel_idx      */
   0,      /* ticker_pixel_line_idx */
   0,      /* cur_time              */
   0,      /* old_time              */
   NULL,   /* updatetime_cb         */
   NULL,   /* list                  */
   NULL,   /* pending               */
   0.0f,   /* delta_time            */
   0       /* flags                 */
};

gfx_animation_t *anim_get_ptr(void)
{
   return &anim_st;
}

/* from https://github.com/kikito/tween.lua/blob/master/tween.lua */
static float easing_linear(float t, float b, float c, float d)
{
   return c * t / d + b;
}

static float easing_in_out_quad(float t, float b, float c, float d)
{
   t = t / d * 2.0f;
   if (t < 1.0f)
      return c / 2.0f * t * t + b;
   return -c / 2.0f * ((t - 1.0f) * (t - 3.0f) - 1.0f) + b;
}

static float easing_in_quad(float t, float b, float c, float d)
{
   float base = t / d;
   return c * (base * base) + b;
}

static float easing_out_quad(float t, float b, float c, float d)
{
   t = t / d;
   return -c * t * (t - 2.0f) + b;
}

static float easing_out_in_quad(float t, float b, float c, float d)
{
   if (t < d / 2.0f)
      return easing_out_quad(t * 2.0f, b, c / 2.0f, d);
   return easing_in_quad((t * 2.0f) - d, b + c / 2.0f, c / 2.0f, d);
}

static float easing_in_cubic(float t, float b, float c, float d)
{
   float base =  t / d;
   return c * (base * base * base) + b;
}

static float easing_out_cubic(float t, float b, float c, float d)
{
   float base = t / d - 1.0f;
   return c * ((base * base * base) + 1.0f) + b;
}

static float easing_in_out_cubic(float t, float b, float c, float d)
{
   t = t / d * 2.0f;
   if (t < 1.0f)
      return c / 2.0f * t * t * t + b;
   t = t - 2.0f;
   return c / 2.0f * (t * t * t + 2.0f) + b;
}

static float easing_out_in_cubic(float t, float b, float c, float d)
{
   if (t < d / 2.0f)
      return easing_out_cubic(t * 2.0f, b, c / 2.0f, d);
   return easing_in_cubic((t * 2.0f) - d, b + c / 2.0f, c / 2.0f, d);
}

static float easing_in_quart(float t, float b, float c, float d)
{
   float base = t / d;
   return c * (base * base * base * base) + b;
}

static float easing_out_quart(float t, float b, float c, float d)
{
   float base = t / d - 1.0f;
   return -c * ((base * base * base * base) - 1.0f) + b;
}

static float easing_in_out_quart(float t, float b, float c, float d)
{
   float base;
   t = t / d * 2.0f;
   if (t < 1.0f)
      return c / 2.0f * (t * t * t * t) + b;
   base = t - 2.0f;
   return -c / 2.0f * ((base * base * base * base) - 2.0f) + b;
}

static float easing_out_in_quart(float t, float b, float c, float d)
{
   if (t < d / 2.0f)
      return easing_out_quart(t * 2.0f, b, c / 2.0f, d);
   return easing_in_quart((t * 2.0f) - d, b + c / 2.0f, c / 2.0f, d);
}

static float easing_in_quint(float t, float b, float c, float d)
{
   float base = t / d;
   return c * (base * base * base * base * base) + b;
}

static float easing_out_quint(float t, float b, float c, float d)
{
   float base = t / d - 1.0f;
   return c * ((base * base * base * base * base) + 1.0f) + b;
}

static float easing_in_out_quint(float t, float b, float c, float d)
{
   float base;
   t = t / d * 2.0f;
   if (t < 1.0f)
      return c / 2.0f * (t * t * t * t * t) + b;
   base = t - 2.0f;
   return c / 2.0f * ((base * base * base * base * base) + 2.0f) + b;
}

static float easing_out_in_quint(float t, float b, float c, float d)
{
   if (t < d / 2.0f)
      return easing_out_quint(t * 2.0f, b, c / 2.0f, d);
   return easing_in_quint((t * 2.0f) - d, b + c / 2.0f, c / 2.0f, d);
}

static float easing_in_sine(float t, float b, float c, float d)
{
   return -c * cos(t / d * (M_PI / 2.0f)) + c + b;
}

static float easing_out_sine(float t, float b, float c, float d)
{
   return c * sin(t / d * (M_PI / 2.0f)) + b;
}

static float easing_in_out_sine(float t, float b, float c, float d)
{
   return -c / 2.0f * (cos(M_PI * t / d) - 1.0f) + b;
}

static float easing_out_in_sine(float t, float b, float c, float d)
{
   if (t < d / 2.0f)
      return easing_out_sine(t * 2.0f, b, c / 2.0f, d);
   return easing_in_sine((t * 2.0f) -d, b + c / 2.0f, c / 2.0f, d);
}

static float easing_in_expo(float t, float b, float c, float d)
{
   if (t == 0.0f)
      return b;
   return c * powf(2, 10 * (t / d - 1)) + b - c * 0.001;
}

static float easing_out_expo(float t, float b, float c, float d)
{
   if (t == d)
      return b + c;
   return c * 1.001 * (-powf(2, -10 * t / d) + 1) + b;
}

static float easing_in_out_expo(float t, float b, float c, float d)
{
   if (t == 0)
      return b;
   if (t == d)
      return b + c;
   t = t / d * 2.0f;
   if (t < 1.0f)
      return c / 2.0f * powf(2, 10 * (t - 1)) + b - c * 0.0005;
   return c / 2.0f * 1.0005 * (-powf(2, -10 * (t - 1)) + 2) + b;
}

static float easing_out_in_expo(float t, float b, float c, float d)
{
   if (t < d / 2.0f)
      return easing_out_expo(t * 2.0f, b, c / 2.0f, d);
   return easing_in_expo((t * 2.0f) - d, b + c / 2.0f, c / 2.0f, d);
}

static float easing_in_circ(float t, float b, float c, float d)
{
   float base = t / d;
   return(-c * (sqrtf(1.0f - (base * base)) - 1.0f) + b);
}

static float easing_out_circ(float t, float b, float c, float d)
{
   float base = t / d - 1;
   return(c * sqrtf(1.0f - (base * base)) + b);
}

static float easing_in_out_circ(float t, float b, float c, float d)
{
   t = t / d * 2.0f;
   if (t < 1.0f)
      return -c / 2 * (sqrtf(1.0f - t * t) - 1.0f) + b;
   t = t - 2.0f;
   return c / 2.0f * (sqrtf(1.0f - t * t) + 1.0f) + b;
}

static float easing_out_in_circ(float t, float b, float c, float d)
{
   if (t < d / 2.0f)
      return easing_out_circ(t * 2.0f, b, c / 2.0f, d);
   return easing_in_circ((t * 2.0f) - d, b + c / 2.0f, c / 2.0f, d);
}

static float easing_out_bounce(float t, float b, float c, float d)
{
   t = t / d;
   if (t < 1 / 2.75f)
      return c * (7.5625f * t * t) + b;
   if (t < 2 / 2.75f)
   {
      t = t - (1.5f / 2.75f);
      return c * (7.5625f * t * t + 0.75f) + b;
   }
   else if (t < 2.5f / 2.75f)
   {
      t = t - (2.25f / 2.75f);
      return c * (7.5625f * t * t + 0.9375f) + b;
   }
   t = t - (2.625f / 2.75f);
   return c * (7.5625f * t * t + 0.984375f) + b;
}

static float easing_in_bounce(float t, float b, float c, float d)
{
   return c - easing_out_bounce(d - t, 0, c, d) + b;
}

static float easing_in_out_bounce(float t, float b, float c, float d)
{
   if (t < d / 2.0f)
      return easing_in_bounce(t * 2.0f,   0.0f, c, d) * 0.5f + b;
   return easing_out_bounce(t * 2.0f - d, 0.0f, c, d) * 0.5f + c * .5f + b;
}

static float easing_out_in_bounce(float t, float b, float c, float d)
{
   if (t < d / 2.0f)
      return easing_out_bounce(t * 2.0f, b, c / 2.0f, d);
   return easing_in_bounce((t * 2.0f) - d, b + c / 2.0f, c / 2.0f, d);
}

static size_t gfx_animation_ticker_generic(uint64_t idx,
      size_t old_width)
{
   const int phase_left_stop   = 2;
   int ticker_period           = (int)(2 * old_width + 4);
   int phase                   = idx % ticker_period;
   int phase_left_moving       = (int)(phase_left_stop + old_width);
   int phase_right_stop        = phase_left_moving + 2;

   if (phase < phase_left_stop)
      return 0;
   else if (phase < phase_left_moving) /* left offset? */
      return phase - phase_left_stop;
   else if (phase < phase_right_stop)
      return old_width;
   /* right offset */
   return (int)(old_width - (phase - phase_right_stop));
}

static void gfx_animation_ticker_loop(uint64_t idx,
      size_t max_width, size_t str_width, size_t spacer_width,
      size_t *offset1, size_t *width1,
      size_t *offset2, size_t *width2,
      size_t *offset3, size_t *width3)
{
   int ticker_period     = (int)(str_width + spacer_width);
   int phase             = idx % ticker_period;

   /* Output offsets/widths are unsigned size_t, but it's
    * easier to perform the required calculations with ints,
    * so create some temporary variables... */
   /* Looping text is composed of up to three strings,
    * where string 1 and 2 are different regions of the
    * source text and string 2 is a spacer:
    *
    *     |-----max_width-----|
    * [string 1][string 2][string 3]
    *
    * The following implementation could probably be optimised,
    * but any performance gains would be trivial compared with
    * all the string manipulation that has to happen afterwards...
    */

   /* String 1 */
   int offset = 0;
   int width  = (int)(str_width - phase);
   if (width < 0)
      width   = 0;
   else if ((width > (int)max_width))
      width   = (int)max_width;

   if (phase < (int)str_width)
      offset  = phase;

   *offset1   = offset;
   *width1    = width;

   /* String 2 */
   offset     = (int)(phase - str_width);
   if (offset < 0)
      offset  = 0;
   width      = (int)(max_width - *width1);
   if (width > (int)spacer_width)
      width   = (int)spacer_width;
   width     -= offset;

   *offset2   = offset;
   *width2    = width;

   /* String 3 */
   width      = (int)(max_width - (*width1 + *width2));
   if (width < 0)
      width   = 0;

   /* Note: offset is always zero here so offset3 is
    * unnecessary - but include it anyway to preserve
    * symmetry... */
   *offset3   = 0;
   *width3    = width;
}

static unsigned get_ticker_smooth_generic_scroll_offset(
      uint64_t idx, unsigned str_width, unsigned field_width)
{
   const unsigned pause_duration = 32;
   unsigned scroll_width         = str_width - field_width;
   unsigned ticker_period        = 2 * (scroll_width + pause_duration);
   unsigned phase                = idx % ticker_period;

   /* Determine scroll offset */
   if (phase < pause_duration)
      return 0;
   else if (phase < ticker_period >> 1)
      return (phase - pause_duration);
   else if (phase < (ticker_period >> 1) + pause_duration)
      return ((ticker_period - (2 * pause_duration)) >> 1);

   return (ticker_period - phase);
}

/* 'Fixed width' font version of ticker_smooth_scan_characters() */
static void ticker_smooth_scan_string_fw(
      size_t num_chars, unsigned glyph_width,
      unsigned field_width, unsigned scroll_offset,
      unsigned *char_offset, unsigned *num_chars_to_copy,
      unsigned *x_offset)
{
   /* Determine index of first character to copy */
   if (scroll_offset > 0)
   {
      *char_offset = (scroll_offset / glyph_width) + 1;
      *x_offset    = glyph_width - (scroll_offset % glyph_width);
   }

   /* Determine number of characters remaining in
    * string once offset has been subtracted */
   if (*char_offset < num_chars)
   {
      size_t chars_remaining = num_chars - *char_offset;
      /* Determine number of characters to copy */
      if ((chars_remaining > 0) && (field_width > *x_offset))
      {
         *num_chars_to_copy = (field_width - *x_offset) / glyph_width;
         if (*num_chars_to_copy > chars_remaining)
            *num_chars_to_copy = (unsigned)chars_remaining;
      }
   }
}

/* 'Fixed width' font version of gfx_animation_ticker_smooth_generic() */
static void gfx_animation_ticker_smooth_generic_fw(uint64_t idx,
      unsigned str_width, size_t num_chars,
      unsigned glyph_width, unsigned field_width,
      unsigned *char_offset, unsigned *num_chars_to_copy, unsigned *x_offset)
{
   /* Initialise output variables to 'sane' values */
   *char_offset       = 0;
   *num_chars_to_copy = 0;
   *x_offset          = 0;

   /* Sanity check */
   if (num_chars >= 1)
   {
      unsigned scroll_offset = get_ticker_smooth_generic_scroll_offset(
            idx, str_width, field_width);
      ticker_smooth_scan_string_fw(
            num_chars, glyph_width, field_width, scroll_offset,
            char_offset, num_chars_to_copy, x_offset);
   }
}

/* 'Fixed width' font version of gfx_animation_ticker_smooth_loop() */
static void gfx_animation_ticker_smooth_loop_fw(uint64_t idx,
      unsigned str_width, size_t num_chars,
      unsigned spacer_width, size_t num_spacer_chars,
      unsigned glyph_width, unsigned field_width,
      unsigned *char_offset1, unsigned *num_chars_to_copy1,
      unsigned *char_offset2, unsigned *num_chars_to_copy2,
      unsigned *char_offset3, unsigned *num_chars_to_copy3,
      unsigned *x_offset)
{
   unsigned ticker_period   = str_width + spacer_width;
   unsigned phase           = idx % ticker_period;

   unsigned remaining_width = field_width;

   /* Initialise output variables to 'sane' values */
   *char_offset1       = 0;
   *num_chars_to_copy1 = 0;
   *char_offset2       = 0;
   *num_chars_to_copy2 = 0;
   *char_offset3       = 0;
   *num_chars_to_copy3 = 0;
   *x_offset           = 0;

   /* Sanity check */
   if ((num_chars < 1) || (num_spacer_chars < 1))
      return;

   /* Looping text is composed of up to three strings,
    * where string 1 and 2 are different regions of the
    * source text and string 2 is a spacer:
    *
    *     |----field_width----|
    * [string 1][string 2][string 3]
    */

   /* String 1 */
   if (phase < str_width)
   {
      unsigned scroll_offset = phase;

      ticker_smooth_scan_string_fw(
            num_chars, glyph_width, remaining_width, scroll_offset,
            char_offset1, num_chars_to_copy1, x_offset);

      /* Update remaining width
       * Note: We can avoid all the display_width shenanigans
       * here (c.f. gfx_animation_ticker_smooth_loop()) because
       * the font width is constant - i.e. we don't have to wrangle
       * out the width of the last 'non-copied' character since it
       * is known a priori, so we can just subtract the string width
       * + offset here, and perform an 'if (remaining_width > glyph_width)'
       * for strings 2 and 3 */
      remaining_width -= (*x_offset + (*num_chars_to_copy1 * glyph_width));
   }

   /* String 2 */
   if (remaining_width > glyph_width)
   {
      unsigned scroll_offset = 0;
      unsigned x_offset2     = 0;

      *char_offset2          = 0;
      *num_chars_to_copy2    = 0;

      /* Check whether we've passed the end of string 1 */
      if (phase > str_width)
         scroll_offset       = phase - str_width;

      ticker_smooth_scan_string_fw(
            num_spacer_chars, glyph_width, remaining_width, scroll_offset,
            char_offset2, num_chars_to_copy2, &x_offset2);

      /* > Update remaining width */
      remaining_width -= (x_offset2 + (*num_chars_to_copy2 * glyph_width));

      /* If scroll_offset is greater than zero, it means
       * string 2 is the first string to be displayed
       * > ticker x offset is therefore string 2's offset */
      if (scroll_offset > 0)
         *x_offset = x_offset2;
   }

   /* String 3 */
   if (remaining_width > glyph_width)
   {
      /* String 3 is only shown when string 2 is shown,
       * so we can take some shortcuts... */
      *char_offset3       = 0;

      /* Determine number of characters to copy */
      *num_chars_to_copy3 = remaining_width / glyph_width;
      if (*num_chars_to_copy3 > num_chars)
         *num_chars_to_copy3 = (unsigned)num_chars;
   }
}

static void ticker_smooth_scan_characters(
      const unsigned *char_widths, size_t num_chars,
      unsigned field_width, unsigned scroll_offset,
      unsigned *char_offset, unsigned *num_chars_to_copy,
      unsigned *x_offset, unsigned *str_width,
      unsigned *display_width)
{
   unsigned i;
   unsigned text_width     = 0;
   unsigned scroll_pos     = scroll_offset;
   bool deferred_str_width = true;

   /* Initialise output variables to 'sane' values */
   *char_offset       = 0;
   *num_chars_to_copy = 0;
   *x_offset          = 0;
   if (str_width)
      *str_width      = 0;
   if (display_width)
      *display_width  = 0;

   /* Determine index of first character to copy */
   if (scroll_pos > 0)
   {
      for (i = 0; i < num_chars; i++)
      {
         if (scroll_pos > char_widths[i])
            scroll_pos -= char_widths[i];
         else
         {
            /* Note: It's okay for char_offset to go out
             * of range here (num_chars_to_copy will be zero
             * in this case) */
            *char_offset = i + 1;
            *x_offset    = char_widths[i] - scroll_pos;
            break;
         }
      }
   }

   /* Determine number of characters to copy */
   for (i = *char_offset; i < num_chars; i++)
   {
      text_width += char_widths[i];

      if (*x_offset + text_width <= field_width)
         (*num_chars_to_copy)++;
      else
      {
         /* Get actual width of resultant string
          * (excluding x offset + end padding)
          * Note that this is only set if we exceed the
          * field width - if all characters up to the end
          * of the string are copied... */
         if (str_width)
         {
            deferred_str_width = false;
            *str_width         = text_width - char_widths[i];
         }
         break;
      }
   }

   /* ...then we have to update str_width here instead */
   if (str_width)
      if (deferred_str_width)
         *str_width = text_width;

   /* Get total display width of resultant string
    * (x offset + text width + end padding) */
   if (display_width)
   {
      *display_width    = *x_offset + text_width;
      if (*display_width > field_width)
         *display_width = field_width;
   }
}

static void gfx_animation_ticker_smooth_generic(uint64_t idx,
      const unsigned *char_widths, size_t num_chars,
      unsigned str_width, unsigned field_width,
      unsigned *char_offset, unsigned *num_chars_to_copy,
      unsigned *x_offset, unsigned *dst_str_width)
{
   /* Initialise output variables to 'sane' values */
   *char_offset       = 0;
   *num_chars_to_copy = 0;
   *x_offset          = 0;
   if (dst_str_width)
      *dst_str_width  = 0;

   /* Sanity check */
   if (num_chars >= 1)
   {
      unsigned scroll_offset = get_ticker_smooth_generic_scroll_offset(
            idx, str_width, field_width);
      ticker_smooth_scan_characters(
            char_widths, num_chars, field_width, scroll_offset,
            char_offset, num_chars_to_copy, x_offset, dst_str_width, NULL);
   }
}

static void gfx_animation_ticker_smooth_loop(uint64_t idx,
      const unsigned *char_widths, size_t num_chars,
      const unsigned *spacer_widths, size_t num_spacer_chars,
      unsigned str_width, unsigned spacer_width, unsigned field_width,
      unsigned *char_offset1, unsigned *num_chars_to_copy1,
      unsigned *char_offset2, unsigned *num_chars_to_copy2,
      unsigned *char_offset3, unsigned *num_chars_to_copy3,
      unsigned *x_offset, unsigned *dst_str_width)

{
   unsigned ticker_period    = str_width + spacer_width;
   unsigned phase            = idx % ticker_period;

   unsigned remaining_width  = field_width;

   /* Initialise output variables to 'sane' values */
   *char_offset1             = 0;
   *num_chars_to_copy1       = 0;
   *char_offset2             = 0;
   *num_chars_to_copy2       = 0;
   *char_offset3             = 0;
   *num_chars_to_copy3       = 0;
   *x_offset                 = 0;
   if (dst_str_width)
      *dst_str_width         = 0;

   /* Sanity check */
   if ((num_chars < 1) || (num_spacer_chars < 1))
      return;

   /* Looping text is composed of up to three strings,
    * where string 1 and 2 are different regions of the
    * source text and string 2 is a spacer:
    *
    *     |----field_width----|
    * [string 1][string 2][string 3]
    */

   /* String 1 */
   if (phase < str_width)
   {
      unsigned scroll_offset = phase;
      unsigned display_width = 0;
      unsigned str1_width    = 0;

      ticker_smooth_scan_characters(
            char_widths, num_chars, remaining_width, scroll_offset,
            char_offset1, num_chars_to_copy1, x_offset, &str1_width, &display_width);

      /* Update remaining width */
      remaining_width       -= display_width;

      /* Update dst_str_width */
      if (dst_str_width)
         *dst_str_width     += str1_width;
   }

   /* String 2 */
   if (remaining_width > 0)
   {
      unsigned scroll_offset = 0;
      unsigned display_width = 0;
      unsigned str2_width    = 0;
      unsigned x_offset2     = 0;

      /* Check whether we've passed the end of string 1 */
      if (phase > str_width)
         scroll_offset = phase - str_width;

      ticker_smooth_scan_characters(
            spacer_widths, num_spacer_chars, remaining_width, scroll_offset,
            char_offset2, num_chars_to_copy2, &x_offset2, &str2_width, &display_width);

      /* > Update remaining width */
      remaining_width -= display_width;

      /* Update dst_str_width */
      if (dst_str_width)
         *dst_str_width += str2_width;

      /* If scroll_offset is greater than zero, it means
       * string 2 is the first string to be displayed
       * > ticker x offset is therefore string 2's offset */
      if (scroll_offset > 0)
         *x_offset = x_offset2;
   }

   /* String 3 */
   if (remaining_width > 0)
   {
      /* String 3 is only shown when string 2 is shown,
       * so we can take some shortcuts... */
      unsigned i;
      unsigned text_width = 0;
      *char_offset3       = 0;

      /* Determine number of characters to copy */
      for (i = 0; i < num_chars; i++)
      {
         text_width += char_widths[i];

         if (text_width <= remaining_width)
            (*num_chars_to_copy3)++;
         else
         {
            /* Update dst_str_width */
            if (dst_str_width)
               *dst_str_width += text_width - char_widths[i];
            break;
         }
      }
   }
}

static void gfx_delayed_animation_cb(void *userdata)
{
   gfx_delayed_animation_t *delayed_animation =
      (gfx_delayed_animation_t*) userdata;

   gfx_animation_push(&delayed_animation->entry);

   free(delayed_animation);
}

void gfx_animation_push_delayed(
      unsigned delay, gfx_animation_ctx_entry_t *entry)
{
   gfx_timer_ctx_entry_t timer_entry;
   gfx_delayed_animation_t *delayed_animation  = (gfx_delayed_animation_t*)
      malloc(sizeof(gfx_delayed_animation_t));

   memcpy(&delayed_animation->entry, entry, sizeof(gfx_animation_ctx_entry_t));

   timer_entry.cb       = gfx_delayed_animation_cb;
   timer_entry.duration = delay;
   timer_entry.userdata = delayed_animation;

   gfx_animation_timer_start(&delayed_animation->timer, &timer_entry);
}

bool gfx_animation_push(gfx_animation_ctx_entry_t *entry)
{
   struct tween t;
   gfx_animation_t *p_anim = &anim_st;

   t.duration           = entry->duration;
   t.running_since      = 0;
   t.initial_value      = *entry->subject;
   t.target_value       = entry->target_value;
   t.subject            = entry->subject;
   t.tag                = entry->tag;
   t.cb                 = entry->cb;
   t.userdata           = entry->userdata;
   t.easing             = NULL;
   t.deleted            = false;

   switch (entry->easing_enum)
   {
      case EASING_LINEAR:
         t.easing       = &easing_linear;
         break;
         /* Quad */
      case EASING_IN_QUAD:
         t.easing       = &easing_in_quad;
         break;
      case EASING_OUT_QUAD:
         t.easing       = &easing_out_quad;
         break;
      case EASING_IN_OUT_QUAD:
         t.easing       = &easing_in_out_quad;
         break;
      case EASING_OUT_IN_QUAD:
         t.easing       = &easing_out_in_quad;
         break;
         /* Cubic */
      case EASING_IN_CUBIC:
         t.easing       = &easing_in_cubic;
         break;
      case EASING_OUT_CUBIC:
         t.easing       = &easing_out_cubic;
         break;
      case EASING_IN_OUT_CUBIC:
         t.easing       = &easing_in_out_cubic;
         break;
      case EASING_OUT_IN_CUBIC:
         t.easing       = &easing_out_in_cubic;
         break;
         /* Quart */
      case EASING_IN_QUART:
         t.easing       = &easing_in_quart;
         break;
      case EASING_OUT_QUART:
         t.easing       = &easing_out_quart;
         break;
      case EASING_IN_OUT_QUART:
         t.easing       = &easing_in_out_quart;
         break;
      case EASING_OUT_IN_QUART:
         t.easing       = &easing_out_in_quart;
         break;
         /* Quint */
      case EASING_IN_QUINT:
         t.easing       = &easing_in_quint;
         break;
      case EASING_OUT_QUINT:
         t.easing       = &easing_out_quint;
         break;
      case EASING_IN_OUT_QUINT:
         t.easing       = &easing_in_out_quint;
         break;
      case EASING_OUT_IN_QUINT:
         t.easing       = &easing_out_in_quint;
         break;
         /* Sine */
      case EASING_IN_SINE:
         t.easing       = &easing_in_sine;
         break;
      case EASING_OUT_SINE:
         t.easing       = &easing_out_sine;
         break;
      case EASING_IN_OUT_SINE:
         t.easing       = &easing_in_out_sine;
         break;
      case EASING_OUT_IN_SINE:
         t.easing       = &easing_out_in_sine;
         break;
         /* Expo */
      case EASING_IN_EXPO:
         t.easing       = &easing_in_expo;
         break;
      case EASING_OUT_EXPO:
         t.easing       = &easing_out_expo;
         break;
      case EASING_IN_OUT_EXPO:
         t.easing       = &easing_in_out_expo;
         break;
      case EASING_OUT_IN_EXPO:
         t.easing       = &easing_out_in_expo;
         break;
         /* Circ */
      case EASING_IN_CIRC:
         t.easing       = &easing_in_circ;
         break;
      case EASING_OUT_CIRC:
         t.easing       = &easing_out_circ;
         break;
      case EASING_IN_OUT_CIRC:
         t.easing       = &easing_in_out_circ;
         break;
      case EASING_OUT_IN_CIRC:
         t.easing       = &easing_out_in_circ;
         break;
         /* Bounce */
      case EASING_IN_BOUNCE:
         t.easing       = &easing_in_bounce;
         break;
      case EASING_OUT_BOUNCE:
         t.easing       = &easing_out_bounce;
         break;
      case EASING_IN_OUT_BOUNCE:
         t.easing       = &easing_in_out_bounce;
         break;
      case EASING_OUT_IN_BOUNCE:
         t.easing       = &easing_out_in_bounce;
         break;
      default:
         break;
   }

   /* ignore born dead tweens */
   if (!t.easing || t.duration == 0 || t.initial_value == t.target_value)
      return false;

   if (p_anim->flags & GFX_ANIM_FLAG_IN_UPDATE)
      RBUF_PUSH(p_anim->pending, t);
   else
      RBUF_PUSH(p_anim->list, t);

   return true;
}

bool gfx_animation_update(
      retro_time_t current_time,
      bool timedate_enable,
      float _ticker_speed,
      unsigned video_width,
      unsigned video_height)
{
   unsigned i;
   gfx_animation_t *p_anim                     = &anim_st;
   const bool ticker_is_active                 = (p_anim->flags & GFX_ANIM_FLAG_TICKER_IS_ACTIVE) ? true : false;

   static retro_time_t last_clock_update       = 0;
   static retro_time_t last_ticker_update      = 0;
   static retro_time_t last_ticker_slow_update = 0;

   /* Horizontal smooth ticker parameters */
   static float ticker_pixel_accumulator       = 0.0f;
   unsigned ticker_pixel_accumulator_uint      = 0;
   float ticker_pixel_increment                = 0.0f;

   /* Vertical (line) smooth ticker parameters */
   static float ticker_pixel_line_accumulator  = 0.0f;
   unsigned ticker_pixel_line_accumulator_uint = 0;
   float ticker_pixel_line_increment           = 0.0f;

   /* Adjust ticker speed */
   float speed_factor                          =
         (_ticker_speed > 0.0001f) ? _ticker_speed : 1.0f;
   unsigned ticker_speed                       =
      (unsigned)(((float)TICKER_SPEED / speed_factor) + 0.5);
   unsigned ticker_slow_speed                  =
      (unsigned)(((float)TICKER_SLOW_SPEED / speed_factor) + 0.5);

   /* Note: cur_time & old_time are in us (microseconds),
    * delta_time is in ms */
   p_anim->cur_time                            = current_time;
   p_anim->delta_time                          = (p_anim->old_time == 0)
      ? 0.0f
      : (float)(p_anim->cur_time - p_anim->old_time) / 1000.0f;
   p_anim->old_time                            = p_anim->cur_time;

   if (((p_anim->cur_time - last_clock_update) > 1000000) /* 1000000 us == 1 second */
         && timedate_enable)
   {
      p_anim->flags                |= GFX_ANIM_FLAG_IS_ACTIVE;
      last_clock_update             = p_anim->cur_time;
   }

   if (ticker_is_active)
   {
      /* Update non-smooth ticker indices */
      if (p_anim->cur_time - last_ticker_update >= ticker_speed)
      {
         p_anim->ticker_idx++;
         last_ticker_update = p_anim->cur_time;
      }

      if (p_anim->cur_time - last_ticker_slow_update >= ticker_slow_speed)
      {
         p_anim->ticker_slow_idx++;
         last_ticker_slow_update = p_anim->cur_time;
      }

      /* Pixel tickers (horizontal + vertical/line) update
       * every frame (regardless of time delta), so require
       * special handling */

      /* > Get base increment size (+1 every TICKER_PIXEL_PERIOD ms) */
      ticker_pixel_increment = p_anim->delta_time / TICKER_PIXEL_PERIOD;

      /* > Apply ticker speed adjustment */
      ticker_pixel_increment *= speed_factor;

      /* At this point we diverge:
       * > Vertical (line) ticker is based upon text
       *   characteristics (number of characters per
       *   line) - it is therefore independent of display
       *   size/scaling, so speed-adjusted pixel increment
       *   is used directly */
      ticker_pixel_line_increment = ticker_pixel_increment;

      /* > Horizontal ticker is based upon physical line
       *   width - it is therefore very much dependent upon
       *   display size/scaling. Each menu driver is free
       *   to handle video scaling as it pleases - a callback
       *   function set by the menu driver is thus used to
       *   perform menu-specific scaling adjustments */
      if (p_anim->updatetime_cb)
         p_anim->updatetime_cb(&ticker_pixel_increment,
               video_width, video_height);

      /* > Update accumulators */
      ticker_pixel_accumulator           += ticker_pixel_increment;
      ticker_pixel_accumulator_uint       = (unsigned)ticker_pixel_accumulator;

      ticker_pixel_line_accumulator      += ticker_pixel_line_increment;
      ticker_pixel_line_accumulator_uint  = (unsigned)ticker_pixel_line_accumulator;

      /* > Check whether we've accumulated enough
       *   for an idx update */
      if (ticker_pixel_accumulator_uint > 0)
      {
         p_anim->ticker_pixel_idx        += ticker_pixel_accumulator_uint;
         ticker_pixel_accumulator        -= (float)ticker_pixel_accumulator_uint;
      }

      if (ticker_pixel_accumulator_uint > 0)
      {
         p_anim->ticker_pixel_line_idx   += ticker_pixel_line_accumulator_uint;
         ticker_pixel_line_accumulator   -= (float)ticker_pixel_line_accumulator_uint;
      }
   }

   p_anim->flags           |=  GFX_ANIM_FLAG_IN_UPDATE;
   p_anim->flags           &= ~GFX_ANIM_FLAG_PENDING_DELETES;

   for (i = 0; i < RBUF_LEN(p_anim->list); i++)
   {
      struct tween *tween   = &p_anim->list[i];

      if (tween->deleted)
         continue;

      tween->running_since += p_anim->delta_time;

      *tween->subject       = tween->easing(
            tween->running_since,
            tween->initial_value,
            tween->target_value - tween->initial_value,
            tween->duration);

      if (tween->running_since >= tween->duration)
      {
         *tween->subject = tween->target_value;

         if (tween->cb)
            tween->cb(tween->userdata);

         RBUF_REMOVE(p_anim->list, i);
         i--;
      }
   }

   if (p_anim->flags & GFX_ANIM_FLAG_PENDING_DELETES)
   {
      for (i = 0; i < RBUF_LEN(p_anim->list); i++)
      {
         struct tween *tween = &p_anim->list[i];
         if (tween->deleted)
         {
            RBUF_REMOVE(p_anim->list, i);
            i--;
         }
      }
      p_anim->flags &= ~GFX_ANIM_FLAG_PENDING_DELETES;
   }

   if (RBUF_LEN(p_anim->pending) > 0)
   {
      size_t list_len    = RBUF_LEN(p_anim->list);
      size_t pending_len = RBUF_LEN(p_anim->pending);
      RBUF_RESIZE(p_anim->list, list_len + pending_len);
      memcpy(p_anim->list + list_len, p_anim->pending,
            sizeof(*p_anim->pending) * pending_len);
      RBUF_CLEAR(p_anim->pending);
   }

   p_anim->flags              &= ~GFX_ANIM_FLAG_IN_UPDATE;
   if (RBUF_LEN(p_anim->list) > 0)
	   p_anim->flags      |=  GFX_ANIM_FLAG_IS_ACTIVE;
   else
	   p_anim->flags      &= ~GFX_ANIM_FLAG_IS_ACTIVE;

   return ((p_anim->flags & GFX_ANIM_FLAG_IS_ACTIVE) > 0);
}

static size_t build_ticker_loop_string(
      const char* src_str, const char *spacer,
      size_t char_offset1, size_t num_chars1,
      size_t char_offset2, size_t num_chars2,
      size_t char_offset3, size_t num_chars3,
      char *s, size_t len)
{
   size_t _len = 0;
   /* Copy 'trailing' chunk of source string, if required */
   if (num_chars1 > 0)
      _len += utf8cpy(s + _len, len - _len,
            utf8skip(src_str, char_offset1), num_chars1);
   /* Copy chunk of spacer string, if required */
   if (num_chars2 > 0)
      _len += utf8cpy(s + _len, len - _len,
            utf8skip(spacer, char_offset2), num_chars2);
   /* Copy 'leading' chunk of source string, if required */
   if (num_chars3 > 0)
      _len += utf8cpy(s + _len, len - _len,
            utf8skip(src_str, char_offset3), num_chars3);
   return _len;
}

bool gfx_animation_ticker(gfx_animation_ctx_ticker_t *ticker)
{
   gfx_animation_t *p_anim = &anim_st;
   size_t str_len          = utf8len(ticker->str);

   if (!ticker->spacer)
      ticker->spacer       = TICKER_SPACER_DEFAULT;

   if ((size_t)str_len <= ticker->len)
   {
      utf8cpy(ticker->s,
            PATH_MAX_LENGTH,
            ticker->str,
            ticker->len);
      return false;
   }

   if (!ticker->selected)
   {
      size_t _len = utf8cpy(ticker->s,
            PATH_MAX_LENGTH, ticker->str, ticker->len - 3);
      ticker->s[  _len] = '.';
      ticker->s[++_len] = '.';
      ticker->s[++_len] = '.';
      ticker->s[++_len] = '\0';
      return false;
   }

   /* Note: If we reach this point then str_len > ticker->len
    * (previously had an unnecessary 'if (str_len > ticker->len)'
    * check here...) */
   switch (ticker->type_enum)
   {
      case TICKER_TYPE_LOOP:
         {
            size_t offset1, offset2, offset3;
            size_t width1, width2, width3;

            gfx_animation_ticker_loop(
                  ticker->idx,
                  ticker->len,
                  str_len, utf8len(ticker->spacer),
                  &offset1, &width1,
                  &offset2, &width2,
                  &offset3, &width3);

            build_ticker_loop_string(
                  ticker->str, ticker->spacer,
                  offset1, width1,
                  offset2, width2,
                  offset3, width3,
                  ticker->s, PATH_MAX_LENGTH);
         }
         break;
      case TICKER_TYPE_BOUNCE:
      default:
         {
            size_t offset = gfx_animation_ticker_generic(
                  ticker->idx,
                  str_len - ticker->len);

            str_len       = ticker->len;

            utf8cpy(
                  ticker->s,
                  PATH_MAX_LENGTH,
                  utf8skip(ticker->str, offset),
                  str_len);
         }
         break;
   }

   p_anim->flags |= GFX_ANIM_FLAG_TICKER_IS_ACTIVE;

   return true;
}

/* 'Fixed width' font version of gfx_animation_ticker_smooth() */
static bool gfx_animation_ticker_smooth_fw(
      gfx_animation_t *p_anim,
      gfx_animation_ctx_ticker_smooth_t *ticker)
{
   size_t spacer_len            = 0;
   unsigned glyph_width         = ticker->glyph_width;
   unsigned src_str_width       = 0;
   unsigned spacer_width        = 0;
   bool success                 = false;
   bool is_active               = false;

   /* Sanity check has already been performed by
    * gfx_animation_ticker_smooth() - no need to
    * repeat */

   /* Get length + width of src string */
   size_t src_str_len           = utf8len(ticker->src_str);
   if (src_str_len < 1)
      goto end;

   src_str_width = (unsigned)(src_str_len * glyph_width);

   /* If src string width is <= text field width, we
    * can just copy the entire string */
   if (src_str_width <= ticker->field_width)
   {
      utf8cpy(ticker->dst_str, ticker->dst_str_len,
            ticker->src_str, src_str_len);
      if (ticker->dst_str_width)
         *ticker->dst_str_width = src_str_width;
      *ticker->x_offset         = 0;
      success                   = true;
      goto end;
   }

   /* If entry is not selected, just clip input string
    * and add '...' suffix */
   if (!ticker->selected)
   {
      size_t _len;
      unsigned num_chars    = 0;
      unsigned suffix_len   = 3;
      unsigned suffix_width = suffix_len * glyph_width;

      /* Sanity check */
      if (ticker->field_width < suffix_width)
         goto end;

      /* Determine number of characters to copy */
      num_chars = (ticker->field_width - suffix_width) / glyph_width;
      /* Copy string segment + add suffix */
      _len      = utf8cpy(ticker->dst_str, ticker->dst_str_len, ticker->src_str, num_chars);
      ticker->dst_str[  _len] = '.';
      ticker->dst_str[++_len] = '.';
      ticker->dst_str[++_len] = '.';
      ticker->dst_str[++_len] = '\0';

      if (ticker->dst_str_width)
         *ticker->dst_str_width = (num_chars * glyph_width) + suffix_width;
      *ticker->x_offset         = 0;
      success                   = true;
      goto end;
   }

   /* If we get this far, then a scrolling animation
    * is required... */

   /* Use default spacer, if none is provided */
   if (!ticker->spacer)
      ticker->spacer     = TICKER_SPACER_DEFAULT;

   /* Get length + width of spacer */
   if ((spacer_len = utf8len(ticker->spacer)) < 1)
      goto end;

   spacer_width          = (unsigned)(spacer_len * glyph_width);

   /* Determine animation type */
   switch (ticker->type_enum)
   {
      case TICKER_TYPE_LOOP:
      {
         unsigned char_offset1 = 0;
         unsigned num_chars1   = 0;
         unsigned char_offset2 = 0;
         unsigned num_chars2   = 0;
         unsigned char_offset3 = 0;
         unsigned num_chars3   = 0;

         gfx_animation_ticker_smooth_loop_fw(
               ticker->idx,
               src_str_width, src_str_len, spacer_width, spacer_len,
               glyph_width, ticker->field_width,
               &char_offset1, &num_chars1,
               &char_offset2, &num_chars2,
               &char_offset3, &num_chars3,
               ticker->x_offset);

         build_ticker_loop_string(
               ticker->src_str, ticker->spacer,
               char_offset1, num_chars1,
               char_offset2, num_chars2,
               char_offset3, num_chars3,
               ticker->dst_str, ticker->dst_str_len);

         if (ticker->dst_str_width)
            *ticker->dst_str_width = (num_chars1 + num_chars2 + num_chars3) * glyph_width;

         break;
      }
      case TICKER_TYPE_BOUNCE:
      default:
      {
         unsigned char_offset = 0;
         unsigned num_chars   = 0;

         gfx_animation_ticker_smooth_generic_fw(
               ticker->idx,
               src_str_width, src_str_len, glyph_width, ticker->field_width,
               &char_offset, &num_chars, ticker->x_offset);

         /* Copy required substring */
         if (num_chars > 0)
            utf8cpy(
                  ticker->dst_str, ticker->dst_str_len,
                  utf8skip(ticker->src_str, char_offset), num_chars);
         else
            ticker->dst_str[0] = '\0';

         if (ticker->dst_str_width)
            *ticker->dst_str_width = num_chars * glyph_width;

         break;
      }
   }

   success                  = true;
   is_active                = true;
   p_anim->flags           |= GFX_ANIM_FLAG_TICKER_IS_ACTIVE;

end:
   if (!success)
   {
      *ticker->x_offset = 0;

      if (ticker->dst_str_len > 0)
         ticker->dst_str[0] = '\0';
   }

   return is_active;
}

bool gfx_animation_ticker_smooth(gfx_animation_ctx_ticker_smooth_t *ticker)
{
   size_t i;
   size_t src_str_len           = 0;
   size_t spacer_len            = 0;
   unsigned small_src_char_widths[64] = {0};
   unsigned src_str_width       = 0;
   unsigned spacer_width        = 0;
   unsigned *src_char_widths    = NULL;
   unsigned *spacer_char_widths = NULL;
   const char *str_ptr          = NULL;
   bool success                 = false;
   bool is_active               = false;
   gfx_animation_t *p_anim      = &anim_st;

   /* Sanity check */
   if (    string_is_empty(ticker->src_str)
       || (ticker->dst_str_len < 1)
       || (ticker->field_width < 1)
       || (!ticker->font && (ticker->glyph_width < 1)))
      goto end;

   /* If we are using a fixed width font (ticker->font == NULL),
    * switch to optimised code path */
   if (!ticker->font)
      return gfx_animation_ticker_smooth_fw(p_anim, ticker);

   /* Find the display width of each character in
    * the src string + total width */
   if ((src_str_len = utf8len(ticker->src_str)) < 1)
      goto end;

   src_char_widths = small_src_char_widths;

   if (src_str_len > ARRAY_SIZE(small_src_char_widths))
   {
      if (!(src_char_widths = (unsigned*)calloc(src_str_len, sizeof(unsigned))))
         goto end;
   }

   str_ptr = ticker->src_str;
   for (i = 0; i < src_str_len; i++)
   {
      int glyph_width = font_driver_get_message_width(
            ticker->font, str_ptr, 1, ticker->font_scale);

      if (glyph_width < 0)
         goto end;

      src_char_widths[i]  = (unsigned)glyph_width;
      src_str_width      += (unsigned)glyph_width;

      str_ptr             = utf8skip(str_ptr, 1);
   }

   /* If total src string width is <= text field width, we
    * can just copy the entire string */
   if (src_str_width <= ticker->field_width)
   {
      utf8cpy(ticker->dst_str, ticker->dst_str_len,
            ticker->src_str, src_str_len);

      if (ticker->dst_str_width)
         *ticker->dst_str_width = src_str_width;
      *ticker->x_offset = 0;
      success = true;
      goto end;
   }

   /* If entry is not selected, just clip input string
    * and add '...' suffix */
   if (!ticker->selected)
   {
      size_t _len;
      unsigned text_width;
      unsigned current_width = 0;
      unsigned num_chars     = 0;
      int period_width       =
            font_driver_get_message_width(ticker->font,
                  ".", 1, ticker->font_scale);

      /* Sanity check */
      if (period_width < 0)
         goto end;

      if (ticker->field_width < (3 * (unsigned)period_width))
         goto end;

      /* Determine number of characters to copy */
      text_width = ticker->field_width - (3 * period_width);

      for (;;)
      {
         current_width += src_char_widths[num_chars];

         if (current_width > text_width)
         {
            /* Have to go back one in order to get 'actual'
             * value for dst_str_width */
            current_width -= src_char_widths[num_chars];
            break;
         }

         num_chars++;
      }

      /* Copy string segment + add suffix */
      _len = utf8cpy(ticker->dst_str, ticker->dst_str_len,
            ticker->src_str, num_chars);
      ticker->dst_str[  _len] = '.';
      ticker->dst_str[++_len] = '.';
      ticker->dst_str[++_len] = '.';
      ticker->dst_str[++_len] = '\0';

      if (ticker->dst_str_width)
         *ticker->dst_str_width = current_width + (3 * period_width);
      *ticker->x_offset = 0;
      success = true;
      goto end;
   }

   /* If we get this far, then a scrolling animation
    * is required... */

   /* Use default spacer, if none is provided */
   if (!ticker->spacer)
      ticker->spacer = TICKER_SPACER_DEFAULT;

   /* Find the display width of each character in
    * the spacer */
   if ((spacer_len = utf8len(ticker->spacer)) < 1)
      goto end;

   if (!(spacer_char_widths = (unsigned*)calloc(spacer_len,  sizeof(unsigned))))
      goto end;

   str_ptr = ticker->spacer;
   for (i = 0; i < spacer_len; i++)
   {
      int glyph_width = font_driver_get_message_width(
            ticker->font, str_ptr, 1, ticker->font_scale);

      if (glyph_width < 0)
         goto end;

      spacer_char_widths[i] = (unsigned)glyph_width;
      spacer_width += (unsigned)glyph_width;

      str_ptr = utf8skip(str_ptr, 1);
   }

   /* Determine animation type */
   switch (ticker->type_enum)
   {
      case TICKER_TYPE_LOOP:
      {
         unsigned char_offset1 = 0;
         unsigned num_chars1   = 0;
         unsigned char_offset2 = 0;
         unsigned num_chars2   = 0;
         unsigned char_offset3 = 0;
         unsigned num_chars3   = 0;

         gfx_animation_ticker_smooth_loop(
               ticker->idx,
               src_char_widths, src_str_len,
               spacer_char_widths, spacer_len,
               src_str_width, spacer_width, ticker->field_width,
               &char_offset1, &num_chars1,
               &char_offset2, &num_chars2,
               &char_offset3, &num_chars3,
               ticker->x_offset, ticker->dst_str_width);

         build_ticker_loop_string(
               ticker->src_str, ticker->spacer,
               char_offset1, num_chars1,
               char_offset2, num_chars2,
               char_offset3, num_chars3,
               ticker->dst_str, ticker->dst_str_len);

         break;
      }
      case TICKER_TYPE_BOUNCE:
      default:
      {
         unsigned char_offset = 0;
         unsigned num_chars   = 0;

         gfx_animation_ticker_smooth_generic(
               ticker->idx,
               src_char_widths, src_str_len,
               src_str_width, ticker->field_width,
               &char_offset, &num_chars,
               ticker->x_offset, ticker->dst_str_width);

         /* Copy required substring */
         if (num_chars > 0)
            utf8cpy(
                  ticker->dst_str, ticker->dst_str_len,
                  utf8skip(ticker->src_str, char_offset), num_chars);
         else
            ticker->dst_str[0] = '\0';

         break;
      }
   }

   success                  = true;
   is_active                = true;
   p_anim->flags           |= GFX_ANIM_FLAG_TICKER_IS_ACTIVE;

end:

   if (src_char_widths != small_src_char_widths && src_char_widths)
   {
      free(src_char_widths);
      src_char_widths = NULL;
   }

   if (spacer_char_widths)
   {
      free(spacer_char_widths);
      spacer_char_widths = NULL;
   }

   if (!success)
   {
      *ticker->x_offset = 0;

      if (ticker->dst_str_len > 0)
         ticker->dst_str[0] = '\0';
   }

   return is_active;
}

bool gfx_animation_kill_by_tag(uintptr_t *tag)
{
   unsigned i;
   gfx_animation_t *p_anim = &anim_st;

   if (!tag || *tag == (uintptr_t)-1)
      return false;

   /* Scan animation list */
   for (i = 0; i < RBUF_LEN(p_anim->list); ++i)
   {
      struct tween *t = &p_anim->list[i];

      if (t->tag != *tag)
         continue;

      /* If we are currently inside gfx_animation_update(),
       * we are already looping over p_anim->list entries
       * > Cannot modify p_anim->list now, so schedule a
       *   delete for when the gfx_animation_update() loop
       *   is complete */
      if (p_anim->flags & GFX_ANIM_FLAG_IN_UPDATE)
      {
         t->deleted              = true;
         p_anim->flags          |= GFX_ANIM_FLAG_PENDING_DELETES;
      }
      else
      {
         RBUF_REMOVE(p_anim->list, i);
         --i;
      }
   }

   /* If we are currently inside gfx_animation_update(),
    * also have to scan *pending* animation list
    * (otherwise any entries that are simultaneously added
    * and deleted inside gfx_animation_update() won't get
    * deleted at all, producing utter chaos) */
   if (p_anim->flags & GFX_ANIM_FLAG_IN_UPDATE)
   {
      for (i = 0; i < RBUF_LEN(p_anim->pending); ++i)
      {
         struct tween *t = &p_anim->pending[i];

         if (t->tag != *tag)
            continue;

         RBUF_REMOVE(p_anim->pending, i);
         --i;
      }
   }

   return true;
}

void gfx_animation_deinit(void)
{
   gfx_animation_t *p_anim = &anim_st;
   if (!p_anim)
      return;
   RBUF_FREE(p_anim->list);
   RBUF_FREE(p_anim->pending);
   if (p_anim->updatetime_cb)
      p_anim->updatetime_cb = NULL;
   memset(p_anim, 0, sizeof(*p_anim));
}

void gfx_animation_timer_start(float *timer, gfx_timer_ctx_entry_t *timer_entry)
{
   gfx_animation_ctx_entry_t entry;
   uintptr_t tag        = (uintptr_t) timer;

   gfx_animation_kill_by_tag(&tag);

   *timer               = 0.0f;

   entry.easing_enum    = EASING_LINEAR;
   entry.tag            = tag;
   entry.duration       = timer_entry->duration;
   entry.target_value   = 1.0f;
   entry.subject        = timer;
   entry.cb             = timer_entry->cb;
   entry.userdata       = timer_entry->userdata;

   gfx_animation_push(&entry);
}
