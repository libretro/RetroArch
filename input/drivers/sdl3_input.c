/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Higor Euripedes
 *  Copyright (C)      2026 - Rob Loach
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
#include <encodings/utf.h>
#include <libretro.h>

#include <SDL3/SDL.h>

#include "../input_keymaps.h"

#include "../../configuration.h"
#include "../../retroarch.h"

#ifdef HAVE_MENU
#include "../../menu/menu_input.h"
#endif

#include "../../gfx/common/sdl3_common.h"

#ifdef WEBOS
#include <dlfcn.h>
#include "../../gfx/common/sdl3_common_webos.h"
#endif

/* OVERLAY_MAX_TOUCH */
#define SDL3_MAX_TOUCH 16

typedef struct sdl3_input
{
   /* The keyboard state, provided by SDL_GetKeyboardState(). */
   const bool *kb_state;
   int kb_num_keys;
   SDL_Scancode key_scancode_lut[RETROK_LAST];
   /* Whole-pixel relative motion. */
   int16_t mouse_x;
   int16_t mouse_y;
   /* Sub-pixel remainder carried into the next frame. */
   float mouse_rel_x;
   float mouse_rel_y;
   /* Absolute position stays fractional; it's truncated at the API
    * boundary, where nothing accumulates. */
   float mouse_abs_x;
   float mouse_abs_y;
   /* Button states. */
   bool mouse_l;
   bool mouse_r;
   bool mouse_m;
   bool mouse_b4;
   bool mouse_b5;
   bool mouse_wu;
   bool mouse_wd;
   bool mouse_wl;
   bool mouse_wr;

   /* Number of connected touch devices. Saves having to query them
    * every frame. */
   int num_touch_devices;
   unsigned touch_recheck;

   /* Number of active fingers across all touch devices. */
   int num_touches;
   struct
   {
      float x;
      float y;
   } touches[SDL3_MAX_TOUCH];
} sdl3_input_t;

#ifdef WEBOS
enum sdl_webos_special_key
{
   sdl_webos_spkey_back,
   sdl_webos_spkey_return,
   sdl_webos_spkey_up,
   sdl_webos_spkey_down,
   sdl_webos_spkey_left,
   sdl_webos_spkey_right,
   sdl_webos_spkey_size,
};

static uint8_t sdl_webos_special_keymap[sdl_webos_spkey_size] = {0};

/* Set after a real typing key while the OSK/line editor is open. Magic
 * Remote arrows/OK/digits must leave this false so the OSK grid stays
 * under remote control. */
static bool sdl_webos_phys_kbd_typing = false;

/* One-shot sticky keys: webOS often delivers KEYDOWN+KEYUP in the same
 * poll, so SDL_GetKeyboardState is already clear when the menu reads input. */
static bool sdl_webos_sticky_pressed(enum sdl_webos_special_key slot)
{
   if (sdl_webos_special_keymap[slot])
   {
      sdl_webos_special_keymap[slot] = 0;
      return true;
   }
   return false;
}

static bool sdl_webos_is_remote_nav_scancode(SDL_Scancode scancode)
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
      case SDL_SCANCODE_WEBOS_BACK:
      case SDL_SCANCODE_WEBOS_RED:
      case SDL_SCANCODE_WEBOS_GREEN:
      case SDL_SCANCODE_WEBOS_YELLOW:
      case SDL_SCANCODE_WEBOS_BLUE:
      case SDL_SCANCODE_WEBOS_EXIT:
         return true;
      default:
         return false;
   }
}

/* Keys that mean a physical BT keyboard is in use (not Magic Remote). */
static bool sdl_webos_scancode_enables_phys_kbd(SDL_Scancode scancode)
{
   if (sdl_webos_is_remote_nav_scancode(scancode))
      return false;

   /* Remote digit row inserts text but must not switch to caret mode. */
   if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_0)
      return false;

   return true;
}
#endif

/* Rebuilt on SDL_EVENT_KEYMAP_CHANGED (e.g. system layout switch). */
static void sdl3_build_scancode_lut(sdl3_input_t *sdl)
{
   int i;
   for (i = 0; i < RETROK_LAST; i++)
      sdl->key_scancode_lut[i] = rarch_keysym_lut[i]
            ? SDL_GetScancodeFromKey((SDL_Keycode)rarch_keysym_lut[i], NULL)
            : SDL_SCANCODE_UNKNOWN;
}

