/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - Jason Fetters
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
#include <unistd.h>

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../input_keymaps.h"

#include "cocoa_input.h"

#include "../../retroarch.h"
#include "../../driver.h"

#include "../drivers_keyboard/keyboard_event_apple.h"

/* TODO/FIXME -
 * fix game focus toggle */

#ifdef OSX
/* Forward declaration */
float cocoa_screen_get_backing_scale_factor(void);
#endif

static bool apple_key_state[MAX_KEYS];

// Send keyboard inputs directly using RETROK_* codes
// Used by the iOS custom keyboard implementation
void apple_direct_input_keyboard_event(bool down,
      unsigned code, uint32_t character, uint32_t mod, unsigned device)
{
    int appleKey = rarch_keysym_lut[code];
    apple_key_state[appleKey] = down;
    input_keyboard_event(down,
          code,
          character, (enum retro_mod)mod, device);
}


#if TARGET_OS_IPHONE
/* TODO/FIXME - static globals */
static bool small_keyboard_active = false;

#define HIDKEY(X) X
#define MAX_ICADE_PROFILES 4
#define MAX_ICADE_KEYS     0x100

typedef struct icade_map
{
   bool up;
   enum retro_key key;
} icade_map_t;

static icade_map_t icade_maps[MAX_ICADE_PROFILES][MAX_ICADE_KEYS];

static bool handle_small_keyboard(unsigned* code, bool down)
{
   static uint8_t mapping[128];
   static bool map_initialized;
   static const struct { uint8_t orig; uint8_t mod; } mapping_def[] =
   {
      { KEY_Grave,      KEY_Escape     }, { KEY_1,          KEY_F1         },
      { KEY_2,          KEY_F2         }, { KEY_3,          KEY_F3         },
      { KEY_4,          KEY_F4         }, { KEY_5,          KEY_F5         },
      { KEY_6,          KEY_F6         }, { KEY_7,          KEY_F7         },
      { KEY_8,          KEY_F8         }, { KEY_9,          KEY_F9         },
      { KEY_0,          KEY_F10        }, { KEY_Minus,      KEY_F11        },
      { KEY_Equals,     KEY_F12        }, { KEY_Up,         KEY_PageUp     },
      { KEY_Down,       KEY_PageDown   }, { KEY_Left,       KEY_Home       },
      { KEY_Right,      KEY_End        }, { KEY_Q,          KP_7           },
      { KEY_W,          KP_8           }, { KEY_E,          KP_9           },
      { KEY_A,          KP_4           }, { KEY_S,          KP_5           },
      { KEY_D,          KP_6           }, { KEY_Z,          KP_1           },
      { KEY_X,          KP_2           }, { KEY_C,          KP_3           },
      { 0 }
   };
   unsigned translated_code  = 0;

   if (!map_initialized)
   {
      int i;
      for (i = 0; mapping_def[i].orig; i ++)
         mapping[mapping_def[i].orig] = mapping_def[i].mod;
      map_initialized = true;
   }

   if (*code == KEY_RightShift)
   {
      small_keyboard_active = down;
      *code = 0;
      return true;
   }

   if (*code < 128)
      translated_code = mapping[*code];

   /* Allow old keys to be released. */
   if (!down && apple_key_state[*code])
      return false;

   if ((!down && apple_key_state[translated_code]) ||
         small_keyboard_active)
   {
      *code = translated_code;
      return true;
   }

   return false;
}

