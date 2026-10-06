/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Higor Euripedes
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

#include <boolean.h>
#include <string/stdstring.h>
#include <libretro.h>

#include "SDL.h"

#include "../input_keymaps.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../gfx/common/sdl2_common.h"

#ifdef __linux__
#include "../common/linux_common.h"
#endif

#ifdef WEBOS
#include <SDL_webOS.h>
#include <dlfcn.h>
#endif

typedef struct sdl2_input
{
#ifdef __linux__
   /* Light sensors aren't exposed through SDL, and they're not usually part of controllers */
   linux_illuminance_sensor_t *illuminance_sensor;
#endif
   uint32_t mouse_rel;   /* VIDEO_POS_PACK */
   uint32_t mouse_abs;   /* VIDEO_POS_PACK */
   int mouse_l;
   int mouse_r;
   int mouse_m;
   int mouse_b4;
   int mouse_b5;
   int mouse_wu;
   int mouse_wd;
   int mouse_wl;
   int mouse_wr;
} sdl2_input_t;

#ifdef WEBOS
enum sdl2_webos_special_key
{
   sdl2_webos_spkey_back,
   sdl2_webos_spkey_return,
   sdl2_webos_spkey_up,
   sdl2_webos_spkey_down,
   sdl2_webos_spkey_left,
   sdl2_webos_spkey_right,
   sdl2_webos_spkey_size,
};

static uint8_t sdl2_webos_special_keymap[sdl2_webos_spkey_size] = {0};

/* Set after a real typing key while the OSK/line editor is open. Magic
 * Remote arrows/OK/digits must leave this false so the OSK grid stays
 * under remote control. */
static bool sdl2_webos_phys_kbd_typing = false;

/* One-shot sticky keys: webOS often delivers KEYDOWN+KEYUP in the same
 * poll, so SDL_GetKeyboardState is already clear when the menu reads input. */
static bool sdl2_webos_sticky_pressed(enum sdl2_webos_special_key slot)
{
   if (sdl2_webos_special_keymap[slot])
   {
      sdl2_webos_special_keymap[slot] = 0;
      return true;
   }
   return false;
}

static bool sdl2_webos_is_remote_nav_scancode(SDL_Scancode scancode)
{
   switch ((int)scancode)
   {
      case SDL_SCANCODE_UP:
      case SDL_SCANCODE_DOWN:
      case SDL_SCANCODE_LEFT:
      case SDL_SCANCODE_RIGHT:
      case SDL_SCANCODE_RETURN:
      case SDL_SCANCODE_ESCAPE:
      case SDL_SCANCODE_PAGEUP:
      case SDL_SCANCODE_PAGEDOWN:
      case SDL_WEBOS_SCANCODE_BACK:
      case SDL_WEBOS_SCANCODE_RED:
      case SDL_WEBOS_SCANCODE_GREEN:
      case SDL_WEBOS_SCANCODE_YELLOW:
      case SDL_WEBOS_SCANCODE_BLUE:
      case SDL_WEBOS_SCANCODE_EXIT:
         return true;
      default:
         return false;
   }
}

/* Keys that mean a physical BT keyboard is in use (not Magic Remote). */
static bool sdl2_webos_scancode_enables_phys_kbd(SDL_Scancode scancode)
{
   if (sdl2_webos_is_remote_nav_scancode(scancode))
      return false;

   /* Remote digit row inserts text but must not switch to caret mode. */
   if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_0)
      return false;

   return true;
}
#endif

static void *sdl2_input_init(const char *joypad_driver)
{
   sdl2_input_t    *sdl = (sdl2_input_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_sdl);

   return sdl;
}

