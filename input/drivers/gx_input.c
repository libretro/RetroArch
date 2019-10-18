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

#include <libretro.h>

#include "../../config.def.h"

#include "../input_driver.h"

/* TODO/FIXME -
 * fix game focus toggle */

#ifdef HW_RVL
/* gx joypad functions */
bool gxpad_mousevalid(unsigned port);
void gx_joypad_read_mouse(unsigned port, int *irx, int *iry, uint32_t *button);

typedef struct
{
   int x_abs, y_abs;
   int x_last, y_last;
   uint32_t button;
} gx_input_mouse_t;
#endif

typedef struct gx_input
{
   const input_device_driver_t *joypad;
#ifdef HW_RVL
   int mouse_max;
   gx_input_mouse_t *mouse;
#endif
} gx_input_t;

#ifdef HW_RVL
static int16_t gx_lightgun_state(gx_input_t *gx, unsigned id, uint16_t joy_idx)
{
   struct video_viewport vp = {0};
   video_driver_get_viewport_info(&vp);
   int16_t res_x               = 0;
   int16_t res_y               = 0;
   int16_t res_screen_x        = 0;
   int16_t res_screen_y        = 0;
   int16_t x = 0;
   int16_t y = 0;

   vp.x                        = 0;
   vp.y                        = 0;
   vp.width                    = 0;
   vp.height                   = 0;
   vp.full_width               = 0;
   vp.full_height              = 0;

   x = gx->mouse[joy_idx].x_abs;
   y = gx->mouse[joy_idx].y_abs;

   if (!(video_driver_translate_coord_viewport_wrap(&vp, x, y,
         &res_x, &res_y, &res_screen_x, &res_screen_y)))
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
         return res_screen_x;
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
         return res_screen_y;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return gx->mouse[joy_idx].button & (1 << RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
         return gx->mouse[joy_idx].button & (1 << RETRO_DEVICE_ID_LIGHTGUN_AUX_A);
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
         return gx->mouse[joy_idx].button & (1 << RETRO_DEVICE_ID_LIGHTGUN_AUX_B);
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
         return gx->mouse[joy_idx].button & (1 << RETRO_DEVICE_ID_LIGHTGUN_AUX_C);
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return gx->mouse[joy_idx].button & (1 << RETRO_DEVICE_ID_LIGHTGUN_START);
      case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
         return gx->mouse[joy_idx].button & (1 << RETRO_DEVICE_ID_LIGHTGUN_SELECT);
      case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
         return !gxpad_mousevalid(joy_idx);
      default:
         return 0;
   }

   return 0;
}

static int16_t gx_mouse_state(gx_input_t *gx, unsigned id, uint16_t joy_idx)
{
   int x = 0;
   int y = 0;

   settings_t *settings = config_get_ptr();
   int x_scale = settings->uints.input_mouse_scale;
   int y_scale = settings->uints.input_mouse_scale;

   x = (gx->mouse[joy_idx].x_abs - gx->mouse[joy_idx].x_last) * x_scale;
   y = (gx->mouse[joy_idx].y_abs - gx->mouse[joy_idx].y_last) * y_scale;

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return gx->mouse[joy_idx].button & (1 << RETRO_DEVICE_ID_MOUSE_LEFT);
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return gx->mouse[joy_idx].button & (1 << RETRO_DEVICE_ID_MOUSE_RIGHT);
      default:
         return 0;
   }
}
#endif

