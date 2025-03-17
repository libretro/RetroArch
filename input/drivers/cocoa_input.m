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
#include "../../ui/drivers/cocoa/cocoa_common.h"
#include "../../ui/ui_companion_driver.h"

#ifdef HAVE_COREMOTION
#import <CoreMotion/CoreMotion.h>
static CMMotionManager *motionManager;
#endif
#ifdef HAVE_MFI
#import <GameController/GameController.h>
#endif

#if TARGET_OS_IPHONE
#define HIDKEY(X) X
#else
#define HIDKEY(X) (X < 128) ? MAC_NATIVE_TO_HID[X] : 0
#endif

#define MAX_ICADE_PROFILES 4
#define MAX_ICADE_KEYS     0x100

typedef struct icade_map
{
   bool up;
   enum retro_key key;
} icade_map_t;

/* TODO/FIXME -
 * fix game focus toggle */

/*
 * FORWARD DECLARATIONS
 */
#ifdef OSX
float cocoa_screen_get_backing_scale_factor(void);
#endif

#if TARGET_OS_IPHONE
/* TODO/FIXME - static globals */
static bool small_keyboard_active = false;
static icade_map_t icade_maps[MAX_ICADE_PROFILES][MAX_ICADE_KEYS];
#if TARGET_OS_IOS
static UISelectionFeedbackGenerator *feedbackGenerator;
#endif
#endif

static bool apple_key_state[MAX_KEYS];

void apple_input_keyboard_reset(void)
{
   memset(apple_key_state, 0, sizeof(apple_key_state));
}

/* Send keyboard inputs directly using RETROK_* codes
 * Used by the iOS custom keyboard implementation */
void apple_direct_input_keyboard_event(bool down,
      unsigned code, uint32_t character, uint32_t mod, unsigned device)
{
    int apple_key              = rarch_keysym_lut[code];

    if (!apple_key)
       return;

    apple_key_state[apple_key] = down;
    input_keyboard_event(down,
          code,
          character, (enum retro_mod)mod, device);
}

#if TARGET_OS_IPHONE
static bool apple_input_handle_small_keyboard(unsigned* code, bool down)
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