static void *sdl3_input_init(const char *joypad_driver)
{
   sdl3_input_t *sdl = (sdl3_input_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   /* Set up the SDL event queue. */
   if (!SDL_InitSubSystem(SDL_INIT_EVENTS))
   {
      free(sdl);
      return NULL;
   }

   input_keymaps_init_keyboard_lut(rarch_key_map_sdl3);

   sdl->kb_state = SDL_GetKeyboardState(&sdl->kb_num_keys);
   sdl3_build_scancode_lut(sdl);

   /* Prime the touch probe so a present touchscreen works from the
    * first frame (see sdl3_poll_touch). */
   {
      SDL_TouchID *devices = SDL_GetTouchDevices(&sdl->num_touch_devices);
      SDL_free(devices);
   }

   return sdl;
}

static bool sdl3_key_pressed(sdl3_input_t *sdl, int key)
{
   SDL_Scancode sym = 0;

   if (!key)
      return false;

#ifdef WEBOS
   if (key == RETROK_BACKSPACE
         && sdl_webos_sticky_pressed(sdl_webos_spkey_back))
      return true;
   /* Sticky pulse (Magic Remote) → OSK grid / OK. Held BT keys must not
    * also report as menu joypad while the line editor owns them. */
   if (key == RETROK_RETURN
         || key == RETROK_UP
         || key == RETROK_DOWN
         || key == RETROK_LEFT
         || key == RETROK_RIGHT)
   {
      enum sdl_webos_special_key slot = sdl_webos_spkey_return;

      if (key == RETROK_UP)
         slot = sdl_webos_spkey_up;
      else if (key == RETROK_DOWN)
         slot = sdl_webos_spkey_down;
      else if (key == RETROK_LEFT)
         slot = sdl_webos_spkey_left;
      else if (key == RETROK_RIGHT)
         slot = sdl_webos_spkey_right;

      if (sdl_webos_sticky_pressed(slot))
         return true;

      if (input_state_get_ptr()
            && (input_state_get_ptr()->flags & INP_FLAG_KB_MAPPING_BLOCKED))
         return false;
   }
   if (key == RETROK_F1 && sdl->kb_state[SDL_SCANCODE_WEBOS_EXIT])
      return true;
   if (key == RETROK_x && sdl->kb_state[SDL_SCANCODE_WEBOS_RED])
      return true;
   if (key == RETROK_z && sdl->kb_state[SDL_SCANCODE_WEBOS_GREEN])
      return true;
   if (key == RETROK_s && sdl->kb_state[SDL_SCANCODE_WEBOS_YELLOW])
      return true;
   if (key == RETROK_a && sdl->kb_state[SDL_SCANCODE_WEBOS_BLUE])
      return true;
#endif

   /* The keyboard state array is refreshed by SDL while pumping
    * window events - it stays empty until a focused SDL3 window
    * exists (i.e. the SDL3 video driver is running). */
   sym = sdl->key_scancode_lut[key];

   if ((int)sym >= sdl->kb_num_keys)
      return false;

   return sdl->kb_state[sym];
}

/* Resolves a retro_keybind mouse-button bind (bind->mbutton) against
 * the polled mouse state; used by the lightgun bind checks below. */
static bool sdl3_mouse_button_pressed(sdl3_input_t *sdl, unsigned key)
{
   switch (key)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return sdl->mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return sdl->mouse_r;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return sdl->mouse_m;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         return sdl->mouse_b4;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return sdl->mouse_b5;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return sdl->mouse_wu;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return sdl->mouse_wd;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         return sdl->mouse_wr;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         return sdl->mouse_wl;
   }

   return false;
}

