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
#include "../input_keymaps.h"
#include "../../gfx/video_driver.h"

#if defined(HW_RVL) && defined(GEKKO_NATIVE)
#include <gekko/keyboard.h>
#include <gekko/mouse.h>
#define GX_KEYBOARD
#define GX_USB_MOUSE
#endif

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
   /* One per port; the joypad driver says which point anywhere. */
   gx_input_mouse_t mouse[DEFAULT_MAX_PADS];
#else
   void *empty;
#endif
#ifdef GX_USB_MOUSE
   gk_mouse_t usb_mouse;                     /* since the last poll */
#endif
#ifdef GX_KEYBOARD
   uint8_t key_down[(RETROK_LAST + 7) / 8];  /* by RETROK_* */
   uint8_t keys[6];                          /* USB keyboard usages */
   uint8_t modifiers;
   bool    caps_lock;
#endif
} gx_input_t;

#ifdef GX_KEYBOARD
/* US layout: usages 0x04 (a) to 0x38 (/), unshifted and shifted. */
static const char kbd_chars[2][0x39 - 0x04 + 1] = {
   "abcdefghijklmnopqrstuvwxyz1234567890\n\x1b\b\t -=[]\\#;'`,./",
   "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()\n\x1b\b\t _+{}|~:\"~<>?"
};

static uint16_t kbd_mod(const gx_input_t *gx)
{
   uint16_t mod = 0;
   if (gx->modifiers & (GK_KBD_LSHIFT | GK_KBD_RSHIFT))
      mod |= RETROKMOD_SHIFT;
   if (gx->modifiers & (GK_KBD_LCTRL | GK_KBD_RCTRL))
      mod |= RETROKMOD_CTRL;
   if (gx->modifiers & (GK_KBD_LALT | GK_KBD_RALT))
      mod |= RETROKMOD_ALT;
   if (gx->modifiers & (GK_KBD_LSUPER | GK_KBD_RSUPER))
      mod |= RETROKMOD_META;
   if (gx->caps_lock)
      mod |= RETROKMOD_CAPSLOCK;
   return mod;
}

static void kbd_key(gx_input_t *gx, uint8_t usage, bool down)
{
   uint32_t c    = 0;
   unsigned code = input_keymaps_translate_keysym_to_rk(usage);
   if (!code || code >= RETROK_LAST)
      return;
   if (down)
      gx->key_down[code >> 3] |=  (uint8_t)(1 << (code & 7));
   else
      gx->key_down[code >> 3] &= ~(uint8_t)(1 << (code & 7));
   if (down && code == RETROK_CAPSLOCK)
      gx->caps_lock = !gx->caps_lock;
   if (down && usage >= 0x04 && usage <= 0x38)
   {
      bool shift = (gx->modifiers & (GK_KBD_LSHIFT | GK_KBD_RSHIFT)) != 0;
      /* Caps Lock shifts the letters only */
      if (usage <= 0x1d && gx->caps_lock)
         shift = !shift;
      c = (uint8_t)kbd_chars[shift ? 1 : 0][usage - 0x04];
   }
   else if (down && usage == 0x4c)   /* Delete */
      c = 0x7f;
   input_keyboard_event(down, code, c, kbd_mod(gx), RETRO_DEVICE_KEYBOARD);
}

static bool kbd_pressed(const gx_input_t *gx, unsigned key)
{
   return key && key < RETROK_LAST
      && (gx->key_down[key >> 3] & (1 << (key & 7)));
}

static bool kbd_held(const uint8_t *keys, uint8_t usage)
{
   unsigned i;
   for (i = 0; i < 6; i++)
      if (keys[i] == usage)
         return true;
   return false;
}

/* What changed between the keys and modifiers held before and now,
 * as key events. */
static void kbd_update(gx_input_t *gx, uint8_t modifiers,
      const uint8_t *keys)
{
   unsigned i;
   uint8_t changed = gx->modifiers ^ modifiers;
   for (i = 0; i < 8; i++)
      if (changed & (1 << i))
      {
         gx->modifiers ^= (uint8_t)(1 << i);
         kbd_key(gx, (uint8_t)(0xe0 + i), (modifiers >> i) & 1);
      }
   for (i = 0; i < 6; i++)
      if (gx->keys[i] && !kbd_held(keys, gx->keys[i]))
         kbd_key(gx, gx->keys[i], false);
   for (i = 0; i < 6; i++)
      if (keys[i] && !kbd_held(gx->keys, keys[i]))
         kbd_key(gx, keys[i], true);
   memcpy(gx->keys, keys, sizeof(gx->keys));
}