static int16_t gx_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   gx_input_t *gx             = (gx_input_t*)data;

   if (port >= DEFAULT_MAX_PADS || !gx)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               /* Auto-binds are per joypad, not per user. */
               const uint64_t joykey  = (binds[port][i].joykey != NO_BTN)
                  ? binds[port][i].joykey : joypad_info.auto_binds[i].joykey;
               const uint32_t joyaxis = (binds[port][i].joyaxis != AXIS_NONE)
                  ? binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis;

               if ((uint16_t)joykey != NO_BTN && gx->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(gx->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               {
                  ret |= (1 << i);
                  continue;
               }
            }

            return ret;
         }
         else
         {
            /* Auto-binds are per joypad, not per user. */
            const uint64_t joykey  = (binds[port][id].joykey != NO_BTN)
               ? binds[port][id].joykey : joypad_info.auto_binds[id].joykey;
            const uint32_t joyaxis = (binds[port][id].joyaxis != AXIS_NONE)
               ? binds[port][id].joyaxis : joypad_info.auto_binds[id].joyaxis;

            if ((uint16_t)joykey != NO_BTN && gx->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(gx->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(gx->joypad,
                  joypad_info, port, idx, id, binds[port]);
         break;
#ifdef HW_RVL
      case RETRO_DEVICE_MOUSE:
         return gx_mouse_state(gx, id, joypad_info.joy_idx);

      case RETRO_DEVICE_LIGHTGUN:
         return gx_lightgun_state(gx, id, joypad_info.joy_idx);
#endif
   }

   return 0;
}

static void gx_input_free_input(void *data)
{
   gx_input_t *gx = (gx_input_t*)data;

   if (!gx)
      return;

   if (gx->joypad)
      gx->joypad->destroy();
#ifdef HW_RVL
   if(gx->mouse)
      free(gx->mouse);
#endif
   free(gx);
}

#ifdef HW_RVL
static inline int gx_count_mouse(gx_input_t *gx)
{
   int count = 0;

   if(gx)
   {
      for(int i=0; i<DEFAULT_MAX_PADS; i++)
      {
         if(gx->joypad->name(i))
         {
            if(!strcmp(gx->joypad->name(i), "Wiimote Controller"))
            {
               count++;
            }
         }
      }
   }

   return count;
}
#endif

static void *gx_input_init(const char *joypad_driver)
{
   gx_input_t *gx = (gx_input_t*)calloc(1, sizeof(*gx));
   if (!gx)
      return NULL;

   gx->joypad = input_joypad_init_driver(joypad_driver, gx);
#ifdef HW_RVL
   /* Allocate at least 1 mouse at startup */
   gx->mouse_max = 1;
   gx->mouse = (gx_input_mouse_t*) calloc(gx->mouse_max, sizeof(gx_input_mouse_t));
#endif
   return gx;
}

#ifdef HW_RVL
static void gx_input_poll_mouse(gx_input_t *gx)
{
   int count = 0;
   count = gx_count_mouse(gx);

   if(gx && count > 0)
   {
      if(count != gx->mouse_max)
      {
         gx_input_mouse_t* tmp = NULL;

         tmp = (gx_input_mouse_t*)realloc(gx->mouse, count * sizeof(gx_input_mouse_t));
         if(!tmp) 
         {
            free(gx->mouse);
         }
         else
         {
            gx->mouse = tmp;
            gx->mouse_max = count;

            for(int i=0; i<gx->mouse_max; i++)
            {
               gx->mouse[i].x_last = 0;
               gx->mouse[i].y_last = 0;
            }
         }
      }
      
      for(unsigned i=0; i<gx->mouse_max; i++)
      {
         gx->mouse[i].x_last = gx->mouse[i].x_abs;
         gx->mouse[i].y_last = gx->mouse[i].y_abs;
         gx_joypad_read_mouse(i, &gx->mouse[i].x_abs, &gx->mouse[i].y_abs, &gx->mouse[i].button);
      } 
   }
}
#endif

static void gx_input_poll(void *data)
{
   gx_input_t *gx = (gx_input_t*)data;

   if (gx && gx->joypad)
   {
      gx->joypad->poll();
#ifdef HW_RVL
      if(gx->mouse)
         gx_input_poll_mouse(gx);
#endif
   }
}

static uint64_t gx_input_get_capabilities(void *data)
{
   (void)data;
#ifdef HW_RVL
   return (1 << RETRO_DEVICE_JOYPAD) |
          (1 << RETRO_DEVICE_ANALOG) |
          (1 << RETRO_DEVICE_MOUSE) |
          (1 << RETRO_DEVICE_LIGHTGUN);
#else
   return (1 << RETRO_DEVICE_JOYPAD) |
          (1 << RETRO_DEVICE_ANALOG);
#endif
}

static const input_device_driver_t  *gx_input_get_joypad_driver(void *data)
{
   gx_input_t *gx = (gx_input_t*)data;
   if (!gx)
      return NULL;
   return gx->joypad;
}

static void gx_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool gx_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

input_driver_t input_gx = {
   gx_input_init,
   gx_input_poll,
   gx_input_state,
   gx_input_free_input,
   NULL,
   NULL,
   gx_input_get_capabilities,
   "gx",

   gx_input_grab_mouse,
   NULL,
   gx_input_set_rumble,
   gx_input_get_joypad_driver,
   NULL,
   false
};