static int16_t sdl3_input_state(
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
   int16_t ret = 0;
   sdl3_input_t *sdl = (sdl3_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if ((binds[port][i].key && binds[port][i].key < RETROK_LAST)
                           && sdl3_key_pressed(sdl, binds[port][i].key))
                        ret |= (1 << i);
                  }
               }
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid)
            {
               if ((binds[port][id].key && binds[port][id].key < RETROK_LAST)
                     && sdl3_key_pressed(sdl, binds[port][id].key)
                     && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                  )
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         {
            int id_minus_key = 0;
            int id_plus_key = 0;
            unsigned id_minus = 0;
            unsigned id_plus = 0;
            bool id_plus_valid = false;
            bool id_minus_valid = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid = binds[port][id_minus].valid;
            id_plus_valid = binds[port][id_plus].valid;
            id_minus_key = binds[port][id_minus].key;
            id_plus_key = binds[port][id_plus].key;

            if (id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
            {
               if (sdl3_key_pressed(sdl, id_plus_key))
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
            {
               if (sdl3_key_pressed(sdl, id_minus_key))
                  ret += -0x7fff;
            }
         }
         return ret;
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         if (config_get_ptr()->uints.input_mouse_index[ port ] == 0)
         {
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return sdl->mouse_l;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return sdl->mouse_r;
#ifdef WEBOS
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  /* Note: webOS wheel is reversed */
                  if (sdl->mouse_wd != 0)
                  {
                      sdl->mouse_wd = 0;
                      return 1;
                  }
                  break;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  if (sdl->mouse_wu != 0)
                  {
                      sdl->mouse_wu = 0;
                      return 1;
                  }
                  break;
               case RETRO_DEVICE_ID_MOUSE_X:
                  /* MOUSE_SCREEN must be absolute (menu/OSK hit-test);
                   * RETRO_DEVICE_MOUSE stays relative for cores. */
                  return (device == RARCH_DEVICE_MOUSE_SCREEN)
                        ? sdl->mouse_abs_x : sdl->mouse_x;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  return (device == RARCH_DEVICE_MOUSE_SCREEN)
                        ? sdl->mouse_abs_y : sdl->mouse_y;
#else
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  return sdl->mouse_wu;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  return sdl->mouse_wd;
               case RETRO_DEVICE_ID_MOUSE_X:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return (int16_t)sdl->mouse_abs_x;
                  return sdl->mouse_x;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return (int16_t)sdl->mouse_abs_y;
                  return sdl->mouse_y;
#endif
               case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
                  return sdl->mouse_wr;
               case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
                  return sdl->mouse_wl;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return sdl->mouse_m;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
                  return sdl->mouse_b4;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
                  return sdl->mouse_b5;
            }
         }
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         {
            video_viewport_t vp = {0};
            bool screen = device == RARCH_DEVICE_POINTER_SCREEN;
            int16_t res_x = 0;
            int16_t res_y = 0;
            int16_t res_screen_x = 0;
            int16_t res_screen_y = 0;
            int abs_x = 0;
            int abs_y = 0;
            int16_t pressed = 0;

            if (id == RETRO_DEVICE_ID_POINTER_COUNT)
               return sdl->num_touches ? sdl->num_touches : (sdl->mouse_l ? 1 : 0);

            if (!video_driver_get_viewport_info(&vp))
               break;

            /* Touch contacts take precedence; the mouse doubles as
             * pointer 0 when no fingers are down (touch/pointer
             * overlay support - input_poll_overlay walks pointer
             * indices until PRESSED reads 0). */
            if (sdl->num_touches > 0)
            {
               if ((int)idx >= sdl->num_touches)
                  return 0;
               abs_x = (int)(sdl->touches[idx].x * (float)vp.full_width);
               abs_y = (int)(sdl->touches[idx].y * (float)vp.full_height);
               pressed = 1;
            }
            else
            {
               if (idx != 0)
                  return 0;
               abs_x = (int)sdl->mouse_abs_x;
               abs_y = (int)sdl->mouse_abs_y;
               pressed = sdl->mouse_l;
            }

            if (video_driver_translate_coord_viewport(
                        &vp, abs_x, abs_y,
                        &res_x, &res_y, &res_screen_x, &res_screen_y,
                        true))
            {
               if (screen)
               {
                  res_x = res_screen_x;
                  res_y = res_screen_y;
               }

               switch (id)
               {
                  case RETRO_DEVICE_ID_POINTER_X:
                     return res_x;
                  case RETRO_DEVICE_ID_POINTER_Y:
                     return res_y;
                  case RETRO_DEVICE_ID_POINTER_PRESSED:
                     return pressed;
                  case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
                     return input_driver_pointer_is_offscreen(res_x, res_y);
               }
            }
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         /* While a text box is open, Ctrl is the clipboard-paste
          * modifier (see sdl3_paste_clipboard), so ignore acting
          * on it here. */
         if ((id == RETROK_LCTRL || id == RETROK_RCTRL) && input_state_get_ptr()->keyboard_line.enabled)
            return 0;
         return (id && id < RETROK_LAST) && sdl3_key_pressed(sdl, id);
      case RETRO_DEVICE_LIGHTGUN:
         switch (id)
         {
            /* Aiming */
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
               {
                  video_viewport_t vp  = {0};
                  int16_t res_x        = 0;
                  int16_t res_y        = 0;
                  int16_t res_screen_x = 0;
                  int16_t res_screen_y = 0;

                  if (video_driver_translate_coord_viewport_wrap(
                              &vp, (int)sdl->mouse_abs_x, (int)sdl->mouse_abs_y,
                              &res_x, &res_y, &res_screen_x, &res_screen_y))
                  {
                     switch (id)
                     {
                        case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
                           return res_x;
                        case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                           return res_y;
                        case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                           return input_driver_pointer_is_offscreen(res_x, res_y);
                        default:
                           break;
                     }
                  }
               }
               break;
            /* Buttons */
            case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
            case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
            case RETRO_DEVICE_ID_LIGHTGUN_START:
            case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
            case RETRO_DEVICE_ID_LIGHTGUN_PAUSE: /* deprecated */
               {
                  unsigned new_id                 = input_driver_lightgun_id_convert(id);
                  const uint64_t bind_joykey      = input_config_binds[port][new_id].joykey;
                  const uint64_t bind_joyaxis     = input_config_binds[port][new_id].joyaxis;
                  const uint64_t autobind_joykey  = input_autoconf_binds[port][new_id].joykey;
                  const uint64_t autobind_joyaxis = input_autoconf_binds[port][new_id].joyaxis;
                  uint16_t joyport                = joypad_info->joy_idx;
                  float axis_threshold            = joypad_info->axis_threshold;
                  const uint64_t joykey           = (bind_joykey != NO_BTN)
                        ? bind_joykey  : autobind_joykey;
                  const uint32_t joyaxis          = (bind_joyaxis != AXIS_NONE)
                        ? bind_joyaxis : autobind_joyaxis;

                  if (binds[port][new_id].valid)
                  {
                     if ((uint16_t)joykey != NO_BTN && joypad->button(
                              joyport, (uint16_t)joykey))
                        return 1;
                     if (joyaxis != AXIS_NONE &&
                           ((float)abs(joypad->axis(joyport, joyaxis))
                            / 0x8000) > axis_threshold)
                        return 1;
                     else if ((binds[port][new_id].key && binds[port][new_id].key < RETROK_LAST)
                           && !keyboard_mapping_blocked
                           && sdl3_key_pressed(sdl, binds[port][new_id].key)
                        )
                        return 1;
                     else if (sdl3_mouse_button_pressed(sdl,
                           binds[port][new_id].mbutton))
                        return 1;
                  }
               }
               break;
            /* Deprecated relative aiming */
            case RETRO_DEVICE_ID_LIGHTGUN_X:
               return sdl->mouse_x;
            case RETRO_DEVICE_ID_LIGHTGUN_Y:
               return sdl->mouse_y;
         }
         break;
   }

   return 0;
}

static void sdl3_input_free(void *data)
{
   sdl3_input_t *sdl = (sdl3_input_t*)data;

   if (!sdl)
      return;

   /* Drop only the events this driver owns, matching what
    * sdl3_input_poll consumes. Flushing SDL_EVENT_FIRST..LAST would
    * also take the video driver's pending SDL_EVENT_QUIT and the
    * joypad driver's hotplug events with it - harmless at shutdown,
    * but input drivers are also torn down and recreated on a runtime
    * driver switch, where that can swallow a window close the user
    * already clicked. */
   SDL_FlushEvents(SDL_EVENT_KEY_DOWN,         SDL_EVENT_MOUSE_REMOVED);
   SDL_FlushEvents(SDL_EVENT_FINGER_DOWN,      SDL_EVENT_FINGER_CANCELED);
   SDL_FlushEvents(SDL_EVENT_PEN_PROXIMITY_IN, SDL_EVENT_PEN_AXIS);

   /* Nothing polls after this point, so the flags would stay raised
    * across a runtime driver switch. */
   input_state_get_ptr()->flags &=
      ~(INP_FLAG_NATIVE_KB_SHOWN | INP_FLAG_NATIVE_KB_AVAIL);

   SDL_QuitSubSystem(SDL_INIT_EVENTS);
   free(sdl);
}

static bool sdl3_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   /* Sensors are not exposed through the SDL3 keyboard/mouse driver.
    * Gamepad gyro/accel are handled by the SDL3 joypad driver. */
   switch (action)
   {
      case RETRO_SENSOR_ILLUMINANCE_DISABLE:
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         /* Disabling an unsupported sensor shouldn't fail. */
         return true;
      default:
         break;
   }

   return false;
}

