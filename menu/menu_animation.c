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
#include <lists/string_list.h>

#define DG_DYNARR_IMPLEMENTATION
#include <stdio.h>
#include <retro_assert.h>
#define DG_DYNARR_ASSERT(cond, msg)  (void)0
#include <array/dynarray.h>
#undef DG_DYNARR_IMPLEMENTATION

#include "menu_animation.h"
#include "menu_driver.h"
#include "../configuration.h"
#include "../performance_counters.h"

struct tween
{
   float       duration;
   float       running_since;
   float       initial_value;
   float       target_value;
   float       *subject;
   uintptr_t   tag;
   easing_cb   easing;
   tween_cb    cb;
   void        *userdata;
   bool        deleted;
};

DA_TYPEDEF(struct tween, tween_array_t)

struct menu_animation
{
   tween_array_t list;
   tween_array_t pending;
   bool initialized;
   bool pending_deletes;
   bool in_update;
};

typedef struct menu_animation menu_animation_t;

#define TICKER_SPEED       333333
#define TICKER_SLOW_SPEED  1666666

/* Pixel ticker nominally increases by one after each
 * ticker_pixel_period ms (actual increase depends upon
 * ticker speed setting and display resolution) */
static const float ticker_pixel_period = (1.0f / 60.0f) * 1000.0f;

static const char ticker_spacer_default[] = TICKER_SPACER_DEFAULT;

static menu_animation_t anim     = {{0}};
static retro_time_t cur_time     = 0;
static retro_time_t old_time     = 0;
static uint64_t ticker_idx       = 0; /* updated every TICKER_SPEED us */
static uint64_t ticker_slow_idx  = 0; /* updated every TICKER_SLOW_SPEED us */
static uint64_t ticker_pixel_idx = 0; /* updated every frame */
static float delta_time          = 0.0f;
static bool animation_is_active  = false;
static bool ticker_is_active     = false;

/* from https://github.com/kikito/tween.lua/blob/master/tween.lua */

static float easing_linear(float t, float b, float c, float d)
{
   return c * t / d + b;
}

static float easing_in_out_quad(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 2) + b;
   return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
}

static float easing_in_quad(float t, float b, float c, float d)
{
   return c * pow(t / d, 2) + b;
}

static float easing_out_quad(float t, float b, float c, float d)
{
   t = t / d;
   return -c * t * (t - 2) + b;
}