static bool apple_input_handle_icade_event(unsigned kb_type_idx, unsigned *code, bool *keydown)
{
   static bool initialized = false;
   bool ret                = false;

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

   if ((*code < MAX_ICADE_KEYS) && (icade_maps[kb_type_idx][*code].key != RETROK_UNKNOWN))
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

   if (keyboard_gamepad_enable)
   {
      if (apple_input_handle_icade_event(
               settings->uints.input_keyboard_gamepad_mapping_type,
               &code, &down))
         character = 0;
      else
         code      = 0;
   }
   else if (small_keyboard_enable)
   {
      if (apple_input_handle_small_keyboard(&code, down))
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
void apple_input_keyboard_event(bool down,
      unsigned code, uint32_t character, uint32_t mod, unsigned device)
{
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
   code                  = HIDKEY(code);
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
   cocoa_input_data_t *apple = NULL;
#ifdef HAVE_COREMOTION
   if (@available(macOS 10.15, *))
      if (!motionManager)
         motionManager = [[CMMotionManager alloc] init];
#endif

#if TARGET_OS_IOS
   if (!feedbackGenerator)
      feedbackGenerator = [[UISelectionFeedbackGenerator alloc] init];
   [feedbackGenerator prepare];
#endif

   /* TODO/FIXME - shouldn't we free the above in case this fails for
    * TARGET_OS_IOS / HAVE_COREMOTION? */
   if (!(apple = (cocoa_input_data_t*)calloc(1, sizeof(*apple))))
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
#else
   int     backing_scale_factor = 1;
#endif

   if (!apple)
      return;

   apple->mouse_rel_x = apple->window_pos_x - apple->mouse_x_last;
   apple->mouse_x_last = apple->window_pos_x;

   apple->mouse_rel_y = apple->window_pos_y - apple->mouse_y_last;
   apple->mouse_y_last = apple->window_pos_y;

   for (i = 0; i < apple->touch_count || i == 0; i++)
   {
      struct video_viewport vp;

      memset(&vp, 0, sizeof(vp));

      video_driver_translate_coord_viewport_confined_wrap(
            &vp,
            apple->touches[i].screen_x * backing_scale_factor,
            apple->touches[i].screen_y * backing_scale_factor,
            &apple->touches[i].confined_x,
            &apple->touches[i].confined_y,
            &apple->touches[i].full_x,
            &apple->touches[i].full_y);

      video_driver_translate_coord_viewport_wrap(
            &vp,
            apple->touches[i].screen_x * backing_scale_factor,
            apple->touches[i].screen_y * backing_scale_factor,
            &apple->touches[i].fixed_x,
            &apple->touches[i].fixed_y,
            &apple->touches[i].full_x,
            &apple->touches[i].full_y);
   }
}

static int16_t cocoa_lightgun_aiming_state(
      cocoa_input_data_t *apple, unsigned idx, unsigned id)
{
   struct video_viewport vp    = {0};
   int16_t res_x               = 0;
   int16_t res_y               = 0;
   int16_t res_screen_x        = 0;
   int16_t res_screen_y        = 0;
   
   int16_t x = apple->window_pos_x;
   int16_t y = apple->window_pos_y;

#ifndef IOS
   x *= cocoa_screen_get_backing_scale_factor();
   y *= cocoa_screen_get_backing_scale_factor();
#endif

   if (video_driver_translate_coord_viewport_wrap(
               &vp, x, y,
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

   return 0;
}

static bool cocoa_mouse_button_pressed(
      cocoa_input_data_t *apple, unsigned port, unsigned key)
{
   switch (key)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return apple->mouse_buttons & 1;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return apple->mouse_buttons & 2;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return false;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return apple->mouse_wu;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return apple->mouse_wd;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         return apple->mouse_wl;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         return apple->mouse_wr;
   }

   return false;
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
                  if (     (binds[port][i].key && binds[port][i].key < RETROK_LAST)
                        && apple_key_state[rarch_keysym_lut[binds[port][i].key]])
                     ret |= (1 << i);
               }
            }
            return ret;
         }

         if (binds[port][id].valid)
         {
            if (id < RARCH_BIND_LIST_END)
            {
               if (     (binds[port][id].key && binds[port][id].key < RETROK_LAST)
                     && apple_key_state[rarch_keysym_lut[binds[port][id].key]]
                     && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                  )
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         {
            int16_t ret           = 0;
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid        = binds[port][id_minus].valid;
            id_plus_valid         = binds[port][id_plus].valid;
            id_minus_key          = binds[port][id_minus].key;
            id_plus_key           = binds[port][id_plus].key;

            if (id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
            {
               if (apple_key_state[rarch_keysym_lut[(enum retro_key)id_plus_key]])
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
            {
               if (apple_key_state[rarch_keysym_lut[(enum retro_key)id_minus_key]])
                  ret += -0x7fff;
            }
            return ret;
         }
         break;

      case RETRO_DEVICE_KEYBOARD:
         return (id && id < RETROK_LAST) && apple_key_state[rarch_keysym_lut[(enum retro_key)id]];
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
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
            return apple->mouse_rel_x;
         case RETRO_DEVICE_ID_MOUSE_Y:
            if (device == RARCH_DEVICE_MOUSE_SCREEN)
            {
#ifdef IOS
               return apple->window_pos_y;
#else
               return apple->window_pos_y * cocoa_screen_get_backing_scale_factor();
#endif
            }
            return apple->mouse_rel_y;
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
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         {
            // with a physical mouse that is hovering, the touch_count will be 0
            // and apple->touches[0] will have the hover position
            if ((idx == 0 || idx < apple->touch_count) && (idx < MAX_TOUCHES))
            {
               const cocoa_touch_data_t *touch = (const cocoa_touch_data_t *)
                  &apple->touches[idx];

               if (touch)
               {
                  switch (id)
                  {
                     case RETRO_DEVICE_ID_POINTER_PRESSED:
                        if (!apple->touch_count)
                           return 0;
                        if (device == RARCH_DEVICE_POINTER_SCREEN)
                           return (touch->full_x  != -0x8000) && (touch->full_y  != -0x8000); /* Inside? */
                        return    (touch->fixed_x != -0x8000) && (touch->fixed_y != -0x8000); /* Inside? */
                     case RETRO_DEVICE_ID_POINTER_X:
                        return (device == RARCH_DEVICE_POINTER_SCREEN) ? touch->full_x : touch->confined_x;
                     case RETRO_DEVICE_ID_POINTER_Y:
                        return (device == RARCH_DEVICE_POINTER_SCREEN) ? touch->full_y : touch->confined_y;
                     case RETRO_DEVICE_ID_POINTER_COUNT:
                        return apple->touch_count;
                     case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
                        return input_driver_pointer_is_offscreen(touch->fixed_x, touch->fixed_y);
                  }
               }
            }
         }
         break;
      case RETRO_DEVICE_LIGHTGUN:
         switch (id)
         {
            /*aiming*/
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
               return cocoa_lightgun_aiming_state(apple, idx, id);
            /*buttons*/
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
            case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
               {
                  unsigned new_id                = input_driver_lightgun_id_convert(id);
                  const uint32_t bind_joykey     = input_config_binds[port][new_id].joykey;
                  const uint32_t bind_joyaxis    = input_config_binds[port][new_id].joyaxis;
                  const uint32_t autobind_joykey = input_autoconf_binds[port][new_id].joykey;
                  const uint32_t autobind_joyaxis= input_autoconf_binds[port][new_id].joyaxis;
                  uint16_t joyport               = joypad_info->joy_idx;
                  float axis_threshold           = joypad_info->axis_threshold;
                  const uint32_t joykey          = (bind_joykey != NO_BTN) ? bind_joykey  : autobind_joykey;
                  const uint32_t joyaxis         = (bind_joyaxis != AXIS_NONE) ? bind_joyaxis : autobind_joyaxis;
                  
                  if (binds[port][new_id].valid)
                  {
                     if ((uint16_t)joykey != NO_BTN && joypad->button(joyport, (uint16_t)joykey))
                        return 1;
                     if (joyaxis != AXIS_NONE &&
                         ((float)abs(joypad->axis(joyport, joyaxis))
                          / 0x8000) > axis_threshold)
                        return 1;
                     else if ((binds[port][new_id].key && binds[port][new_id].key < RETROK_LAST)
                              && !keyboard_mapping_blocked
                              && apple_key_state[rarch_keysym_lut[(enum retro_key)binds[port][new_id].key]])
                        return 1;
                     else
                     {
                        settings_t *settings = config_get_ptr();
                        if (settings->uints.input_mouse_index[port] == 0)
                        {
                           if (cocoa_mouse_button_pressed(apple, port, binds[port][new_id].mbutton))
                              return 1;
                        }
                     }
                  }
               }
               break;
         }
         break;
   }

   return 0;
}

static void cocoa_input_free(void *data)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   if (!apple || !data)
      return;

   memset(apple_key_state, 0, sizeof(apple_key_state));

   free(apple);
}

static uint64_t cocoa_input_get_capabilities(void *data)
{
   return
        (1 << RETRO_DEVICE_JOYPAD)
      | (1 << RETRO_DEVICE_MOUSE)
      | (1 << RETRO_DEVICE_KEYBOARD)
      | (1 << RETRO_DEVICE_LIGHTGUN)
      | (1 << RETRO_DEVICE_POINTER)
      | (1 << RETRO_DEVICE_ANALOG);
}

static bool cocoa_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   if (   (action != RETRO_SENSOR_ACCELEROMETER_ENABLE)
       && (action != RETRO_SENSOR_ACCELEROMETER_DISABLE)
       && (action != RETRO_SENSOR_GYROSCOPE_ENABLE)
       && (action != RETRO_SENSOR_GYROSCOPE_DISABLE))
      return false;

#ifdef HAVE_MFI
   if (@available(iOS 14.0, macOS 11.0, tvOS 14.0, *))
   {
      for (GCController *controller in [GCController controllers])
      {
         if (!controller || controller.playerIndex != port)
            continue;
         if (!controller.motion)
            break;
         if (action == RETRO_SENSOR_ACCELEROMETER_ENABLE && !controller.motion.hasGravityAndUserAcceleration)
            break;
         if (action == RETRO_SENSOR_GYROSCOPE_ENABLE && !controller.motion.hasAttitudeAndRotationRate)
            break;
         if (controller.motion.sensorsRequireManualActivation)
         {
            /* This is a bug, we assume if you turn on/off either
             * you want both on/off */
            if (     (action == RETRO_SENSOR_ACCELEROMETER_ENABLE)
                  || (action == RETRO_SENSOR_GYROSCOPE_ENABLE))
               controller.motion.sensorsActive = YES;
            else
               controller.motion.sensorsActive = NO;
         }
         /* no such thing as update interval for GCController? */
         return true;
      }
   }
#endif

#ifdef HAVE_COREMOTION
   if (port != 0)
      return false;

   if (!motionManager || !motionManager.deviceMotionAvailable)
      return false;

   if (     (action == RETRO_SENSOR_ACCELEROMETER_ENABLE)
         || (action == RETRO_SENSOR_GYROSCOPE_ENABLE))
   {
      if (!motionManager.deviceMotionActive)
         [motionManager startDeviceMotionUpdates];
      motionManager.deviceMotionUpdateInterval = 1.0f / (float)rate;
   }
   else
   {
      if (motionManager.deviceMotionActive)
         [motionManager stopDeviceMotionUpdates];
   }

   return true;
#else
   return false;
#endif
}

static float cocoa_input_get_sensor_input(void *data, unsigned port, unsigned id)
{
#ifdef HAVE_MFI
   if (@available(iOS 14.0, macOS 11.0, tvOS 14.0, *))
   {
      for (GCController *controller in [GCController controllers])
      {
         if (!controller || controller.playerIndex != port)
            continue;
         if (!controller.motion)
            break;
         switch (id)
         {
            case RETRO_SENSOR_ACCELEROMETER_X:
               return controller.motion.acceleration.x;
            case RETRO_SENSOR_ACCELEROMETER_Y:
               return controller.motion.acceleration.y;
            case RETRO_SENSOR_ACCELEROMETER_Z:
               return controller.motion.acceleration.z;
            case RETRO_SENSOR_GYROSCOPE_X:
               return controller.motion.rotationRate.x;
            case RETRO_SENSOR_GYROSCOPE_Y:
               return controller.motion.rotationRate.y;
            case RETRO_SENSOR_GYROSCOPE_Z:
               return controller.motion.rotationRate.z;
         }
      }
   }
#endif

#ifdef HAVE_COREMOTION
   if (port == 0 && motionManager && motionManager.deviceMotionActive)
   {
      switch (id)
      {
         case RETRO_SENSOR_ACCELEROMETER_X:
            return motionManager.deviceMotion.gravity.x + motionManager.deviceMotion.userAcceleration.x;
         case RETRO_SENSOR_ACCELEROMETER_Y:
            return motionManager.deviceMotion.gravity.y + motionManager.deviceMotion.userAcceleration.y;
         case RETRO_SENSOR_ACCELEROMETER_Z:
            return motionManager.deviceMotion.gravity.z + motionManager.deviceMotion.userAcceleration.z;
         case RETRO_SENSOR_GYROSCOPE_X:
            return motionManager.deviceMotion.rotationRate.x;
         case RETRO_SENSOR_GYROSCOPE_Y:
            return motionManager.deviceMotion.rotationRate.y;
         case RETRO_SENSOR_GYROSCOPE_Z:
            return motionManager.deviceMotion.rotationRate.z;
      }
   }
#endif

   return 0.0f;
}

#if TARGET_OS_IOS
static void cocoa_input_keypress_vibrate(void)
{
   [feedbackGenerator selectionChanged];
   [feedbackGenerator prepare];
}
#endif

#ifdef OSX
static void cocoa_input_grab_mouse(void *data, bool state)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   if (state)
   {
      NSWindow *window      = (BRIDGE NSWindow*)ui_companion_cocoa.get_main_window(nil);
      CGPoint window_pos    = window.frame.origin;
      CGSize window_size    = window.frame.size;
      CGPoint window_center = CGPointMake(window_pos.x + window_size.width / 2.0f, window_pos.y + window_size.height / 2.0f);
      CGWarpMouseCursorPosition(window_center);
   }

   CGAssociateMouseAndMouseCursorPosition(!state);
   cocoa_show_mouse(nil, !state);
   apple->mouse_grabbed = state;
}
#elif TARGET_OS_IOS
static void cocoa_input_grab_mouse(void *data, bool state)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   apple->mouse_grabbed = state;

   if (@available(iOS 14, *))
      [[CocoaView get] setNeedsUpdateOfPrefersPointerLocked];
}
#endif

input_driver_t input_cocoa = {
   cocoa_input_init,
   cocoa_input_poll,
   cocoa_input_state,
   cocoa_input_free,
   cocoa_input_set_sensor_state,
   cocoa_input_get_sensor_input,
   cocoa_input_get_capabilities,
   "cocoa",
#if defined(OSX) || TARGET_OS_IOS
   cocoa_input_grab_mouse,
#else
   NULL,                         /* grab_mouse */
#endif
   NULL,                         /* grab_stdin */
#if TARGET_OS_IOS
   cocoa_input_keypress_vibrate
#else
   NULL                          /* vibrate */
#endif
};