static void sdl3_poll_mouse(sdl3_input_t *sdl)
{
   SDL_Window *win;
   float dx = 0.0f;
   float dy = 0.0f;
   SDL_MouseButtonFlags btn = SDL_GetMouseState(&sdl->mouse_abs_x, &sdl->mouse_abs_y);
   SDL_GetRelativeMouseState(&dx, &dy);

   sdl->mouse_rel_x += dx;
   sdl->mouse_rel_y += dy;

   /* Converting a float outside int16_t's range is undefined behaviour
    * rather than a wrap, so clamp before the cast. Only reachable if
    * the frontend stops polling for a long stretch while the mouse
    * keeps moving. */
   sdl->mouse_rel_x = MIN(MAX(sdl->mouse_rel_x, -32767.0f), 32767.0f);
   sdl->mouse_rel_y = MIN(MAX(sdl->mouse_rel_y, -32767.0f), 32767.0f);

   sdl->mouse_x = (int16_t)sdl->mouse_rel_x;
   sdl->mouse_y = (int16_t)sdl->mouse_rel_y;

   sdl->mouse_rel_x -= (float)sdl->mouse_x;
   sdl->mouse_rel_y -= (float)sdl->mouse_y;

   /* SDL reports mouse coordinates in window coordinates (points),
    * while the video driver's viewport metrics are in output pixels. */
   if (!(win = sdl3_get_window()))
      win = SDL_GetMouseFocus();

   if (win)
   {
      float density = SDL_GetWindowPixelDensity(win);
      if (density > 0.0f && density != 1.0f)
      {
         sdl->mouse_abs_x *= density;
         sdl->mouse_abs_y *= density;
      }
   }

   sdl->mouse_l = (SDL_BUTTON_MASK(SDL_BUTTON_LEFT) & btn) != 0;
   sdl->mouse_r = (SDL_BUTTON_MASK(SDL_BUTTON_RIGHT) & btn) != 0;
   sdl->mouse_m = (SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE) & btn) != 0;
   sdl->mouse_b4 = (SDL_BUTTON_MASK(SDL_BUTTON_X1) & btn) != 0;
   sdl->mouse_b5 = (SDL_BUTTON_MASK(SDL_BUTTON_X2) & btn) != 0;
}