static bool handle_icade_event(unsigned *code, bool *keydown)
{
   static bool initialized = false;
   bool ret                = false;
   settings_t *settings    = config_get_ptr();
   unsigned kb_type_idx    = settings->uints.input_keyboard_gamepad_mapping_type;

   if (!initialized)
   {
      unsigned i;
      unsigned j = 0;

      for (j = 0; j < MAX_ICADE_PROFILES; j++)
      {
         for (i = 0; i < MAX_ICADE_KEYS; i++)
         {
            icade_maps[j][i].key = RETROK_UNKNOWN;
            icade_maps[j][i].up  = false;
         }
      }

      /* iPega PG-9017 */
      j = 1;

      icade_maps[j][rarch_keysym_lut[RETROK_a]].key = RETROK_LEFT;
      icade_maps[j][rarch_keysym_lut[RETROK_q]].key = RETROK_LEFT;
      icade_maps[j][rarch_keysym_lut[RETROK_c]].key = RETROK_RIGHT;
      icade_maps[j][rarch_keysym_lut[RETROK_d]].key = RETROK_RIGHT;
      icade_maps[j][rarch_keysym_lut[RETROK_e]].key = RETROK_UP;
      icade_maps[j][rarch_keysym_lut[RETROK_w]].key = RETROK_UP;
      icade_maps[j][rarch_keysym_lut[RETROK_x]].key = RETROK_DOWN;
      icade_maps[j][rarch_keysym_lut[RETROK_z]].key = RETROK_DOWN;
      icade_maps[j][rarch_keysym_lut[RETROK_f]].key = RETROK_z;
      icade_maps[j][rarch_keysym_lut[RETROK_u]].key = RETROK_z;
      icade_maps[j][rarch_keysym_lut[RETROK_i]].key = RETROK_q;
      icade_maps[j][rarch_keysym_lut[RETROK_m]].key = RETROK_q;
      icade_maps[j][rarch_keysym_lut[RETROK_j]].key = RETROK_a;
      icade_maps[j][rarch_keysym_lut[RETROK_n]].key = RETROK_a;
      icade_maps[j][rarch_keysym_lut[RETROK_k]].key = RETROK_w;
      icade_maps[j][rarch_keysym_lut[RETROK_p]].key = RETROK_w;
      icade_maps[j][rarch_keysym_lut[RETROK_h]].key = RETROK_x;
      icade_maps[j][rarch_keysym_lut[RETROK_r]].key = RETROK_x;
      icade_maps[j][rarch_keysym_lut[RETROK_y]].key = RETROK_s;
      icade_maps[j][rarch_keysym_lut[RETROK_t]].key = RETROK_s;

      icade_maps[j][rarch_keysym_lut[RETROK_e]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_z]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_q]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_c]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_f]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_m]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_t]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_n]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_p]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_r]].up  = true;

      /* 8-bitty */
      j = 2;

      icade_maps[j][rarch_keysym_lut[RETROK_a]].key = RETROK_LEFT;
      icade_maps[j][rarch_keysym_lut[RETROK_q]].key = RETROK_LEFT;
      icade_maps[j][rarch_keysym_lut[RETROK_c]].key = RETROK_RIGHT;
      icade_maps[j][rarch_keysym_lut[RETROK_d]].key = RETROK_RIGHT;
      icade_maps[j][rarch_keysym_lut[RETROK_e]].key = RETROK_UP;
      icade_maps[j][rarch_keysym_lut[RETROK_w]].key = RETROK_UP;
      icade_maps[j][rarch_keysym_lut[RETROK_x]].key = RETROK_DOWN;
      icade_maps[j][rarch_keysym_lut[RETROK_z]].key = RETROK_DOWN;
      icade_maps[j][rarch_keysym_lut[RETROK_h]].key = RETROK_q;
      icade_maps[j][rarch_keysym_lut[RETROK_r]].key = RETROK_q;
      icade_maps[j][rarch_keysym_lut[RETROK_j]].key = RETROK_w;
      icade_maps[j][rarch_keysym_lut[RETROK_n]].key = RETROK_w;
      icade_maps[j][rarch_keysym_lut[RETROK_i]].key = RETROK_a;
      icade_maps[j][rarch_keysym_lut[RETROK_m]].key = RETROK_a;
      icade_maps[j][rarch_keysym_lut[RETROK_k]].key = RETROK_z;
      icade_maps[j][rarch_keysym_lut[RETROK_p]].key = RETROK_z;
      icade_maps[j][rarch_keysym_lut[RETROK_y]].key = RETROK_RSHIFT;
      icade_maps[j][rarch_keysym_lut[RETROK_t]].key = RETROK_RSHIFT;
      icade_maps[j][rarch_keysym_lut[RETROK_u]].key = RETROK_RETURN;
      icade_maps[j][rarch_keysym_lut[RETROK_f]].key = RETROK_RETURN;
      icade_maps[j][rarch_keysym_lut[RETROK_l]].key = RETROK_x;
      icade_maps[j][rarch_keysym_lut[RETROK_v]].key = RETROK_x;
      icade_maps[j][rarch_keysym_lut[RETROK_o]].key = RETROK_s;
      icade_maps[j][rarch_keysym_lut[RETROK_g]].key = RETROK_s;

      icade_maps[j][rarch_keysym_lut[RETROK_e]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_z]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_q]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_c]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_r]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_n]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_m]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_p]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_t]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_f]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_v]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_g]].up  = true;

      /* SNES30 8bitDo */
      j = 3;

      icade_maps[j][rarch_keysym_lut[RETROK_e]].key = RETROK_UP;
      icade_maps[j][rarch_keysym_lut[RETROK_w]].key = RETROK_UP;
      icade_maps[j][rarch_keysym_lut[RETROK_x]].key = RETROK_DOWN;
      icade_maps[j][rarch_keysym_lut[RETROK_z]].key = RETROK_DOWN;
      icade_maps[j][rarch_keysym_lut[RETROK_a]].key = RETROK_LEFT;
      icade_maps[j][rarch_keysym_lut[RETROK_q]].key = RETROK_LEFT;
      icade_maps[j][rarch_keysym_lut[RETROK_c]].key = RETROK_RIGHT;
      icade_maps[j][rarch_keysym_lut[RETROK_d]].key = RETROK_RIGHT;
      icade_maps[j][rarch_keysym_lut[RETROK_u]].key = RETROK_x;
      icade_maps[j][rarch_keysym_lut[RETROK_f]].key = RETROK_x;
      icade_maps[j][rarch_keysym_lut[RETROK_h]].key = RETROK_z;
      icade_maps[j][rarch_keysym_lut[RETROK_r]].key = RETROK_z;
      icade_maps[j][rarch_keysym_lut[RETROK_y]].key = RETROK_a;
      icade_maps[j][rarch_keysym_lut[RETROK_t]].key = RETROK_a;
      icade_maps[j][rarch_keysym_lut[RETROK_j]].key = RETROK_s;
      icade_maps[j][rarch_keysym_lut[RETROK_n]].key = RETROK_s;
      icade_maps[j][rarch_keysym_lut[RETROK_k]].key = RETROK_q;
      icade_maps[j][rarch_keysym_lut[RETROK_p]].key = RETROK_q;
      icade_maps[j][rarch_keysym_lut[RETROK_i]].key = RETROK_w;
      icade_maps[j][rarch_keysym_lut[RETROK_m]].key = RETROK_w;
      icade_maps[j][rarch_keysym_lut[RETROK_l]].key = RETROK_RSHIFT;
      icade_maps[j][rarch_keysym_lut[RETROK_v]].key = RETROK_RSHIFT;
      icade_maps[j][rarch_keysym_lut[RETROK_o]].key = RETROK_RETURN;
      icade_maps[j][rarch_keysym_lut[RETROK_g]].key = RETROK_RETURN;

      icade_maps[j][rarch_keysym_lut[RETROK_v]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_g]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_e]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_z]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_q]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_c]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_r]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_f]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_n]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_t]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_p]].up  = true;
      icade_maps[j][rarch_keysym_lut[RETROK_m]].up  = true;

      initialized = true;
   }

   if ((*code < 0x20) && (icade_maps[kb_type_idx][*code].key != RETROK_UNKNOWN))
   {
      *keydown     = icade_maps[kb_type_idx][*code].up ? false : true;
      ret          = true;
      *code        = rarch_keysym_lut[icade_maps[kb_type_idx][*code].key];
   }

   return ret;
}