static void kbd_poll(gx_input_t *gx)
{
   static const uint8_t none[6] = { 0 };
   gk_kbd_event_t ev;
   while (gk_kbd_read(&ev))
   {
      if (ev.type == GK_KBD_DISCONNECT)
         kbd_update(gx, 0, none);
      /* Too many keys at once reads as usage 1 everywhere */
      else if (ev.type == GK_KBD_KEYS && ev.keys[0] != 0x01)
         kbd_update(gx, ev.modifiers, ev.keys);
   }
}
#endif

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

   if (     port >= DEFAULT_MAX_PADS || !gx
         || joypad_info->joy_idx >= DEFAULT_MAX_PADS)
      return 0;

   switch (device)
   {
#ifdef GX_KEYBOARD
      case RETRO_DEVICE_JOYPAD:
         /* The keyboard's binds */
         if (!binds)
            break;
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            if (!keyboard_mapping_blocked)
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
                  if (     RETRO_KEYBIND_VALID(&binds[port][i])
                        && kbd_pressed(gx, RETRO_KEYBIND_KEY(&binds[port][i])))
                     ret |= (1 << i);
            return ret;
         }
         if (     id < RARCH_BIND_LIST_END
               && RETRO_KEYBIND_VALID(&binds[port][id])
               && kbd_pressed(gx, RETRO_KEYBIND_KEY(&binds[port][id]))
               && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked))
            return 1;
         break;
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_KEYBOARD:
         return kbd_pressed(gx, id) ? 1 : 0;
#else
      case RETRO_DEVICE_JOYPAD:
      case RETRO_DEVICE_ANALOG:
         break;
#endif
      case RETRO_DEVICE_MOUSE:
#ifdef GX_USB_MOUSE
         /* USB mice, all as one, are mouse 0 while there are any. */
         if (     gx->usb_mouse.count
               && config_get_ptr()->uints.input_mouse_index[port] == 0)
         {
            const gk_mouse_t *m = &gx->usb_mouse;
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_X:
                  return (int16_t)m->dx;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  return (int16_t)m->dy;
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return (m->buttons & GK_MOUSE_LEFT) ? 1 : 0;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return (m->buttons & GK_MOUSE_RIGHT) ? 1 : 0;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return (m->buttons & GK_MOUSE_MIDDLE) ? 1 : 0;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
                  return (m->buttons & GK_MOUSE_4) ? 1 : 0;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
                  return (m->buttons & GK_MOUSE_5) ? 1 : 0;
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  return m->wheel > 0 ? 1 : 0;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  return m->wheel < 0 ? 1 : 0;
               default:
                  break;
            }
            break;
         }
#endif
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
            int16_t x                   = gx->mouse[joy_idx].x_abs;
            int16_t y                   = gx->mouse[joy_idx].y_abs;

            /* Pre-patch had a video_driver_get_viewport_info(&vp)
             * call here, immediately followed by six lines that
             * zeroed every field it populated. Net effect was the
             * zero-initialiser above. The call dispatched through
             * the video vtable on every lightgun query - delete it. */

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
   free(data);
}


static void *gx_input_init(const char *joypad_driver)
{
#ifdef GX_KEYBOARD
   input_keymaps_init_keyboard_lut(rarch_key_map_hid);
#endif
   return calloc(1, sizeof(gx_input_t));
}

#ifdef HW_RVL
static void rvl_input_poll(void *data)
{
   unsigned i;
   gx_input_t *gx = (gx_input_t*)data;
   if (!gx)
      return;
   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      gx->mouse[i].x_last = gx->mouse[i].x_abs;
      gx->mouse[i].y_last = gx->mouse[i].y_abs;
      gx_joypad_read_mouse(i, &gx->mouse[i].x_abs, &gx->mouse[i].y_abs,
            &gx->mouse[i].button);
   }
#ifdef GX_KEYBOARD
   kbd_poll(gx);
#endif
#ifdef GX_USB_MOUSE
   gk_mouse_read(&gx->usb_mouse);
#endif
}

static uint64_t rvl_input_get_capabilities(void *data)
{
   return   (1 << RETRO_DEVICE_JOYPAD)
          | (1 << RETRO_DEVICE_ANALOG)
#ifdef GX_KEYBOARD
          | (1 << RETRO_DEVICE_KEYBOARD)
#endif
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