/* Snapshot the active fingers across all touchscreens. Polling
 * SDL_GetTouchFingers avoids tracking finger-id lifetimes through
 * SDL_EVENT_FINGER_* events by hand. */
static void sdl3_poll_touch(sdl3_input_t *sdl)
{
   int i;
   int num_devices = 0;
   int num_direct = 0;
   SDL_TouchID *devices     = NULL;

   sdl->num_touches = 0;

   /* Skip the per-frame device query on touch-less setups; re-probe
    * every ~10s since SDL3 has no touch hotplug event. */
   if (sdl->num_touch_devices == 0)
   {
      if (++sdl->touch_recheck < 600)
         return;
      sdl->touch_recheck = 0;
   }

   devices = SDL_GetTouchDevices(&num_devices);
   if (!devices)
   {
      sdl->num_touch_devices = 0;
      return;
   }

   for (i = 0; i < num_devices; i++)
   {
      int j, num_fingers = 0;
      SDL_Finger **fingers;

      /* Only SDL_TOUCH_DEVICE_DIRECT is a touchscreen. The two indirect
       * types are trackpads, whose fingers are device or cursor-relative. */
      if (SDL_GetTouchDeviceType(devices[i]) != SDL_TOUCH_DEVICE_DIRECT)
         continue;

      num_direct++;

      if (sdl->num_touches >= SDL3_MAX_TOUCH)
         continue;

      if (!(fingers = SDL_GetTouchFingers(devices[i], &num_fingers)))
         continue;

      for (j = 0; j < num_fingers && sdl->num_touches < SDL3_MAX_TOUCH; j++)
      {
         sdl->touches[sdl->num_touches].x = fingers[j]->x;
         sdl->touches[sdl->num_touches].y = fingers[j]->y;
         sdl->num_touches++;
      }

      SDL_free(fingers);
   }

   SDL_free(devices);

   /* Count touchscreens only, so a machine whose sole touch device is
    * a trackpad still takes the cheap early-out above. */
   sdl->num_touch_devices = num_direct;
}