static float easing_out_in_quad(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_quad(t * 2, b, c / 2, d);
   return easing_in_quad((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_cubic(float t, float b, float c, float d)
{
   return c * pow(t / d, 3) + b;
}

static float easing_out_cubic(float t, float b, float c, float d)
{
   return c * (pow(t / d - 1, 3) + 1) + b;
}

static float easing_in_out_cubic(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * t * t * t + b;
   t = t - 2;
   return c / 2 * (t * t * t + 2) + b;
}

static float easing_out_in_cubic(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_cubic(t * 2, b, c / 2, d);
   return easing_in_cubic((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_quart(float t, float b, float c, float d)
{
   return c * pow(t / d, 4) + b;
}

static float easing_out_quart(float t, float b, float c, float d)
{
   return -c * (pow(t / d - 1, 4) - 1) + b;
}

static float easing_in_out_quart(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 4) + b;
   return -c / 2 * (pow(t - 2, 4) - 2) + b;
}

static float easing_out_in_quart(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_quart(t * 2, b, c / 2, d);
   return easing_in_quart((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_quint(float t, float b, float c, float d)
{
   return c * pow(t / d, 5) + b;
}

static float easing_out_quint(float t, float b, float c, float d)
{
   return c * (pow(t / d - 1, 5) + 1) + b;
}

static float easing_in_out_quint(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 5) + b;
   return c / 2 * (pow(t - 2, 5) + 2) + b;
}

static float easing_out_in_quint(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_quint(t * 2, b, c / 2, d);
   return easing_in_quint((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_sine(float t, float b, float c, float d)
{
   return -c * cos(t / d * (M_PI / 2)) + c + b;
}

static float easing_out_sine(float t, float b, float c, float d)
{
   return c * sin(t / d * (M_PI / 2)) + b;
}

static float easing_in_out_sine(float t, float b, float c, float d)
{
   return -c / 2 * (cos(M_PI * t / d) - 1) + b;
}

static float easing_out_in_sine(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_sine(t * 2, b, c / 2, d);
   return easing_in_sine((t * 2) -d, b + c / 2, c / 2, d);
}

static float easing_in_expo(float t, float b, float c, float d)
{
   if (t == 0)
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
   t = t / d * 2;
   if (t < 1)
      return c / 2 * powf(2, 10 * (t - 1)) + b - c * 0.0005;
   return c / 2 * 1.0005 * (-powf(2, -10 * (t - 1)) + 2) + b;
}

static float easing_out_in_expo(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_expo(t * 2, b, c / 2, d);
   return easing_in_expo((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_circ(float t, float b, float c, float d)
{
   return(-c * (sqrt(1 - powf(t / d, 2)) - 1) + b);
}

static float easing_out_circ(float t, float b, float c, float d)
{
   return(c * sqrt(1 - powf(t / d - 1, 2)) + b);
}

static float easing_in_out_circ(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return -c / 2 * (sqrt(1 - t * t) - 1) + b;
   t = t - 2;
   return c / 2 * (sqrt(1 - t * t) + 1) + b;
}

static float easing_out_in_circ(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_circ(t * 2, b, c / 2, d);
   return easing_in_circ((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_out_bounce(float t, float b, float c, float d)
{
   t = t / d;
   if (t < 1 / 2.75)
      return c * (7.5625 * t * t) + b;
   if (t < 2 / 2.75)
   {
      t = t - (1.5 / 2.75);
      return c * (7.5625 * t * t + 0.75) + b;
   }
   else if (t < 2.5 / 2.75)
   {
      t = t - (2.25 / 2.75);
      return c * (7.5625 * t * t + 0.9375) + b;
   }
   t = t - (2.625 / 2.75);
   return c * (7.5625 * t * t + 0.984375) + b;
}

static float easing_in_bounce(float t, float b, float c, float d)
{
   return c - easing_out_bounce(d - t, 0, c, d) + b;
}

static float easing_in_out_bounce(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_in_bounce(t * 2, 0, c, d) * 0.5 + b;
   return easing_out_bounce(t * 2 - d, 0, c, d) * 0.5 + c * .5 + b;
}

static float easing_out_in_bounce(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_bounce(t * 2, b, c / 2, d);
   return easing_in_bounce((t * 2) - d, b + c / 2, c / 2, d);
}

static void menu_animation_ticker_generic(uint64_t idx,
      size_t max_width, size_t *offset, size_t *width)
{
   int ticker_period     = (int)(2 * (*width - max_width) + 4);
   int phase             = idx % ticker_period;

   int phase_left_stop   = 2;
   int phase_left_moving = (int)(phase_left_stop + (*width - max_width));
   int phase_right_stop  = phase_left_moving + 2;

   int left_offset       = phase - phase_left_stop;
   int right_offset      = (int)((*width - max_width) - (phase - phase_right_stop));

   if (phase < phase_left_stop)
      *offset = 0;
   else if (phase < phase_left_moving)
      *offset = left_offset;
   else if (phase < phase_right_stop)
      *offset = *width - max_width;
   else
      *offset = right_offset;

   *width = max_width;
}

static void menu_animation_ticker_loop(uint64_t idx,
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
   int offset;
   int width;
   
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
   offset   = (phase < (int)str_width) ? phase : 0;
   width    = (int)(str_width - phase);
   width    = (width < 0) ? 0 : width;
   width    = (width > (int)max_width) ? (int)max_width : width;
   
   *offset1 = offset;
   *width1  = width;
   
   /* String 2 */
   offset   = (int)(phase - str_width);
   offset   = offset < 0 ? 0 : offset;
   width    = (int)(max_width - *width1);
   width    = (width > (int)spacer_width) ? (int)spacer_width : width;
   width    = width - offset;
   
   *offset2 = offset;
   *width2  = width;
   
   /* String 3 */
   width    = (int)(max_width - (*width1 + *width2));
   width    = width < 0 ? 0 : width;
   
   /* Note: offset is always zero here so offset3 is
    * unnecessary - but include it anyway to preserve
    * symmetry... */
   *offset3 = 0;
   *width3  = width;
}

static unsigned get_ticker_smooth_generic_scroll_offset(
      uint64_t idx, unsigned str_width, unsigned field_width)
{
   unsigned scroll_width   = str_width - field_width;
   unsigned scroll_offset  = 0;

   unsigned pause_duration = 32;
   unsigned ticker_period  = 2 * (scroll_width + pause_duration);
   unsigned phase          = idx % ticker_period;

   /* Determine scroll offset */
   if (phase < pause_duration)
      scroll_offset = 0;
   else if (phase < ticker_period >> 1)
      scroll_offset = phase - pause_duration;
   else if (phase < (ticker_period >> 1) + pause_duration)
      scroll_offset = (ticker_period - (2 * pause_duration)) >> 1;
   else
      scroll_offset = ticker_period - phase;

   return scroll_offset;
}

/* 'Fixed width' font version of ticker_smooth_scan_characters() */
static void ticker_smooth_scan_string_fw(
      size_t num_chars, unsigned glyph_width, unsigned field_width, unsigned scroll_offset,
      unsigned *char_offset, unsigned *num_chars_to_copy, unsigned *x_offset)
{
   unsigned chars_remaining = 0;

   /* Initialise output variables to 'sane' values */
   *char_offset       = 0;
   *num_chars_to_copy = 0;
   *x_offset          = 0;

   /* Determine index of first character to copy */
   if (scroll_offset > 0)
   {
      *char_offset = (scroll_offset / glyph_width) + 1;
      *x_offset    = glyph_width - (scroll_offset % glyph_width);
   }

   /* Determine number of characters remaining in
    * string once offset has been subtracted */
   chars_remaining = (*char_offset >= num_chars) ? 0 : num_chars - *char_offset;

   /* Determine number of characters to copy */
   if ((chars_remaining > 0) && (field_width > *x_offset))
   {
      *num_chars_to_copy = (field_width - *x_offset) / glyph_width;
      *num_chars_to_copy = (*num_chars_to_copy > chars_remaining) ? chars_remaining : *num_chars_to_copy;
   }
}

/* 'Fixed width' font version of menu_animation_ticker_smooth_generic() */
static void menu_animation_ticker_smooth_generic_fw(uint64_t idx,
      unsigned str_width, size_t num_chars,
      unsigned glyph_width, unsigned field_width,
      unsigned *char_offset, unsigned *num_chars_to_copy, unsigned *x_offset)
{
   unsigned scroll_offset = get_ticker_smooth_generic_scroll_offset(
      idx, str_width, field_width);

   /* Initialise output variables to 'sane' values */
   *char_offset       = 0;
   *num_chars_to_copy = 0;
   *x_offset          = 0;

   /* Sanity check */
   if (num_chars < 1)
      return;

   ticker_smooth_scan_string_fw(
         num_chars, glyph_width, field_width, scroll_offset,
         char_offset, num_chars_to_copy, x_offset);
}

/* 'Fixed width' font version of menu_animation_ticker_smooth_loop() */
static void menu_animation_ticker_smooth_loop_fw(uint64_t idx,
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
       * here (c.f. menu_animation_ticker_smooth_loop()) because
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

      /* Check whether we've passed the end of string 1 */
      if (phase > str_width)
         scroll_offset = phase - str_width;
      else
         scroll_offset = 0;

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
      *num_chars_to_copy3 = (*num_chars_to_copy3 > num_chars) ? num_chars : *num_chars_to_copy3;
   }
}

static void ticker_smooth_scan_characters(
      const unsigned *char_widths, size_t num_chars, unsigned field_width, unsigned scroll_offset,
      unsigned *char_offset, unsigned *num_chars_to_copy, unsigned *x_offset,
      unsigned *str_width, unsigned *display_width)
{
   unsigned text_width     = 0;
   unsigned scroll_pos     = scroll_offset;
   bool deferred_str_width = true;
   unsigned i;

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
            *x_offset = char_widths[i] - scroll_pos;
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
            *str_width = text_width - char_widths[i];
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
      *display_width = *x_offset + text_width;
      *display_width = (*display_width > field_width) ? field_width : *display_width;
   }
}

static void menu_animation_ticker_smooth_generic(uint64_t idx,
      const unsigned *char_widths, size_t num_chars, unsigned str_width, unsigned field_width,
      unsigned *char_offset, unsigned *num_chars_to_copy, unsigned *x_offset, unsigned *dst_str_width)
{
   unsigned scroll_offset = get_ticker_smooth_generic_scroll_offset(
      idx, str_width, field_width);

   /* Initialise output variables to 'sane' values */
   *char_offset       = 0;
   *num_chars_to_copy = 0;
   *x_offset          = 0;
   if (dst_str_width)
      *dst_str_width  = 0;

   /* Sanity check */
   if (num_chars < 1)
      return;

   ticker_smooth_scan_characters(
      char_widths, num_chars, field_width, scroll_offset,
      char_offset, num_chars_to_copy, x_offset, dst_str_width, NULL);
}

static void menu_animation_ticker_smooth_loop(uint64_t idx,
      const unsigned *char_widths, size_t num_chars,
      const unsigned *spacer_widths, size_t num_spacer_chars,
      unsigned str_width, unsigned spacer_width, unsigned field_width,
      unsigned *char_offset1, unsigned *num_chars_to_copy1,
      unsigned *char_offset2, unsigned *num_chars_to_copy2,
      unsigned *char_offset3, unsigned *num_chars_to_copy3,
      unsigned *x_offset, unsigned *dst_str_width)

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
   if (dst_str_width)
      *dst_str_width   = 0;

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
      remaining_width -= display_width;

      /* Update dst_str_width */
      if (dst_str_width)
         *dst_str_width += str1_width;
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
      else
         scroll_offset = 0;

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

static size_t get_line_display_ticks(size_t line_len)
{
   /* Mean human reading speed for all western languages,
    * characters per minute */
   float cpm            = 1000.0f;
   /* Base time for which a line should be shown, in us */
   float line_duration  = (line_len * 60.0f * 1000.0f * 1000.0f) / cpm;
   /* Ticker updates (nominally) once every TICKER_SPEED us
    * > Return base number of ticks for which line should be shown */
   return (size_t)(line_duration / (float)TICKER_SPEED);
}

static void menu_animation_line_ticker_generic(uint64_t idx,
      size_t line_len, size_t max_lines, size_t num_lines,
      size_t *line_offset)
{
   size_t line_ticks    = get_line_display_ticks(line_len);
   /* Note: This function is only called if num_lines > max_lines */
   size_t excess_lines  = num_lines - max_lines;
   /* Ticker will pause for one line duration when the first
    * or last line is reached (this is mostly required for the
    * case where num_lines == (max_lines + 1), since otherwise
    * the text flicks rapidly up and down in disconcerting
    * fashion...) */
   size_t ticker_period = (excess_lines * 2) + 2;
   size_t phase         = (idx / line_ticks) % ticker_period;

   /* Pause on first line */
   if (phase > 0)
      phase--;
   /* Pause on last line */
   if (phase > excess_lines)
      phase--;

   /* Lines scrolling upwards */
   if (phase <= excess_lines)
      *line_offset = phase;
   /* Lines scrolling downwards */
   else
      *line_offset = (excess_lines * 2) - phase;
}

static void menu_animation_line_ticker_loop(uint64_t idx,
      size_t line_len, size_t num_lines,
      size_t *line_offset)
{
   size_t line_ticks    = get_line_display_ticks(line_len);
   size_t ticker_period = num_lines + 1;
   size_t phase         = (idx / line_ticks) % ticker_period;

   /* In this case, line_offset is simply equal to the phase */
   *line_offset = phase;
}

static size_t get_line_smooth_scroll_ticks(size_t line_len)
{
   /* Mean human reading speed for all western languages,
    * characters per minute */
   float cpm            = 1000.0f;
   /* Base time for which a line should be shown, in ms */
   float line_duration  = (line_len * 60.0f * 1000.0f) / cpm;
   /* Ticker updates (nominally) once every ticker_pixel_period ms
    * > Return base number of ticks for which text should scroll
    *   from one line to the next */
   return (size_t)(line_duration / ticker_pixel_period);
}

static void set_line_smooth_fade_parameters(
      bool scroll_up, size_t scroll_ticks, size_t line_phase, size_t line_height,
      size_t num_lines, size_t num_display_lines, size_t line_offset, float y_offset,
      size_t *top_fade_line_offset, float *top_fade_y_offset, float *top_fade_alpha,
      size_t *bottom_fade_line_offset, float *bottom_fade_y_offset, float *bottom_fade_alpha)
{
   float fade_out_alpha     = 0.0f;
   float fade_in_alpha      = 0.0f;

   /* When a line fades out, alpha transitions from
    * 1 to 0 over the course of one half of the
    * scrolling line height. When a line fades in,
    * it's the other way around */
   fade_out_alpha           = ((float)scroll_ticks - ((float)line_phase * 2.0f)) / (float)scroll_ticks;
   fade_in_alpha            = -1.0f * fade_out_alpha;
   fade_out_alpha           = (fade_out_alpha < 0.0f) ? 0.0f : fade_out_alpha;
   fade_in_alpha            = (fade_in_alpha < 0.0f)  ? 0.0f : fade_in_alpha;

   *top_fade_line_offset    = (line_offset > 0) ? line_offset - 1 : num_lines;
   *top_fade_y_offset       = y_offset - (float)line_height;
   *top_fade_alpha          = scroll_up ? fade_out_alpha : fade_in_alpha;

   *bottom_fade_line_offset = line_offset + num_display_lines;
   *bottom_fade_y_offset    = y_offset + (float)(line_height * num_display_lines);
   *bottom_fade_alpha       = scroll_up ? fade_in_alpha : fade_out_alpha;
}

static void set_line_smooth_fade_parameters_default(
      size_t *top_fade_line_offset, float *top_fade_y_offset, float *top_fade_alpha,
      size_t *bottom_fade_line_offset, float *bottom_fade_y_offset, float *bottom_fade_alpha)
{
   *top_fade_line_offset    = 0;
   *top_fade_y_offset       = 0.0f;
   *top_fade_alpha          = 0.0f;

   *bottom_fade_line_offset = 0;
   *bottom_fade_y_offset    = 0.0f;
   *bottom_fade_alpha       = 0.0f;
}

static void menu_animation_line_ticker_smooth_generic(uint64_t idx,
      bool fade_enabled, size_t line_len, size_t line_height,
      size_t max_display_lines, size_t num_lines,
      size_t *num_display_lines, size_t *line_offset, float *y_offset,
      bool *fade_active,
      size_t *top_fade_line_offset, float *top_fade_y_offset, float *top_fade_alpha,
      size_t *bottom_fade_line_offset, float *bottom_fade_y_offset, float *bottom_fade_alpha)
{
   size_t scroll_ticks  = get_line_smooth_scroll_ticks(line_len);
   /* Note: This function is only called if num_lines > max_display_lines */
   size_t excess_lines  = num_lines - max_display_lines;
   /* Ticker will pause for one line duration when the first
    * or last line is reached */
   size_t ticker_period = ((excess_lines * 2) + 2) * scroll_ticks;
   size_t phase         = idx % ticker_period;
   size_t line_phase    = 0;
   bool pause           = false;
   bool scroll_up       = true;

   /* Pause on first line */
   if (phase < scroll_ticks)
      pause = true;
   phase = (phase >= scroll_ticks) ? phase - scroll_ticks : 0;
   /* Pause on last line and change direction */
   if (phase >= excess_lines * scroll_ticks)
   {
      scroll_up = false;

      if (phase < (excess_lines + 1) * scroll_ticks)
      {
         pause = true;
         phase = 0;
      }
      else
         phase -= (excess_lines + 1) * scroll_ticks;
   }

   line_phase = phase % scroll_ticks;

   if (pause || (line_phase == 0))
   {
      /* Static display of max_display_lines
       * (no animation) */
      *num_display_lines = max_display_lines;
      *y_offset          = 0.0f;
      *fade_active       = false;

      if (pause)
         *line_offset    = scroll_up ? 0 : excess_lines;
      else
         *line_offset    = scroll_up ? (phase / scroll_ticks) : (excess_lines - (phase / scroll_ticks));
   }
   else
   {
      /* Scroll animation is active */
      *num_display_lines = max_display_lines - 1;
      *fade_active       = fade_enabled;

      if (scroll_up)
      {
         *line_offset    = (phase / scroll_ticks) + 1;
         *y_offset       = (float)line_height * (float)(scroll_ticks - line_phase) / (float)scroll_ticks;
      }
      else
      {
         *line_offset = excess_lines - (phase / scroll_ticks);
         *y_offset    = (float)line_height * (1.0f - (float)(scroll_ticks - line_phase) / (float)scroll_ticks);
      }

      /* Set fade parameters if fade animation is active */
      if (*fade_active)
         set_line_smooth_fade_parameters(
               scroll_up, scroll_ticks, line_phase, line_height,
               num_lines, *num_display_lines, *line_offset, *y_offset,
               top_fade_line_offset, top_fade_y_offset, top_fade_alpha,
               bottom_fade_line_offset, bottom_fade_y_offset, bottom_fade_alpha);
   }

   /* Set 'default' fade parameters if fade animation
    * is inactive */
   if (!*fade_active)
      set_line_smooth_fade_parameters_default(
            top_fade_line_offset, top_fade_y_offset, top_fade_alpha,
            bottom_fade_line_offset, bottom_fade_y_offset, bottom_fade_alpha);
}

static void menu_animation_line_ticker_smooth_loop(uint64_t idx,
      bool fade_enabled, size_t line_len, size_t line_height,
      size_t max_display_lines, size_t num_lines,
      size_t *num_display_lines, size_t *line_offset, float *y_offset,
      bool *fade_active,
      size_t *top_fade_line_offset, float *top_fade_y_offset, float *top_fade_alpha,
      size_t *bottom_fade_line_offset, float *bottom_fade_y_offset, float *bottom_fade_alpha)
{
   size_t scroll_ticks  = get_line_smooth_scroll_ticks(line_len);
   size_t ticker_period = (num_lines + 1) * scroll_ticks;
   size_t phase         = idx % ticker_period;
   size_t line_phase    = phase % scroll_ticks;

   *line_offset         = phase / scroll_ticks;

   if (line_phase == (scroll_ticks - 1))
   {
      /* Static display of max_display_lines
       * (no animation) */
      *num_display_lines = max_display_lines;
      *fade_active       = false;
   }
   else
   {
      *num_display_lines = max_display_lines - 1;
      *fade_active       = fade_enabled;
   }

   *y_offset             = (float)line_height * (float)(scroll_ticks - line_phase) / (float)scroll_ticks;

   /* Set fade parameters */
   if (*fade_active)
      set_line_smooth_fade_parameters(
            true, scroll_ticks, line_phase, line_height,
            num_lines, *num_display_lines, *line_offset, *y_offset,
            top_fade_line_offset, top_fade_y_offset, top_fade_alpha,
            bottom_fade_line_offset, bottom_fade_y_offset, bottom_fade_alpha);
   else
      set_line_smooth_fade_parameters_default(
            top_fade_line_offset, top_fade_y_offset, top_fade_alpha,
            bottom_fade_line_offset, bottom_fade_y_offset, bottom_fade_alpha);
}

static void menu_delayed_animation_cb(void *userdata)
{
   menu_delayed_animation_t *delayed_animation = (menu_delayed_animation_t*) userdata;

   menu_animation_push(&delayed_animation->entry);

   free(delayed_animation);
}

void menu_animation_push_delayed(unsigned delay, menu_animation_ctx_entry_t *entry)
{
   menu_timer_ctx_entry_t timer_entry;
   menu_delayed_animation_t *delayed_animation  = (menu_delayed_animation_t*) malloc(sizeof(menu_delayed_animation_t));

   memcpy(&delayed_animation->entry, entry, sizeof(menu_animation_ctx_entry_t));

   timer_entry.cb       = menu_delayed_animation_cb;
   timer_entry.duration = delay;
   timer_entry.userdata = delayed_animation;

   menu_timer_start(&delayed_animation->timer, &timer_entry);
}

bool menu_animation_push(menu_animation_ctx_entry_t *entry)
{
   struct tween t;

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

   if (!anim.initialized)
   {
      da_init(anim.list);
      da_init(anim.pending);
      anim.initialized = true;
   }

   if (anim.in_update)
      da_push(anim.pending, t);
   else
      da_push(anim.list, t);

   return true;
}

static void menu_animation_update_time(bool timedate_enable, unsigned video_width, unsigned video_height)
{
   static retro_time_t
      last_clock_update       = 0;
   static retro_time_t
      last_ticker_update      = 0;
   static retro_time_t
      last_ticker_slow_update = 0;

   static float ticker_pixel_accumulator  = 0.0f;
   unsigned ticker_pixel_accumulator_uint = 0;
   float ticker_pixel_increment           = 0.0f;

   /* Adjust ticker speed */
   settings_t *settings       = config_get_ptr();
   float speed_factor         = settings->floats.menu_ticker_speed > 0.0001f ? settings->floats.menu_ticker_speed : 1.0f;
   unsigned ticker_speed      = (unsigned)(((float)TICKER_SPEED / speed_factor) + 0.5);
   unsigned ticker_slow_speed = (unsigned)(((float)TICKER_SLOW_SPEED / speed_factor) + 0.5);

   /* Note: cur_time & old_time are in us, delta_time is in ms */
   cur_time   = cpu_features_get_time_usec();
   delta_time = old_time == 0 ? 0.0f : (float)(cur_time - old_time) / 1000.0f;
   old_time   = cur_time;

   if (((cur_time - last_clock_update) > 1000000) /* 1000000 us == 1 second */
         && timedate_enable)
   {
      animation_is_active   = true;
      last_clock_update     = cur_time;
   }

   if (ticker_is_active)
   {
      if (cur_time - last_ticker_update >= ticker_speed)
      {
         ticker_idx++;
         last_ticker_update = cur_time;
      }

      if (cur_time - last_ticker_slow_update >= ticker_slow_speed)
      {
         ticker_slow_idx++;
         last_ticker_slow_update = cur_time;
      }

      /* Pixel ticker updates every frame (regardless of time delta),
       * so requires special handling */

      /* > Get base increment size (+1 every ticker_pixel_period ms) */
      ticker_pixel_increment = delta_time / ticker_pixel_period;

      /* > Apply ticker speed adjustment */
      ticker_pixel_increment *= speed_factor;

      /* > Apply display resolution adjustment
       *   (baseline resolution: 1920x1080)
       *   Note 1: RGUI framebuffer size is independent of
       *   display resolution, so have to use a fixed multiplier.
       *   We choose a value such that text is scrolled
       *   1 pixel every 4 frames when ticker speed is 1x,
       *   which matches almost exactly the scroll speed
       *   of non-smooth ticker text (scrolling 1 pixel
       *   every 2 frames is optimal, but may be too fast
       *   for some users - so play it safe. Users can always
       *   set ticker speed to 2x if they prefer)
       *   Note 2: It turns out that resolution adjustment
       *   also fails for Ozone, because it doesn't implement
       *   any kind of DPI scaling - i.e. text gets smaller
       *   as resolution increases. This is annoying. It
       *   means we have to use a fixed multiplier for
       *   Ozone as well...
       *   Note 3: GLUI uses the new DPI scaling system,
       *   so scaling multiplier is menu_display_get_dpi_scale()
       *   multiplied by a small correction factor (since the
       *   default 1.0x speed is just a little faster than the
       *   non-smooth ticker) */
      if (string_is_equal(settings->arrays.menu_driver, "rgui"))
         ticker_pixel_increment *= 0.25f;
      /* TODO/FIXME: Remove this Ozone special case if/when
       * Ozone gets proper DPI scaling */
      else if (string_is_equal(settings->arrays.menu_driver, "ozone"))
         ticker_pixel_increment *= 0.5f;
      else if (string_is_equal(settings->arrays.menu_driver, "glui"))
         ticker_pixel_increment *= (menu_display_get_dpi_scale(video_width, video_height) * 0.8f);
      else if (video_width > 0)
         ticker_pixel_increment *= ((float)video_width / 1920.0f);

      /* > Update accumulator */
      ticker_pixel_accumulator += ticker_pixel_increment;
      ticker_pixel_accumulator_uint = (unsigned)ticker_pixel_accumulator;

      /* > Check whether we've accumulated enough for an idx update */
      if (ticker_pixel_accumulator_uint > 0)
      {
         ticker_pixel_idx += ticker_pixel_accumulator_uint;
         ticker_pixel_accumulator -= (float)ticker_pixel_accumulator_uint;
      }
   }
}

bool menu_animation_update(unsigned video_width, unsigned video_height)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   menu_animation_update_time(settings->bools.menu_timedate_enable, video_width, video_height);

   anim.in_update       = true;
   anim.pending_deletes = false;

   for (i = 0; i < da_count(anim.list); i++)
   {
      struct tween *tween   = da_getptr(anim.list, i);

      if (!tween || tween->deleted)
         continue;

      tween->running_since += delta_time;

      *tween->subject = tween->easing(
            tween->running_since,
            tween->initial_value,
            tween->target_value - tween->initial_value,
            tween->duration);

      if (tween->running_since >= tween->duration)
      {
         *tween->subject = tween->target_value;

         if (tween->cb)
            tween->cb(tween->userdata);

         da_delete(anim.list, i);
         i--;
      }
   }

   if (anim.pending_deletes)
   {
      for (i = 0; i < da_count(anim.list); i++)
      {
         struct tween *tween = da_getptr(anim.list, i);
         if (!tween)
            continue;
         if (tween->deleted)
         {
            da_delete(anim.list, i);
            i--;
         }
      }
      anim.pending_deletes = false;
   }

   if (da_count(anim.pending) > 0)
   {
      da_addn(anim.list, anim.pending.p, da_count(anim.pending));
      da_clear(anim.pending);
   }

   anim.in_update      = false;
   animation_is_active = da_count(anim.list) > 0;

   return animation_is_active;
}

static void build_ticker_loop_string(
      const char* src_str, const char *spacer,
      unsigned char_offset1, unsigned num_chars1,
      unsigned char_offset2, unsigned num_chars2,
      unsigned char_offset3, unsigned num_chars3,
      char *dest_str, size_t dest_str_len)
{
   char tmp[PATH_MAX_LENGTH];

   tmp[0] = '\0';
   dest_str[0] = '\0';

   /* Copy 'trailing' chunk of source string, if required */
   if (num_chars1 > 0)
   {
      utf8cpy(
            dest_str, dest_str_len,
            utf8skip(src_str, char_offset1), num_chars1);
   }

   /* Copy chunk of spacer string, if required */
   if (num_chars2 > 0)
   {
      utf8cpy(
            tmp, sizeof(tmp),
            utf8skip(spacer, char_offset2), num_chars2);

      strlcat(dest_str, tmp, dest_str_len);
   }

   /* Copy 'leading' chunk of source string, if required */
   if (num_chars3 > 0)
   {
      utf8cpy(
            tmp, sizeof(tmp),
            utf8skip(src_str, char_offset3), num_chars3);

      strlcat(dest_str, tmp, dest_str_len);
   }
}

bool menu_animation_ticker(menu_animation_ctx_ticker_t *ticker)
{
   size_t str_len = utf8len(ticker->str);

   if (!ticker->spacer)
      ticker->spacer = ticker_spacer_default;

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
      utf8cpy(ticker->s,
            PATH_MAX_LENGTH, ticker->str, ticker->len - 3);
      strlcat(ticker->s, "...", ticker->len);
      return false;
   }

   /* Note: If we reach this point then str_len > ticker->len
    * (previously had an unecessary 'if (str_len > ticker->len)'
    * check here...) */
   switch (ticker->type_enum)
   {
      case TICKER_TYPE_LOOP:
      {
         size_t offset1, offset2, offset3;
         size_t width1, width2, width3;
         
         menu_animation_ticker_loop(
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
         
         break;
      }
      case TICKER_TYPE_BOUNCE:
      default:
      {
         size_t offset  = 0;
         
         menu_animation_ticker_generic(
               ticker->idx,
               ticker->len,
               &offset,
               &str_len);
         
         utf8cpy(
               ticker->s,
               PATH_MAX_LENGTH,
               utf8skip(ticker->str, offset),
               str_len);
         
         break;
      }
   }

   ticker_is_active = true;

   return true;
}

/* 'Fixed width' font version of menu_animation_ticker_smooth() */
bool menu_animation_ticker_smooth_fw(menu_animation_ctx_ticker_smooth_t *ticker)
{
   size_t src_str_len           = 0;
   size_t spacer_len            = 0;
   unsigned glyph_width         = ticker->glyph_width;
   unsigned src_str_width       = 0;
   unsigned spacer_width        = 0;
   bool success                 = false;
   bool is_active               = false;

   /* Sanity check has already been performed by
    * menu_animation_ticker_smooth() - no need to
    * repeat */

   /* Get length + width of src string */
   src_str_len = utf8len(ticker->src_str);
   if (src_str_len < 1)
      goto end;

   src_str_width = src_str_len * glyph_width;

   /* If src string width is <= text field width, we
    * can just copy the entire string */
   if (src_str_width <= ticker->field_width)
   {
      utf8cpy(ticker->dst_str, ticker->dst_str_len, ticker->src_str, src_str_len);

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
      unsigned num_chars    = 0;
      unsigned suffix_len   = 3;
      unsigned suffix_width = suffix_len * glyph_width;

      /* Sanity check */
      if (ticker->field_width < suffix_width)
         goto end;

      /* Determine number of characters to copy */
      num_chars = (ticker->field_width - suffix_width) / glyph_width;

      /* Copy string segment + add suffix */
      utf8cpy(ticker->dst_str, ticker->dst_str_len, ticker->src_str, num_chars);
      strlcat(ticker->dst_str, "...", ticker->dst_str_len);

      if (ticker->dst_str_width)
         *ticker->dst_str_width = (num_chars * glyph_width) + suffix_width;
      *ticker->x_offset = 0;
      success = true;
      goto end;
   }

   /* If we get this far, then a scrolling animation
    * is required... */

   /* Use default spacer, if none is provided */
   if (!ticker->spacer)
      ticker->spacer = ticker_spacer_default;

   /* Get length + width of spacer */
   spacer_len = utf8len(ticker->spacer);
   if (spacer_len < 1)
      goto end;

   spacer_width = spacer_len * glyph_width;

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

         menu_animation_ticker_smooth_loop_fw(
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

         ticker->dst_str[0] = '\0';

         menu_animation_ticker_smooth_generic_fw(
               ticker->idx,
               src_str_width, src_str_len, glyph_width, ticker->field_width,
               &char_offset, &num_chars, ticker->x_offset);

         /* Copy required substring */
         if (num_chars > 0)
         {
            utf8cpy(
                  ticker->dst_str, ticker->dst_str_len,
                  utf8skip(ticker->src_str, char_offset), num_chars);
         }

         if (ticker->dst_str_width)
            *ticker->dst_str_width = num_chars * glyph_width;

         break;
      }
   }

   success          = true;
   is_active        = true;
   ticker_is_active = true;

end:

   if (!success)
   {
      *ticker->x_offset = 0;

      if (ticker->dst_str_len > 0)
         ticker->dst_str[0] = '\0';
   }

   return is_active;
}

bool menu_animation_ticker_smooth(menu_animation_ctx_ticker_smooth_t *ticker)
{
   size_t i;
   size_t src_str_len           = 0;
   size_t spacer_len            = 0;
   unsigned src_str_width       = 0;
   unsigned spacer_width        = 0;
   unsigned *src_char_widths    = NULL;
   unsigned *spacer_char_widths = NULL;
   const char *str_ptr          = NULL;
   bool success                 = false;
   bool is_active               = false;

   /* Sanity check */
   if (string_is_empty(ticker->src_str) ||
       (ticker->dst_str_len < 1) ||
       (ticker->field_width < 1) ||
       (!ticker->font && (ticker->glyph_width < 1)))
      goto end;

   /* If we are using a fixed width font (ticker->font == NULL),
    * switch to optimised code path */
   if (!ticker->font)
      return menu_animation_ticker_smooth_fw(ticker);

   /* Find the display width of each character in
    * the src string + total width */
   src_str_len = utf8len(ticker->src_str);
   if (src_str_len < 1)
      goto end;

   src_char_widths = (unsigned*)calloc(src_str_len, sizeof(unsigned));
   if (!src_char_widths)
      goto end;

   str_ptr = ticker->src_str;
   for (i = 0; i < src_str_len; i++)
   {
      int glyph_width = font_driver_get_message_width(
            ticker->font, str_ptr, 1, ticker->font_scale);

      if (glyph_width < 0)
         goto end;

      src_char_widths[i] = (unsigned)glyph_width;
      src_str_width += (unsigned)glyph_width;

      str_ptr = utf8skip(str_ptr, 1);
   }

   /* If total src string width is <= text field width, we
    * can just copy the entire string */
   if (src_str_width <= ticker->field_width)
   {
      utf8cpy(ticker->dst_str, ticker->dst_str_len, ticker->src_str, src_str_len);

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
      unsigned text_width;
      unsigned current_width = 0;
      unsigned num_chars     = 0;
      int period_width       =
            font_driver_get_message_width(ticker->font, ".", 1, ticker->font_scale);

      /* Sanity check */
      if (period_width < 0)
         goto end;

      if (ticker->field_width < (3 * period_width))
         goto end;

      /* Determine number of characters to copy */
      text_width = ticker->field_width - (3 * period_width);
      while (true)
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
      utf8cpy(ticker->dst_str, ticker->dst_str_len, ticker->src_str, num_chars);
      strlcat(ticker->dst_str, "...", ticker->dst_str_len);

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
      ticker->spacer = ticker_spacer_default;

   /* Find the display width of each character in
    * the spacer */
   spacer_len = utf8len(ticker->spacer);
   if (spacer_len < 1)
      goto end;

   spacer_char_widths = (unsigned*)calloc(spacer_len,  sizeof(unsigned));
   if (!spacer_char_widths)
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

         menu_animation_ticker_smooth_loop(
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

         ticker->dst_str[0] = '\0';

         menu_animation_ticker_smooth_generic(
               ticker->idx,
               src_char_widths, src_str_len, src_str_width, ticker->field_width,
               &char_offset, &num_chars, ticker->x_offset, ticker->dst_str_width);

         /* Copy required substring */
         if (num_chars > 0)
         {
            utf8cpy(
                  ticker->dst_str, ticker->dst_str_len,
                  utf8skip(ticker->src_str, char_offset), num_chars);
         }

         break;
      }
   }

   success          = true;
   is_active        = true;
   ticker_is_active = true;

end:

   if (src_char_widths)
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

static void build_line_ticker_string(
      size_t num_display_lines, size_t line_offset, struct string_list *lines,
      char *dest_str, size_t dest_str_len)
{
   size_t i;

   for (i = 0; i < num_display_lines; i++)
   {
      size_t offset     = i + line_offset;
      size_t line_index = offset % (lines->size + 1);
      bool line_valid   = true;

      if (line_index >= lines->size)
         line_valid = false;

      if (line_valid)
         strlcat(dest_str, lines->elems[line_index].data, dest_str_len);

      if (i < num_display_lines - 1)
         strlcat(dest_str, "\n", dest_str_len);
   }
}

bool menu_animation_line_ticker(menu_animation_ctx_line_ticker_t *line_ticker)
{
   char *wrapped_str         = NULL;
   struct string_list *lines = NULL;
   size_t line_offset        = 0;
   bool success              = false;
   bool is_active            = false;

   /* Sanity check */
   if (!line_ticker)
      return false;

   if (string_is_empty(line_ticker->str) ||
       (line_ticker->line_len < 1) ||
       (line_ticker->max_lines < 1))
      goto end;

   /* Line wrap input string */
   wrapped_str = (char*)malloc((strlen(line_ticker->str) + 1) * sizeof(char));
   if (!wrapped_str)
      goto end;

   word_wrap(
         wrapped_str,
         line_ticker->str,
         (int)line_ticker->line_len,
         true, 0);

   if (string_is_empty(wrapped_str))
      goto end;

   /* Split into component lines */
   lines = string_split(wrapped_str, "\n");
   if (!lines)
      goto end;

   /* Check whether total number of lines fits within
    * the set limit */
   if (lines->size <= line_ticker->max_lines)
   {
      strlcpy(line_ticker->s, wrapped_str, line_ticker->len);
      success = true;
      goto end;
   }

   /* Determine offset of first line in wrapped string */
   switch (line_ticker->type_enum)
   {
      case TICKER_TYPE_LOOP:
      {
         menu_animation_line_ticker_loop(
               line_ticker->idx,
               line_ticker->line_len,
               lines->size,
               &line_offset);

         break;
      }
      case TICKER_TYPE_BOUNCE:
      default:
      {
         menu_animation_line_ticker_generic(
               line_ticker->idx,
               line_ticker->line_len,
               line_ticker->max_lines,
               lines->size,
               &line_offset);

         break;
      }
   }

   /* Build output string from required lines */
   build_line_ticker_string(
      line_ticker->max_lines, line_offset, lines,
      line_ticker->s, line_ticker->len);

   success          = true;
   is_active        = true;
   ticker_is_active = true;

end:

   if (wrapped_str)
   {
      free(wrapped_str);
      wrapped_str = NULL;
   }

   if (lines)
   {
      string_list_free(lines);
      lines = NULL;
   }

   if (!success)
      if (line_ticker->len > 0)
         line_ticker->s[0] = '\0';

   return is_active;
}

bool menu_animation_line_ticker_smooth(menu_animation_ctx_line_ticker_smooth_t *line_ticker)
{
   char *wrapped_str              = NULL;
   struct string_list *lines      = NULL;
   int glyph_width                = 0;
   int glyph_height               = 0;
   size_t line_len                = 0;
   size_t max_display_lines       = 0;
   size_t num_display_lines       = 0;
   size_t line_offset             = 0;
   size_t top_fade_line_offset    = 0;
   size_t bottom_fade_line_offset = 0;
   bool fade_active               = false;
   bool success                   = false;
   bool is_active                 = false;

   /* Sanity check */
   if (!line_ticker)
      return false;

   if (!line_ticker->font ||
       string_is_empty(line_ticker->src_str) ||
       (line_ticker->field_width < 1) ||
       (line_ticker->field_height < 1))
      goto end;

   /* Get font dimensions */

   /* > Width
    *   This is a bit of a fudge. Performing a 'font aware'
    *   (i.e. character display width) word wrap is too CPU
    *   intensive, so we just sample the width of a common
    *   character and hope for the best. (We choose 'a' because
    *   this is what Ozone uses for spacing calculations, and
    *   it is proven to work quite well) */
   glyph_width = font_driver_get_message_width(
         line_ticker->font, "a", 1, line_ticker->font_scale);

   if (glyph_width < 0)
      goto end;

   /* > Height */
   glyph_height = font_driver_get_line_height(
         line_ticker->font, line_ticker->font_scale);

   if (glyph_height < 0)
      goto end;

   /* Determine line wrap parameters */
   line_len          = (size_t)(line_ticker->field_width  / glyph_width);
   max_display_lines = (size_t)(line_ticker->field_height / glyph_height);

   if ((line_len < 1) || (max_display_lines < 1))
      goto end;

   /* Line wrap input string */
   wrapped_str = (char*)malloc((strlen(line_ticker->src_str) + 1) * sizeof(char));
   if (!wrapped_str)
      goto end;

   word_wrap(
         wrapped_str,
         line_ticker->src_str,
         (int)line_len,
         true, 0);

   if (string_is_empty(wrapped_str))
      goto end;

   /* Split into component lines */
   lines = string_split(wrapped_str, "\n");
   if (!lines)
      goto end;

   /* Check whether total number of lines fits within
    * the set field limit */
   if (lines->size <= max_display_lines)
   {
      strlcpy(line_ticker->dst_str, wrapped_str, line_ticker->dst_str_len);
      *line_ticker->y_offset = 0.0f;

      /* No fade animation is required */
      if (line_ticker->fade_enabled)
      {
         if (line_ticker->top_fade_str_len > 0)
            line_ticker->top_fade_str[0] = '\0';

         if (line_ticker->bottom_fade_str_len > 0)
            line_ticker->bottom_fade_str[0] = '\0';

         *line_ticker->top_fade_y_offset = 0.0f;
         *line_ticker->bottom_fade_y_offset = 0.0f;

         *line_ticker->top_fade_alpha = 0.0f;
         *line_ticker->bottom_fade_alpha = 0.0f;
      }

      success = true;
      goto end;
   }

   /* Determine which lines should be shown, along with
    * y axis draw offset */
   switch (line_ticker->type_enum)
   {
      case TICKER_TYPE_LOOP:
      {
         menu_animation_line_ticker_smooth_loop(
               line_ticker->idx,
               line_ticker->fade_enabled,
               line_len, (size_t)glyph_height,
               max_display_lines, lines->size,
               &num_display_lines, &line_offset, line_ticker->y_offset,
               &fade_active,
               &top_fade_line_offset, line_ticker->top_fade_y_offset, line_ticker->top_fade_alpha,
               &bottom_fade_line_offset, line_ticker->bottom_fade_y_offset, line_ticker->bottom_fade_alpha);

         break;
      }
      case TICKER_TYPE_BOUNCE:
      default:
      {
         menu_animation_line_ticker_smooth_generic(
               line_ticker->idx,
               line_ticker->fade_enabled,
               line_len, (size_t)glyph_height,
               max_display_lines, lines->size,
               &num_display_lines, &line_offset, line_ticker->y_offset,
               &fade_active,
               &top_fade_line_offset, line_ticker->top_fade_y_offset, line_ticker->top_fade_alpha,
               &bottom_fade_line_offset, line_ticker->bottom_fade_y_offset, line_ticker->bottom_fade_alpha);

         break;
      }
   }

   /* Build output string from required lines */
   build_line_ticker_string(
         num_display_lines, line_offset, lines,
         line_ticker->dst_str, line_ticker->dst_str_len);

   /* Extract top/bottom fade strings, if required */
   if (fade_active)
   {
      /* We waste a handful of clock cycles by using
       * build_line_ticker_string() here, but it saves
       * rewriting a heap of code... */
      build_line_ticker_string(
            1, top_fade_line_offset, lines,
            line_ticker->top_fade_str, line_ticker->top_fade_str_len);

      build_line_ticker_string(
            1, bottom_fade_line_offset, lines,
            line_ticker->bottom_fade_str, line_ticker->bottom_fade_str_len);
   }

   success          = true;
   is_active        = true;
   ticker_is_active = true;

end:

   if (wrapped_str)
   {
      free(wrapped_str);
      wrapped_str = NULL;
   }

   if (lines)
   {
      string_list_free(lines);
      lines = NULL;
   }

   if (!success)
   {
      if (line_ticker->dst_str_len > 0)
         line_ticker->dst_str[0] = '\0';

      if (line_ticker->fade_enabled)
      {
         if (line_ticker->top_fade_str_len > 0)
            line_ticker->top_fade_str[0] = '\0';

         if (line_ticker->bottom_fade_str_len > 0)
            line_ticker->bottom_fade_str[0] = '\0';

         *line_ticker->top_fade_alpha = 0.0f;
         *line_ticker->bottom_fade_alpha = 0.0f;
      }
   }

   return is_active;
}

bool menu_animation_is_active(void)
{
   return animation_is_active || ticker_is_active;
}

bool menu_animation_kill_by_tag(menu_animation_ctx_tag *tag)
{
   unsigned i;

   if (!tag || *tag == (uintptr_t)-1)
      return false;

   for (i = 0; i < da_count(anim.list); ++i)
   {
      struct tween *t = da_getptr(anim.list, i);
      if (!t || t->tag != *tag)
         continue;

      if (anim.in_update)
      {
         t->deleted = true;
         anim.pending_deletes = true;
      }
      else
      {
         da_delete(anim.list, i);
         --i;
      }
   }

   return true;
}

void menu_animation_kill_by_subject(menu_animation_ctx_subject_t *subject)
{
   unsigned i, j,  killed = 0;
   float            **sub = (float**)subject->data;

   for (i = 0; i < da_count(anim.list) && killed < subject->count; ++i)
   {
      struct tween *t = da_getptr(anim.list, i);
      if (!t)
         continue;

      for (j = 0; j < subject->count; ++j)
      {
         if (t->subject != sub[j])
            continue;

         if (anim.in_update)
         {
            t->deleted = true;
            anim.pending_deletes = true;
         }
         else
         {
            da_delete(anim.list, i);
            --i;
         }

         killed++;
         break;
      }
   }
}

float menu_animation_get_delta_time(void)
{
   return delta_time;
}

bool menu_animation_ctl(enum menu_animation_ctl_state state, void *data)
{
   switch (state)
   {
      case MENU_ANIMATION_CTL_DEINIT:
         {
            size_t i;

            for (i = 0; i < da_count(anim.list); i++)
            {
               struct tween *t = da_getptr(anim.list, i);
               if (!t)
                  continue;

               if (t->subject)
                  t->subject = NULL;
            }

            da_free(anim.list);
            da_free(anim.pending);

            memset(&anim, 0, sizeof(menu_animation_t));
         }
         cur_time                  = 0;
         old_time                  = 0;
         delta_time                = 0.0f;
         break;
      case MENU_ANIMATION_CTL_CLEAR_ACTIVE:
         animation_is_active       = false;
         ticker_is_active          = false;
         break;
      case MENU_ANIMATION_CTL_SET_ACTIVE:
         animation_is_active       = true;
         ticker_is_active          = true;
         break;
      case MENU_ANIMATION_CTL_NONE:
      default:
         break;
   }

   return true;
}

void menu_timer_start(menu_timer_t *timer, menu_timer_ctx_entry_t *timer_entry)
{
   menu_animation_ctx_entry_t entry;
   menu_animation_ctx_tag tag = (uintptr_t) timer;

   menu_timer_kill(timer);

   *timer = 0.0f;

   entry.easing_enum    = EASING_LINEAR;
   entry.tag            = tag;
   entry.duration       = timer_entry->duration;
   entry.target_value   = 1.0f;
   entry.subject        = timer;
   entry.cb             = timer_entry->cb;
   entry.userdata       = timer_entry->userdata;

   menu_animation_push(&entry);
}

void menu_timer_kill(menu_timer_t *timer)
{
   menu_animation_ctx_tag tag = (uintptr_t) timer;
   menu_animation_kill_by_tag(&tag);
}

uint64_t menu_animation_get_ticker_idx(void)
{
   return ticker_idx;
}

uint64_t menu_animation_get_ticker_slow_idx(void)
{
   return ticker_slow_idx;
}

uint64_t menu_animation_get_ticker_pixel_idx(void)
{
   return ticker_pixel_idx;
}
