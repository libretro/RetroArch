/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - The RetroArch team
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

/* The alpha each image of the overlay's page is handed: the page's
 * opacity, a pressed desc's highlight, and 0 for an image the LED
 * driver hides. Kept apart from input_driver.c so that
 * samples/tasks/overlay can compile this file as it is against a stub
 * driver.
 *
 * The pass runs at every input poll; an image is set only when its
 * alpha has changed since it was last handed over. A driver may pay
 * for a set - D3D10/11/12 map the sprite buffer for each one, and the
 * threaded wrapper replays every image's alpha when any is set.
 *
 * A pack that names its LED images (overlayN_descM_led = K) is the
 * only source: the image of a desc naming LED K shows while K is lit.
 * ledN_map, which names a slot of whatever page is loaded, is for a
 * pack that names none, and reaches the image at that slot whatever
 * its desc does when pressed: an LED pack may put its lights on keys
 * or buttons. */

#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_OVERLAY

#include "input_overlay.h"
#include "../led/led_defines.h"

/* Hands image @i its alpha, unless it already has it. */
static void input_overlay_set_image_alpha(input_overlay_t *ol,
      unsigned i, float alpha)
{
   if (ol->alpha_sent && i < ol->alpha_cap)
   {
      if (ol->alpha_sent[i] == alpha)
         return;
      ol->alpha_sent[i] = alpha;
   }
   ol->iface->set_alpha(ol->iface_data, i, alpha);
}

void input_overlay_alpha_forget(input_overlay_t *ol)
{
   size_t i;
   if (!ol || !ol->alpha_sent)
      return;
   /* No alpha is negative: every image is set at the next pass. */
   for (i = 0; i < ol->alpha_cap; i++)
      ol->alpha_sent[i] = -1.0f;
}

bool input_overlay_image_hidden(const input_overlay_t *ol,
      unsigned image, uint32_t lit, const unsigned *led_map)
{
   size_t i;

   if (!led_map || !ol || !ol->active)
      return false;

   if (ol->flags & INPUT_OVERLAY_HAS_LEDS)
   {
      for (i = 0; i < ol->active->size; i++)
      {
         const struct overlay_desc *desc = &ol->active->descs[i];
         if (     desc->led
               && desc->image_index == image
               && OVERLAY_HAS_IMAGE(&desc->image)
               && !(lit & (1u << (desc->led - 1))))
            return true;
      }
      return false;
   }

   for (i = 0; i < MAX_LEDS; i++)
      if (led_map[i] == image && !(lit & (1u << i)))
         return true;
   return false;
}

void input_overlay_hide_leds(input_overlay_t *ol,
      uint32_t lit, const unsigned *led_map)
{
   size_t i;

   if (!led_map || !ol || !ol->active || !ol->iface->set_alpha)
      return;

   if (ol->flags & INPUT_OVERLAY_HAS_LEDS)
   {
      for (i = 0; i < ol->active->size; i++)
      {
         const struct overlay_desc *desc = &ol->active->descs[i];
         if (     desc->led
               && OVERLAY_HAS_IMAGE(&desc->image)
               && !(lit & (1u << (desc->led - 1))))
            input_overlay_set_image_alpha(ol, desc->image_index, 0.0f);
      }
      return;
   }

   /* A map entry is read from the config with no range of its own
    * (unmapped is (unsigned)-1): only a slot the page has is touched. */
   for (i = 0; i < MAX_LEDS; i++)
      if (     led_map[i] < ol->active->load_images_size
            && !(lit & (1u << i)))
         input_overlay_set_image_alpha(ol, led_map[i], 0.0f);
}

void input_overlay_alpha_pass(input_overlay_t *ol, float mod,
      bool show_input, float opacity,
      uint32_t lit, const unsigned *led_map)
{
   size_t i;
   size_t n;
   float *want;

   if (!ol || !ol->active || !ol->iface->set_alpha)
      return;

   if (ol->flags & INPUT_OVERLAY_GAMEPAD_HIDDEN)
      mod  = 0.0f;
   n       = ol->active->load_images_size;
   want    = (ol->alpha_want && n <= ol->alpha_cap) ? ol->alpha_want : NULL;

   /* Without the scratch, every image is set as it is worked out, and
    * a pressed one twice. */
   for (i = 0; i < n; i++)
   {
      float a = (led_map && input_overlay_image_hidden(ol,
                  (unsigned)i, lit, led_map)) ? 0.0f : mod;
      if (want)
         want[i] = a;
      else
         ol->iface->set_alpha(ol->iface_data, (unsigned)i, a);
   }

   /* A pressed desc is lit, unless the LED driver has hidden its
    * image: that stays hidden whatever is pressed. */
   if (show_input)
   {
      for (i = 0; i < ol->active->size; i++)
      {
         const struct overlay_desc *desc = &ol->active->descs[i];
         if (     !desc->touch_mask
               || !OVERLAY_HAS_IMAGE(&desc->image)
               || desc->image_index >= n
               || (led_map && input_overlay_image_hidden(ol,
                     desc->image_index, lit, led_map)))
            continue;
         if (want)
            want[desc->image_index] = desc->alpha_mod * opacity;
         else
            ol->iface->set_alpha(ol->iface_data, desc->image_index,
                  desc->alpha_mod * opacity);
      }
   }

   if (want)
      for (i = 0; i < n; i++)
         input_overlay_set_image_alpha(ol, (unsigned)i, want[i]);
}

#endif