/* Translates an SDL_Keymod to a RETROKMOD. */
static uint16_t sdl3_translate_mod(SDL_Keymod smod)
{
   uint16_t mod = 0;

   if (smod & SDL_KMOD_SHIFT)
      mod |= RETROKMOD_SHIFT;
   if (smod & SDL_KMOD_CTRL)
      mod |= RETROKMOD_CTRL;
   if (smod & SDL_KMOD_ALT)
      mod |= RETROKMOD_ALT;
   if (smod & SDL_KMOD_GUI)
      mod |= RETROKMOD_META;
   if (smod & SDL_KMOD_NUM)
      mod |= RETROKMOD_NUMLOCK;
   if (smod & SDL_KMOD_CAPS)
      mod |= RETROKMOD_CAPSLOCK;
   if (smod & SDL_KMOD_SCROLL)
      mod |= RETROKMOD_SCROLLOCK;

   return mod;
}

/* On devices where SDL_StartTextInput() brings up the system
 * keyboard, hold it back until a menu dialog actually wants text and
 * the user has opted in. On desktop, text input stays quietly enabled
 * in the background, ensuring normal keyboard controls work.
 *
 * Also publishes INP_FLAG_NATIVE_KB_SHOWN for the frontend: this runs
 * once per poll on the main thread, so consumers on the video thread
 * (gfx_display_draw_keyboard) read a plain flag instead of calling
 * into SDL from a thread SDL does not expect. */
static void sdl3_manage_text_input(void)
{
   bool want                    = false;
   bool shown                   = false;
   input_driver_state_t *input_st = input_state_get_ptr();
   SDL_Window *win;

   if (!sdl3_uses_screen_keyboard() || !(win = sdl3_get_window()))
   {
      input_st->flags &= ~(INP_FLAG_NATIVE_KB_SHOWN | INP_FLAG_NATIVE_KB_AVAIL);
      return;
   }

   input_st->flags |= INP_FLAG_NATIVE_KB_AVAIL;

#ifdef HAVE_MENU
   want = menu_input_dialog_get_display_kb()
       && config_get_ptr()->bools.input_sdl3_system_keyboard;
#endif

   if (want == SDL_TextInputActive(win))
      goto publish;

   if (want)
   {
      int w, h;
      SDL_Rect area;
      SDL_TextInputType type = SDL_TEXTINPUT_TYPE_TEXT;
      SDL_PropertiesID props = SDL_CreateProperties();

#ifdef HAVE_MENU
      switch (menu_input_dialog_get_kb_text_type())
      {
         case MENU_INPUT_DIALOG_KB_TYPE_PASSWORD:
            type = SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN;
            break;
         case MENU_INPUT_DIALOG_KB_TYPE_NUMBER:
            type = SDL_TEXTINPUT_TYPE_NUMBER;
            break;
         default:
            break;
      }
#endif

      /* Menu drivers draw the dialog's entry field in the top half of
       * the screen, so keep the system keyboard/IME from covering it.
       * Uses window coordinates, not pixels. */
      SDL_GetWindowSize(win, &w, &h);
      area.x = 0;
      area.y = 0;
      area.w = w;
      area.h = h / 2;
      SDL_SetTextInputArea(win, &area, 0);

      SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, type);
      SDL_StartTextInputWithProperties(win, props);
      SDL_DestroyProperties(props);
   }
   else
      SDL_StopTextInput(win);

publish:
   /* SDL_StartTextInput() only asks; the panel can take a frame to
    * appear and the user can dismiss it behind our back. Report what
    * is actually on screen. */
   shown = SDL_ScreenKeyboardShown(win);
   if (shown)
      input_st->flags |=  INP_FLAG_NATIVE_KB_SHOWN;
   else
      input_st->flags &= ~INP_FLAG_NATIVE_KB_SHOWN;
}

/* Translates control/modifier keys into their ASCII character counterpart. */
static uint32_t sdl3_translate_control_key(unsigned code, uint16_t mod)
{
   switch (code)
   {
      case RETROK_BACKSPACE:
      case RETROK_TAB:
      case RETROK_RETURN:
      case RETROK_ESCAPE:
      case RETROK_DELETE:
      case RETROK_KP_ENTER:
         return input_keymaps_translate_rk_to_ascii((enum retro_key)code, (enum retro_mod)mod);
      default:
         break;
   }

   return 0;
}