static bool sdl2_key_pressed(int key)
{
   int num_keys;
   const uint8_t *keymap = SDL_GetKeyboardState(&num_keys);
   unsigned sym          = SDL_GetScancodeFromKey(rarch_keysym_lut[(enum retro_key)key]);

   if (!key)
      return false;

#ifdef WEBOS
   if (key == RETROK_BACKSPACE
         && sdl2_webos_sticky_pressed(sdl2_webos_spkey_back))
      return true;
   /* Sticky pulse (Magic Remote) → OSK grid / OK. Held BT keys must not
    * also report as menu joypad while the line editor owns them. */
   if (key == RETROK_RETURN
         || key == RETROK_UP
         || key == RETROK_DOWN
         || key == RETROK_LEFT
         || key == RETROK_RIGHT)
   {
      enum sdl2_webos_special_key slot = sdl2_webos_spkey_return;

      if (key == RETROK_UP)
         slot = sdl2_webos_spkey_up;
      else if (key == RETROK_DOWN)
         slot = sdl2_webos_spkey_down;
      else if (key == RETROK_LEFT)
         slot = sdl2_webos_spkey_left;
      else if (key == RETROK_RIGHT)
         slot = sdl2_webos_spkey_right;

      if (sdl2_webos_sticky_pressed(slot))
         return true;

      if (input_driver_keyboard_mapping_blocked())
         return false;
   }
   if (key == RETROK_F1 && keymap[SDL_WEBOS_SCANCODE_EXIT])
      return true;
   if (key == RETROK_x && keymap[SDL_WEBOS_SCANCODE_RED])
      return true;
   if (key == RETROK_z && keymap[SDL_WEBOS_SCANCODE_GREEN])
      return true;
   if (key == RETROK_s && keymap[SDL_WEBOS_SCANCODE_YELLOW])
      return true;
   if (key == RETROK_a && keymap[SDL_WEBOS_SCANCODE_BLUE])
      return true;
#endif

   if (sym >= (unsigned)num_keys)
      return false;

   return keymap[sym];
}

/* Which of @keys are down: bit n of @down for keys[n]. */
static void sdl2_keys_down(void *data, unsigned port,
      const uint16_t *keys, const uint8_t *bind, unsigned count,
      uint32_t *down)
{
   unsigned i;
   (void)data;
   (void)port;
   (void)bind;
   for (i = 0; i < count; i++)
      if (sdl2_key_pressed(keys[i]))
         down[i >> 5] |= (1u << (i & 31));
}