void apple_input_keyboard_event(bool down,
      unsigned code, uint32_t character, uint32_t mod, unsigned device)
{
   settings_t *settings         = config_get_ptr();
   bool keyboard_gamepad_enable = settings->bools.input_keyboard_gamepad_enable;
   bool small_keyboard_enable   = settings->bools.input_small_keyboard_enable;
   code                         = HIDKEY(code);

   if (keyboard_gamepad_enable)
   {
      if (handle_icade_event(&code, &down))
         character = 0;
      else
         code      = 0;
   }
   else if (small_keyboard_enable)
   {
      if (handle_small_keyboard(&code, down))
         character = 0;
   }

   if (code == 0 || code >= MAX_KEYS)
      return;

   apple_key_state[code] = down;

   input_keyboard_event(down,
         input_keymaps_translate_keysym_to_rk(code),
         character, (enum retro_mod)mod, device);
}
#else
/* Taken from https://github.com/depp/keycode,
 * check keycode.h for license. */
static const unsigned char MAC_NATIVE_TO_HID[128] = {
   4, 22,  7,  9, 11, 10, 29, 27,  6, 25,255,  5, 20, 26,  8, 21,
   28, 23, 30, 31, 32, 33, 35, 34, 46, 38, 36, 45, 37, 39, 48, 18,
   24, 47, 12, 19, 40, 15, 13, 52, 14, 51, 49, 54, 56, 17, 16, 55,
   43, 44, 53, 42,255, 41,231,227,225, 57,226,224,229,230,228,255,
   108, 99,255, 85,255, 87,255, 83,255,255,255, 84, 88,255, 86,109,
   110,103, 98, 89, 90, 91, 92, 93, 94, 95,111, 96, 97,255,255,255,
   62, 63, 64, 60, 65, 66,255, 68,255,104,107,105,255, 67,255, 69,
   255,106,117, 74, 75, 76, 61, 77, 59, 78, 58, 80, 79, 81, 82,255
};

#define HIDKEY(X) (X < 128) ? MAC_NATIVE_TO_HID[X] : 0

void apple_input_keyboard_event(bool down,
      unsigned code, uint32_t character, uint32_t mod, unsigned device)
{
   code                         = HIDKEY(code);
   if (code == 0 || code >= MAX_KEYS)
      return;

   apple_key_state[code] = down;

   input_keyboard_event(down,
         input_keymaps_translate_keysym_to_rk(code),
         character, (enum retro_mod)mod, device);
}
#endif