/* Grabs text from the clipboard, and passes it as keyboard input. */
static void sdl3_paste_clipboard(void)
{
   char *text = SDL_GetClipboardText();
   const char *ptr = text;

   if (!text)
      return;

   while (*ptr)
   {
      uint32_t c = utf8_walk(&ptr);

      /* Skip newline and backspace characters, since those would
       * negatively affect the input. */
      if (c >= 0x20 && c != 0x7f)
         input_keyboard_event(true, RETROK_UNKNOWN, c, 0, RETRO_DEVICE_KEYBOARD);
   }

   SDL_free(text);
}

#ifdef WEBOS
bool SDL_webOSCursorVisibility(bool visible)
{
   static bool (*fn)(bool visible) = NULL;
   static bool dlsym_called                = false;
   if (!dlsym_called)
   {
      fn                                   = dlsym(RTLD_NEXT, "SDL_webOSCursorVisibility");
      dlsym_called                         = true;
   }
   if (!fn)
   {
      SDL_ShowCursor();
      return true;
   }
   return fn(visible);
}
#endif

static void sdl3_input_poll(void *data)
{
   SDL_Event event;
   sdl3_input_t *sdl = (sdl3_input_t*)data;

   /* SDL only emits keyboard/mouse-wheel events for a window that owns
    * the input focus. Without an SDL3 video driver to create and pump
    * that window, this queue drains nothing and key/wheel state below
    * never updates. */
   SDL_PumpEvents();

   sdl3_manage_text_input();
   sdl3_poll_mouse(sdl);
   sdl3_poll_touch(sdl);

   sdl->mouse_wu = false;
   sdl->mouse_wd = false;
   sdl->mouse_wl = false;
   sdl->mouse_wr = false;

   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_KEY_DOWN, SDL_EVENT_MOUSE_REMOVED) > 0)
   {
      if (     event.type == SDL_EVENT_KEY_DOWN
            || event.type == SDL_EVENT_KEY_UP)
      {
         uint16_t mod        = sdl3_translate_mod(event.key.mod);
         unsigned code       = input_keymaps_translate_keysym_to_rk(
               event.key.key);
         uint32_t character  = 0;

#ifdef WEBOS
         input_driver_state_t *input_st = input_state_get_ptr();
         bool osk_active = input_st && (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED);

         if (!osk_active)
            sdl_webos_phys_kbd_typing = false;

         switch ((int) event.key.scancode)
         {
            case SDL_SCANCODE_WEBOS_BACK:
               /* Because webOS is sending DOWN/UP at the same time,
                  we save this flag for later */
               sdl_webos_special_keymap[sdl_webos_spkey_back] |= event.type == SDL_EVENT_KEY_DOWN;
               code = RETROK_BACKSPACE;
               break;
            case SDL_SCANCODE_WEBOS_RED:
               code = RETROK_x;
               break;
            case SDL_SCANCODE_WEBOS_GREEN:
               code = RETROK_z;
               break;
            case SDL_SCANCODE_WEBOS_YELLOW:
               code = RETROK_s;
               break;
            case SDL_SCANCODE_WEBOS_BLUE:
               code = RETROK_a;
               break;
            case SDL_SCANCODE_WEBOS_EXIT:
               code = RETROK_F1;
               break;
            case SDL_SCANCODE_UP:
            case SDL_SCANCODE_DOWN:
            case SDL_SCANCODE_LEFT:
            case SDL_SCANCODE_RIGHT:
               /* Default: Magic Remote → OSK grid. After BT typing keys,
                * ←/→ move the caret and ↑/↓ act as home/end. */
               if (osk_active && !sdl_webos_phys_kbd_typing)
               {
                  if (event.type == SDL_EVENT_KEY_DOWN)
                  {
                     if (event.key.scancode == SDL_SCANCODE_UP)
                        sdl_webos_special_keymap[sdl_webos_spkey_up] = 1;
                     else if (event.key.scancode == SDL_SCANCODE_DOWN)
                        sdl_webos_special_keymap[sdl_webos_spkey_down] = 1;
                     else if (event.key.scancode == SDL_SCANCODE_LEFT)
                        sdl_webos_special_keymap[sdl_webos_spkey_left] = 1;
                     else
                        sdl_webos_special_keymap[sdl_webos_spkey_right] = 1;
                  }
                  continue;
               }
               break;
            case SDL_SCANCODE_RETURN:
               /* Default: remote OK → OSK select. After BT typing → save. */
               if (osk_active && !sdl_webos_phys_kbd_typing)
               {
                  if (event.type == SDL_EVENT_KEY_DOWN)
                     sdl_webos_special_keymap[sdl_webos_spkey_return] = 1;
                  continue;
               }
               break;
            default:
               break;
         }

         /* Letters / numpad / backspace / punctuation ⇒ BT keyboard session.
          * Remote digit row is excluded (see sdl_webos_scancode_enables_phys_kbd). */
         if (osk_active
               && event.type == SDL_EVENT_KEY_DOWN
               && sdl_webos_scancode_enables_phys_kbd(event.key.scancode))
            sdl_webos_phys_kbd_typing = true;

         /* Disable cursor when using the buttons */
         if (code && code != RETROK_RETURN)
            SDL_webOSCursorVisibility(false);

#endif
         /* Allow pasting the clipboard. */
         if (     event.type == SDL_EVENT_KEY_DOWN
               && event.key.key == SDLK_V
               && (event.key.mod & SDL_KMOD_CTRL)
               && input_state_get_ptr()->keyboard_line.enabled)
         {
            sdl3_paste_clipboard();
            continue;
         }

         character = sdl3_translate_control_key(code, mod);
#ifdef WEBOS
         if (code == RETROK_RETURN || code == RETROK_KP_ENTER)
            character = '\r';

         /* Numpad Enter is never sent by the Magic Remote; always save. */
         if (code == RETROK_KP_ENTER)
            sdl_webos_phys_kbd_typing = true;
#endif

         input_keyboard_event(event.type == SDL_EVENT_KEY_DOWN,
               code, character, mod,
               RETRO_DEVICE_KEYBOARD);
      }
      else if (event.type == SDL_EVENT_TEXT_INPUT)
      {
         const char *text = event.text.text;
         uint16_t mod = sdl3_translate_mod(SDL_GetModState());

         while (text && *text)
            input_keyboard_event(true, RETROK_UNKNOWN,
                  utf8_walk(&text), mod, RETRO_DEVICE_KEYBOARD);
      }
      else if (event.type == SDL_EVENT_MOUSE_WHEEL)
      {
         float wx = event.wheel.x;
         float wy = event.wheel.y;

         /* FLIPPED = "natural" scrolling: SDL delivers inverted
          * signs and expects the caller to negate them. */
         if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
         {
            wx = -wx;
            wy = -wy;
         }
         sdl->mouse_wu |= wy > 0;
         sdl->mouse_wd |= wy < 0;
         sdl->mouse_wl |= wx < 0;
         sdl->mouse_wr |= wx > 0;
      }
      else if (event.type == SDL_EVENT_KEYMAP_CHANGED)
         sdl3_build_scancode_lut(sdl);
   }

   /* Neither range is consumed anywhere: sdl3_poll_touch reads finger
    * state by polling instead of by event, and pens aren't wired up at
    * all. Both fire at device rate for as long as there's contact, so
    * left in the queue they grow until SDL's queue fills and starts
    * refusing pushes - at which point the events that do matter (quit,
    * keys) get dropped along with them. */
   SDL_FlushEvents(SDL_EVENT_FINGER_DOWN,      SDL_EVENT_FINGER_CANCELED);
   SDL_FlushEvents(SDL_EVENT_PEN_PROXIMITY_IN, SDL_EVENT_PEN_AXIS);
}

static void sdl3_grab_mouse(void *data, bool state)
{
   SDL_Window *win = sdl3_get_window();

   if (win)
   {
      SDL_SetWindowMouseGrab(win, state);
      /* Relative mouse mode matches the game-focus behaviour of
       * the other desktop input drivers (winraw/x11/udev). */
      SDL_SetWindowRelativeMouseMode(win, state);
   }
}

static uint64_t sdl3_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD)
         | (1 << RETRO_DEVICE_MOUSE)
         | (1 << RETRO_DEVICE_KEYBOARD)
         | (1 << RETRO_DEVICE_LIGHTGUN)
         | (1 << RETRO_DEVICE_POINTER)
         | (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_sdl3 = {
   sdl3_input_init,
   sdl3_input_poll,
   sdl3_input_state,
   sdl3_input_free,
   sdl3_set_sensor_state,
   NULL,                   /* get_sensor_input */
   sdl3_get_capabilities,
   "sdl3",
   sdl3_grab_mouse,
   NULL,                   /* grab_stdin */
   NULL                    /* keypress_vibrate */
};
