/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <retro_inline.h>

#include <libretro.h>

#include "../../config.def.h"

#include "../input_driver.h"

/* TODO/FIXME -
 * fix game focus toggle */

#ifdef HW_RVL
/* gx joypad functions */
bool gxpad_mousevalid(unsigned port);

void gx_joypad_read_mouse(unsigned port,
      int *irx, int *iry, uint32_t *button);

typedef struct
{
   int x_abs, y_abs;
   int x_last, y_last;
   uint32_t button;
} gx_input_mouse_t;
#endif

typedef struct gx_input
{
#ifdef HW_RVL
   gx_input_mouse_t *mouse;
   int mouse_max;
#else
   void *empty;
#endif
} gx_input_t;

#ifdef HW_RVL
static int16_t rvl_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   gx_input_t *gx             = (gx_input_t*)data;

   if (port >= DEFAULT_MAX_PADS || !gx)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_MOUSE:
         {
            settings_t *settings       = config_get_ptr();
            uint16_t joy_idx           = joypad_info->joy_idx;
            unsigned input_mouse_scale = settings->uints.input_mouse_scale;
            int x_scale                = input_mouse_scale;
            int y_scale                = input_mouse_scale;
            int x                      = (gx->mouse[joy_idx].x_abs 
                  - gx->mouse[joy_idx].x_last) * x_scale;
            int y                      = (gx->mouse[joy_idx].y_abs 
                  - gx->mouse[joy_idx].y_last) * y_scale;

            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_X:
                  return x;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  return y;
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return gx->mouse[joy_idx].button & 
                     (1 << RETRO_DEVICE_ID_MOUSE_LEFT);
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return gx->mouse[joy_idx].button & 
                     (1 << RETRO_DEVICE_ID_MOUSE_RIGHT);
               default:
                  break;
            }
         }
         break;
      case RETRO_DEVICE_LIGHTGUN:
         {
            struct video_viewport vp    = {0};
            uint16_t joy_idx            = joypad_info->joy_idx;
            int16_t res_x               = 0;
            int16_t res_y               = 0;
            int16_t res_screen_x        = 0;
            int16_t res_screen_y        = 0;
            int16_t x                   = 0;
            int16_t y                   = 0;

            video_driver_get_viewport_info(&vp);

            vp.x                        = 0;
            vp.y                        = 0;
            vp.width                    = 0;
            vp.height                   = 0;
            vp.full_width               = 0;
            vp.full_height              = 0;

            x                           = gx->mouse[joy_idx].x_abs;
            y                           = gx->mouse[joy_idx].y_abs;

            if (video_driver_translate_coord_viewport_wrap(&vp, x, y,
                        &res_x, &res_y, &res_screen_x, &res_screen_y))
            {
               switch (id)
               {
                  case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
                     return res_screen_x;
                  case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                     return res_screen_y;
                  case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
                     return gx->mouse[joy_idx].button & 
                        (1 << RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
                  case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
                     return gx->mouse[joy_idx].button & 
                        (1 << RETRO_DEVICE_ID_LIGHTGUN_AUX_A);
                  case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
                     return gx->mouse[joy_idx].button & 
                        (1 << RETRO_DEVICE_ID_LIGHTGUN_AUX_B);
                  case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
                     return gx->mouse[joy_idx].button & 
                        (1 << RETRO_DEVICE_ID_LIGHTGUN_AUX_C);
                  case RETRO_DEVICE_ID_LIGHTGUN_START:
                     return gx->mouse[joy_idx].button & 
                        (1 << RETRO_DEVICE_ID_LIGHTGUN_START);
                  case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
                     return gx->mouse[joy_idx].button & 
                        (1 << RETRO_DEVICE_ID_LIGHTGUN_SELECT);
                  case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                     return !gxpad_mousevalid(joy_idx);
                  default:
                     break;
               }
            }
         }
         break;
   }

   return 0;
}
#endif

static void gx_input_free_input(void *data)
{
   gx_input_t *gx = (gx_input_t*)data;

   if (!gx)
      return;

#ifdef HW_RVL
   if (gx->mouse)
      free(gx->mouse);
#endif
   free(gx);
}


static void *gx_input_init(const char *joypad_driver)
{
   gx_input_t *gx = (gx_input_t*)calloc(1, sizeof(*gx));
   if (!gx)
      return NULL;

#ifdef HW_RVL
   /* Allocate at least 1 mouse at startup */
   gx->mouse_max  = 1;
   gx->mouse      = (gx_input_mouse_t*)calloc(
         gx->mouse_max, sizeof(gx_input_mouse_t));
#endif

   return gx;
}

#ifdef HW_RVL
static INLINE int rvl_count_mouse(gx_input_t *gx)
{
   unsigned i;
   int count = 0;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      const char *joypad_name = joypad_driver_name(i);
      if (!string_is_empty(joypad_name))
         if (string_is_equal(joypad_name, "Wiimote Controller"))
            count++;
   }

   return count;
}

static void rvl_input_poll(void *data)
{
   gx_input_t *gx = (gx_input_t*)data;
   if (gx && gx->mouse)
   {
      int count = rvl_count_mouse(gx);

      if (gx && count > 0)
      {
         unsigned i;
         if (count != gx->mouse_max)
         {
            gx_input_mouse_t *tmp = (gx_input_mouse_t*)realloc(
                  gx->mouse, count * sizeof(gx_input_mouse_t));
            if (!tmp) 
               free(gx->mouse);
            else
            {
               unsigned i;
               gx->mouse     = tmp;
               gx->mouse_max = count;

               for (i = 0; i < gx->mouse_max; i++)
               {
                  gx->mouse[i].x_last = 0;
                  gx->mouse[i].y_last = 0;
               }
            }
         }

         for (i = 0; i < gx->mouse_max; i++)
         {
            gx->mouse[i].x_last = gx->mouse[i].x_abs;
            gx->mouse[i].y_last = gx->mouse[i].y_abs;
            gx_joypad_read_mouse(i, &gx->mouse[i].x_abs,
                  &gx->mouse[i].y_abs, &gx->mouse[i].button);
         } 
      }
   }
}

static uint64_t rvl_input_get_capabilities(void *data)
{
   return   (1 << RETRO_DEVICE_JOYPAD)
          | (1 << RETRO_DEVICE_ANALOG)
          | (1 << RETRO_DEVICE_MOUSE)
          | (1 << RETRO_DEVICE_LIGHTGUN);
}
#else
static uint64_t gx_input_get_capabilities(void *data)
{
   return   (1 << RETRO_DEVICE_JOYPAD)
          | (1 << RETRO_DEVICE_ANALOG);
}
#endif

input_driver_t input_gx = {
   gx_input_init,
#ifdef HW_RVL
   rvl_input_poll,
   rvl_input_state,
#else
   NULL,                         /* poll */
   NULL,                         /* input_state */
#endif
   gx_input_free_input,
   NULL,
   NULL,
#ifdef HW_RVL
   rvl_input_get_capabilities,
#else
   gx_input_get_capabilities,
#endif
   "gx",

   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