static void *cocoa_input_init(const char *joypad_driver)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)calloc(1, sizeof(*apple));
   if (!apple)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_apple_hid);

   return apple;
}

static void cocoa_input_poll(void *data)
{
   uint32_t i;
   cocoa_input_data_t *apple    = (cocoa_input_data_t*)data;
#ifndef IOS
   float   backing_scale_factor = cocoa_screen_get_backing_scale_factor();
#endif

   if (!apple)
      return;

   for (i = 0; i < apple->touch_count; i++)
   {
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

#ifndef IOS
      apple->touches[i].screen_x *= backing_scale_factor;
      apple->touches[i].screen_y *= backing_scale_factor;
#endif
      video_driver_translate_coord_viewport_wrap(
            &vp,
            apple->touches[i].screen_x,
            apple->touches[i].screen_y,
            &apple->touches[i].fixed_x,
            &apple->touches[i].fixed_y,
            &apple->touches[i].full_x,
            &apple->touches[i].full_y);
   }
}

static int16_t cocoa_input_state(
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
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            /* Do a bitwise OR to combine both input
             * states together */
            int16_t ret = 0;

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if ((binds[port][i].key < RETROK_LAST) && apple_key_state[rarch_keysym_lut[binds[port][i].key]])
                     ret |= (1 << i);
               }
            }
            return ret;
         }

         if (binds[port][id].valid)
         {
            if (id < RARCH_BIND_LIST_END)
               if (apple_key_state[rarch_keysym_lut[binds[port][id].key]])
                  return 1;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && apple_key_state[rarch_keysym_lut[(enum retro_key)id]];
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         {
            int16_t val = 0;
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_X:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                  {
#ifdef IOS
                     return apple->window_pos_x;
#else
                     return apple->window_pos_x * cocoa_screen_get_backing_scale_factor();
#endif
                  }
#ifdef IOS
                    val = apple->mouse_rel_x;
#else
                  val = apple->window_pos_x - apple->mouse_x_last;
                  apple->mouse_x_last = apple->window_pos_x;
#endif
                  return val;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                  {
#ifdef IOS
                     return apple->window_pos_y;
#else
                     return apple->window_pos_y * cocoa_screen_get_backing_scale_factor();
#endif
                  }
#ifdef IOS
                    val = apple->mouse_rel_y;
#else
                  val = apple->window_pos_y - apple->mouse_y_last;
                  apple->mouse_y_last = apple->window_pos_y;
#endif
                  return val;
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return apple->mouse_buttons & 1;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return apple->mouse_buttons & 2;
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  return apple->mouse_wu;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  return apple->mouse_wd;
               case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
                  return apple->mouse_wl;
               case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
                  return apple->mouse_wr;
            }
         }
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         {
            const bool want_full = (device == RARCH_DEVICE_POINTER_SCREEN);

            if (idx < apple->touch_count && (idx < MAX_TOUCHES))
            {
               int16_t x, y;
               const cocoa_touch_data_t *touch = (const cocoa_touch_data_t *)
                  &apple->touches[idx];

               if (!touch)
                  return 0;

               x = touch->fixed_x;
               y = touch->fixed_y;

               if (want_full)
               {
                  x = touch->full_x;
                  y = touch->full_y;
               }

               switch (id)
               {
                  case RETRO_DEVICE_ID_POINTER_PRESSED:
                     return (x != -0x8000) && (y != -0x8000);
                  case RETRO_DEVICE_ID_POINTER_X:
                     return x;
                  case RETRO_DEVICE_ID_POINTER_Y:
                     return y;
                  case RETRO_DEVICE_ID_POINTER_COUNT:
                     return apple->touch_count;
               }
            }
         }
         break;
   }

   return 0;
}

static void cocoa_input_free(void *data)
{
   unsigned i;
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   if (!apple || !data)
      return;

   for (i = 0; i < MAX_KEYS; i++)
      apple_key_state[i] = 0;

   free(apple);
}

static uint64_t cocoa_input_get_capabilities(void *data)
{
   return
      (1 << RETRO_DEVICE_JOYPAD)   |
      (1 << RETRO_DEVICE_MOUSE)    |
      (1 << RETRO_DEVICE_KEYBOARD) |
      (1 << RETRO_DEVICE_POINTER)  |
      (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_cocoa = {
   cocoa_input_init,
   cocoa_input_poll,
   cocoa_input_state,
   cocoa_input_free,
   NULL,
   NULL,
   cocoa_input_get_capabilities,
   "cocoa",
   NULL,                         /* grab_mouse */
   NULL
};