static int16_t sdl2_input_state(
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
   int16_t       ret = 0;
   sdl2_input_t *sdl = (sdl2_input_t*)data;

   switch (device)
   {
      /* The RetroPad's buttons and the hotkeys, where they are bound to
       * keys or mouse buttons, are the frontend's to answer: it asks
       * sdl2_keys_down() for the keys once a poll. */
      case RETRO_DEVICE_ANALOG:
         {
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid        = RETRO_KEYBIND_VALID(&binds[port][id_minus]);
            id_plus_valid         = RETRO_KEYBIND_VALID(&binds[port][id_plus]);
            id_minus_key          = RETRO_KEYBIND_KEY(&binds[port][id_minus]);
            id_plus_key           = RETRO_KEYBIND_KEY(&binds[port][id_plus]);

            if (id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
            {
               if (sdl2_key_pressed(id_plus_key))
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
            {
               if (sdl2_key_pressed(id_minus_key))
                  ret += -0x7fff;
            }
         }
         return ret;
      /* The mouse, the pointer and the lightgun's aim are the frontend's
       * to answer: the poll publishes the mouse. */
      case RETRO_DEVICE_KEYBOARD:
         return (id && id < RETROK_LAST) && sdl2_key_pressed(id);
      /* TODO: update button binds to match other input drivers */
      case RETRO_DEVICE_LIGHTGUN:
         /* its buttons are the mouse's */
         switch (id)
         {
            case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
               return sdl->mouse_l;
            case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
               return sdl->mouse_m;
            case RETRO_DEVICE_ID_LIGHTGUN_START:
               return sdl->mouse_r;
            case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
               return sdl->mouse_l && sdl->mouse_r;
            default:
               break;
         }
         break;
   }

   return 0;
}

static void sdl2_input_free(void *data)
{
   sdl2_input_t *sdl = (sdl2_input_t*)data;

   if (!sdl)
      return;

   /* Flush out all pending events. */
   SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

#ifdef __linux__
   linux_close_illuminance_sensor(sdl->illuminance_sensor); /* noop if NULL */
#endif

   free(data);
}

static bool sdl2_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned rate)
{
   sdl2_input_t *sdl = (sdl2_input_t*)data;

   if (!sdl)
      return false;

   switch (action)
   {
      case RETRO_SENSOR_ILLUMINANCE_DISABLE:
#ifdef __linux__
         /* If already disabled, then do nothing */
         linux_close_illuminance_sensor(sdl->illuminance_sensor); /* noop if NULL */
         sdl->illuminance_sensor = NULL;
#endif
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         /** Unimplemented sensor actions that probably shouldn't fail */
         return true;

      case RETRO_SENSOR_ILLUMINANCE_ENABLE:
#ifdef __linux__
         /* Unsupported on non-Linux platforms */
         if (sdl->illuminance_sensor)
            /* If we already have a sensor, just set the rate */
            linux_set_illuminance_sensor_rate(sdl->illuminance_sensor, rate);
         else
            sdl->illuminance_sensor = linux_open_illuminance_sensor(rate);

         return sdl->illuminance_sensor != NULL;
#endif
      default:
         break;
   }

   return false;
}

static float sdl2_get_sensor_input(void *data, unsigned port, unsigned id)
{
   sdl2_input_t *sdl = (sdl2_input_t*)data;

   if (!sdl)
      return 0.0f;

   switch (id)
   {
      case RETRO_SENSOR_ILLUMINANCE:
#ifdef __linux__
         if (sdl->illuminance_sensor)
            return linux_get_illuminance_reading(sdl->illuminance_sensor);
#endif
      /* Unsupported on non-Linux platforms */
      default:
         break;
   }

   return 0.0f;
}

static void sdl2_input_grab_mouse(void *data, bool state)
{
   /* the window is the video driver's */
   sdl2_video_grab_window(state);
}

static void sdl2_poll_mouse(sdl2_input_t *sdl)
{
   int rx = 0, ry = 0, ax = 0, ay = 0;
   Uint8 btn     = SDL_GetRelativeMouseState(&rx, &ry);

   SDL_GetMouseState(&ax, &ay);
   sdl->mouse_rel = VIDEO_POS_PACK(rx, ry);
   sdl->mouse_abs = VIDEO_POS_PACK(ax, ay);

   sdl->mouse_l  = (SDL_BUTTON(SDL_BUTTON_LEFT)      & btn) ? 1 : 0;
   sdl->mouse_r  = (SDL_BUTTON(SDL_BUTTON_RIGHT)     & btn) ? 1 : 0;
   sdl->mouse_m  = (SDL_BUTTON(SDL_BUTTON_MIDDLE)    & btn) ? 1 : 0;
   sdl->mouse_b4 = (SDL_BUTTON(SDL_BUTTON_X1)        & btn) ? 1 : 0;
   sdl->mouse_b5 = (SDL_BUTTON(SDL_BUTTON_X2)        & btn) ? 1 : 0;
}

/* The mouse's frame, handed to the frontend, which answers for the
 * mouse, the pointer and the lightgun's aim. The mouse is the port's
 * whose Mouse Index is 0; the pointer and the lightgun are every
 * port's, and the mouse stands for one touch, its left button. */
static void sdl2_publish_pointers(sdl2_input_t *sdl)
{
   input_pointer_frame_t frame;
   unsigned buttons = 0;

   if (sdl->mouse_l)
      buttons |= INPUT_POINTER_LEFT;
   if (sdl->mouse_r)
      buttons |= INPUT_POINTER_RIGHT;
   if (sdl->mouse_m)
      buttons |= INPUT_POINTER_MIDDLE;
   if (sdl->mouse_b4)
      buttons |= INPUT_POINTER_BUTTON_4;
   if (sdl->mouse_b5)
      buttons |= INPUT_POINTER_BUTTON_5;
   if (sdl->mouse_wu)
      buttons |= INPUT_POINTER_WHEEL_UP;
   if (sdl->mouse_wd)
      buttons |= INPUT_POINTER_WHEEL_DOWN;
   if (sdl->mouse_wr)
      buttons |= INPUT_POINTER_HWHEEL_UP;
   if (sdl->mouse_wl)
      buttons |= INPUT_POINTER_HWHEEL_DOWN;
   frame.pos     = sdl->mouse_abs;
   frame.rel     = sdl->mouse_rel;
   frame.buttons = (uint16_t)buttons;
   /* a notch is this frame's: it was kept until the next turn of the
    * wheel, and read as held for as long */
   sdl->mouse_wu = 0;
   sdl->mouse_wd = 0;
   sdl->mouse_wl = 0;
   sdl->mouse_wr = 0;

   input_driver_publish_pointers(&frame, 1,
         INPUT_POINTERS_BY_MOUSE_INDEX | INPUT_POINTERS_AIM_EVERY_PORT);
}

static void sdl2_input_poll(void *data)
{
   SDL_Event event;
   sdl2_input_t *sdl = (sdl2_input_t*)data;

   SDL_PumpEvents();

   sdl2_poll_mouse(sdl);

   while (SDL_PeepEvents(&event, 1,
            SDL_GETEVENT, SDL_KEYDOWN, SDL_MOUSEWHEEL) > 0)
   {
      if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
      {
         uint16_t mod  = 0;
         unsigned code = input_keymaps_translate_keysym_to_rk(
               event.key.keysym.sym);
#ifdef WEBOS
         bool osk_active = input_driver_keyboard_mapping_blocked();

         if (!osk_active)
            sdl2_webos_phys_kbd_typing = false;

         switch ((int) event.key.keysym.scancode)
         {
            case SDL_WEBOS_SCANCODE_BACK:
               /* Because webOS is sending DOWN/UP at the same time,
                  we save this flag for later */
               sdl2_webos_special_keymap[sdl2_webos_spkey_back] |= event.type == SDL_KEYDOWN;
               code = RETROK_BACKSPACE;
               break;
            case SDL_WEBOS_SCANCODE_RED:
               code = RETROK_x;
               break;
            case SDL_WEBOS_SCANCODE_GREEN:
               code = RETROK_z;
               break;
            case SDL_WEBOS_SCANCODE_YELLOW:
               code = RETROK_s;
               break;
            case SDL_WEBOS_SCANCODE_BLUE:
               code = RETROK_a;
               break;
            case SDL_WEBOS_SCANCODE_EXIT:
               code = RETROK_F1;
               break;
            case SDL_SCANCODE_UP:
            case SDL_SCANCODE_DOWN:
            case SDL_SCANCODE_LEFT:
            case SDL_SCANCODE_RIGHT:
               /* Default: Magic Remote → OSK grid. After BT typing keys,
                * ←/→ move the caret and ↑/↓ act as home/end. */
               if (osk_active && !sdl2_webos_phys_kbd_typing)
               {
                  if (event.type == SDL_KEYDOWN)
                  {
                     if (event.key.keysym.scancode == SDL_SCANCODE_UP)
                        sdl2_webos_special_keymap[sdl2_webos_spkey_up] = 1;
                     else if (event.key.keysym.scancode == SDL_SCANCODE_DOWN)
                        sdl2_webos_special_keymap[sdl2_webos_spkey_down] = 1;
                     else if (event.key.keysym.scancode == SDL_SCANCODE_LEFT)
                        sdl2_webos_special_keymap[sdl2_webos_spkey_left] = 1;
                     else
                        sdl2_webos_special_keymap[sdl2_webos_spkey_right] = 1;
                  }
                  continue;
               }
               break;
            case SDL_SCANCODE_RETURN:
               /* Default: remote OK → OSK select. After BT typing → save. */
               if (osk_active && !sdl2_webos_phys_kbd_typing)
               {
                  if (event.type == SDL_KEYDOWN)
                     sdl2_webos_special_keymap[sdl2_webos_spkey_return] = 1;
                  continue;
               }
               break;
            default:
               break;
         }

         /* Letters / numpad / backspace / punctuation ⇒ BT keyboard session.
          * Remote digit row is excluded (see sdl2_webos_scancode_enables_phys_kbd). */
         if (osk_active
               && event.type == SDL_KEYDOWN
               && sdl2_webos_scancode_enables_phys_kbd(event.key.keysym.scancode))
            sdl2_webos_phys_kbd_typing = true;

         /* Disable cursor when using the buttons */
         if (code && code != RETROK_RETURN)
            SDL_webOSCursorVisibility(0);
#endif

         if (event.key.keysym.mod & KMOD_SHIFT)
            mod |= RETROKMOD_SHIFT;

         if (event.key.keysym.mod & KMOD_CTRL)
            mod |= RETROKMOD_CTRL;

         if (event.key.keysym.mod & KMOD_ALT)
            mod |= RETROKMOD_ALT;

         if (event.key.keysym.mod & KMOD_NUM)
            mod |= RETROKMOD_NUMLOCK;

         if (event.key.keysym.mod & KMOD_CAPS)
            mod |= RETROKMOD_CAPSLOCK;

         /* KMOD_SCROLL was added in SDL 2.0.18, use the raw number
            to stay backwards compatible with older versions */
         if (event.key.keysym.mod & 0x8000 /*KMOD_SCROLL*/)
            mod |= RETROKMOD_SCROLLOCK;

         {
            /* Use key+mod ASCII so Shift does not leak as '?' and capitals work. */
            uint32_t character = input_keymaps_translate_rk_to_ascii(
                  (enum retro_key)code, (enum retro_mod)mod);

#ifdef WEBOS
            /* Force Enter / numpad Enter to '\r' so they save the line
             * (see #19275). This must stay webOS-only: with no modifiers
             * held the translation already yields '\r' / '\n', both of
             * which save the line, and with Alt/Ctrl/Meta held it
             * deliberately yields no character so that hotkey chords such
             * as Alt+Enter cannot submit an open line editor. */
            if (code == RETROK_RETURN || code == RETROK_KP_ENTER)
               character = '\r';

            /* Numpad Enter is never sent by the Magic Remote; always save. */
            if (code == RETROK_KP_ENTER)
               sdl2_webos_phys_kbd_typing = true;
#endif

            input_keyboard_event(event.type == SDL_KEYDOWN, code,
                  character, mod, RETRO_DEVICE_KEYBOARD);
         }
      }
      else if (event.type == SDL_MOUSEWHEEL)
      {
         /* SDL's y is positive for a turn away from the user: up */
         sdl->mouse_wu = event.wheel.y > 0;
         sdl->mouse_wd = event.wheel.y < 0;
         sdl->mouse_wl = event.wheel.x < 0;
         sdl->mouse_wr = event.wheel.x > 0;
         break;
      }
   }

   sdl2_publish_pointers(sdl);
}

static uint64_t sdl2_get_capabilities(void *data)
{
   return
           (1 << RETRO_DEVICE_JOYPAD)
         | (1 << RETRO_DEVICE_MOUSE)
         | (1 << RETRO_DEVICE_KEYBOARD)
         | (1 << RETRO_DEVICE_LIGHTGUN)
         | (1 << RETRO_DEVICE_POINTER)
         | (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_sdl2 = {
   sdl2_input_init,
   sdl2_input_poll,
   sdl2_input_state,
   sdl2_input_free,
   sdl2_set_sensor_state,
   sdl2_get_sensor_input,
   sdl2_get_capabilities,
   "sdl2",
   sdl2_input_grab_mouse,
   NULL,
   NULL,
   NULL,
   sdl2_keys_down
};

#ifdef WEBOS
SDL_bool SDL_webOSCursorVisibility(SDL_bool visible)
{
   static SDL_bool (*fn)(SDL_bool visible) = NULL;
   static bool dlsym_called                = false;
   if (!dlsym_called)
   {
      fn                                   = dlsym(RTLD_NEXT, "SDL_webOSCursorVisibility");
      dlsym_called                         = true;
   }
   if (!fn)
   {
      SDL_ShowCursor(SDL_DISABLE);
      SDL_ShowCursor(SDL_ENABLE);
      return SDL_TRUE;
   }
   return fn(visible);
}
#endif
