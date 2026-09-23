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

/* Which images of the overlay the LED driver hides. Kept apart from
 * input_driver.c so that samples/tasks/overlay can compile this file
 * as it is against a stub driver.
 *
 * A pack that names its LED images (overlayN_descM_led = K) is the
 * only source: the image of a desc naming LED K shows while K is lit.
 * ledN_map, which names a slot of whatever page is loaded, is for a
 * pack that names none, and reaches only the images of "nul" buttons:
 * the same config applied to a gamepad pack would otherwise blank
 * whichever controls sit at those slots. */

#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_OVERLAY

#include "input_overlay.h"
#include "../led/led_defines.h"

/* Image @image of @page belongs to a desc nothing happens to when it
 * is pressed. */
static bool input_overlay_image_display_only(const struct overlay *page,
      unsigned image)
{
   size_t i;
   for (i = 0; i < page->size; i++)
   {
      const struct overlay_desc *desc = &page->descs[i];
      if (     desc->image_index == image
            && OVERLAY_HAS_IMAGE(&desc->image))
         return (desc->flags & OVERLAY_DESC_DISPLAY_ONLY) != 0;
   }
   return false;
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
         return input_overlay_image_display_only(ol->active, image);
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
            ol->iface->set_alpha(ol->iface_data, desc->image_index, 0.0f);
      }
      return;
   }

   /* A map entry is read from the config with no range of its own
    * (unmapped is (unsigned)-1): only a slot the page has is touched. */
   for (i = 0; i < MAX_LEDS; i++)
      if (     led_map[i] < ol->active->load_images_size
            && !(lit & (1u << i))
            && input_overlay_image_display_only(ol->active, led_map[i]))
         ol->iface->set_alpha(ol->iface_data, led_map[i], 0.0f);
}

#endif
