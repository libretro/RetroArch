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
#include "../../apple_runtime.h"
#include <unistd.h>
#include <string.h>

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../input_keymaps.h"

#include "cocoa_input.h"

#include "../../retroarch.h"
#include "../../driver.h"
#include "../../configuration.h"

#include "../drivers_keyboard/keyboard_event_apple.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"
#include "../../ui/ui_companion_driver.h"
#ifdef __MACH__
#include <TargetConditionals.h>
#endif

#ifdef HAVE_COREMOTION
#import <CoreMotion/CoreMotion.h>
static CMMotionManager *motionManager;
#endif
#ifdef HAVE_MFI
#import <GameController/GameController.h>
#endif
/* CoreHaptics needs an iOS 13 SDK. Built against an older one there is
 * no RAKeypressHaptics class, its lookup answers nil, and keypresses
 * take the feedback generator instead. */
#if TARGET_OS_IOS && defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
#define RARCH_SDK_COREHAPTICS 1
#import <CoreHaptics/CoreHaptics.h>
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
#if TARGET_OS_OSX
float cocoa_screen_get_backing_scale_factor(void);
#endif

#if TARGET_OS_IPHONE
/* TODO/FIXME - static globals */
static bool small_keyboard_active = false;
static icade_map_t icade_maps[MAX_ICADE_PROFILES][MAX_ICADE_KEYS];
#if TARGET_OS_IOS
/* Fallback for iOS 10-13: a UISelectionFeedbackGenerator (iOS 10),
 * made by class name and driven by selector */
static id feedbackGenerator;

#ifdef RARCH_SDK_COREHAPTICS
#define KEYPRESS_HAPTIC_AVAIL API_AVAILABLE(ios(14.0))
static CHHapticEngine *keypressHapticEngine KEYPRESS_HAPTIC_AVAIL;
static id<CHHapticPatternPlayer> keypressHapticPlayer KEYPRESS_HAPTIC_AVAIL;

/* The keypress haptics are iOS 14 CoreHaptics. They live in a class
 * carrying that availability, so its methods use the API directly, and
 * the driver - which checks the OS first - reaches them by selector
 * through the class looked up once. */
KEYPRESS_HAPTIC_AVAIL
@interface RAKeypressHaptics : NSObject
+ (void)startEngine;
+ (void)vibrate;
+ (void)stopEngine;
@end
#endif

/* nil when this build has no CoreHaptics; looked up once either way */
static id cocoa_keypress_haptics(void)
{
   static id  cls;
   static int looked_up;
   if (!looked_up)
   {
      cls       = apple_rt_class("RAKeypressHaptics");
      looked_up = 1;
   }
   return cls;
}
#endif
#endif

static bool apple_key_state[MAX_KEYS];

/* Drops every key that is currently held. The release is published to
 * the input layer as well as cleared locally, so a core's keyboard
 * callback, the menu's flush-and-wait-for-release and anything else
 * driven by key events see the key-up that the window or the
 * application never got to deliver. */
void apple_input_keyboard_reset(void)
{
   unsigned i;

   for (i = 1; i < MAX_KEYS; i++)
   {
      if (!apple_key_state[i])
         continue;
      apple_key_state[i] = false;
      input_keyboard_event(false,
            input_keymaps_translate_keysym_to_rk(i),
            0, 0, RETRO_DEVICE_KEYBOARD);
   }

#if TARGET_OS_IPHONE
   /* The small-keyboard layer latches on a held modifier, so it goes
    * with the keys it was tracking. */
   small_keyboard_active = false;
#endif
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
   bool keyboard_gamepad_enable = input_config_get_keyboard_gamepad_enable();
   bool small_keyboard_enable   = input_config_get_small_keyboard_enable();
   unsigned original_code       = code;

   if (keyboard_gamepad_enable)
   {
      if (apple_input_handle_icade_event(
               input_config_get_keyboard_gamepad_mapping_type(),
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

   /* Update state for both original and translated keys if different */
   if (original_code > 0 && original_code < MAX_KEYS)
      apple_key_state[original_code] = down;

   if (code == 0 || code >= MAX_KEYS)
      return;

   if (code != original_code)
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
   if (apple_runtime_available(APPLE_RUNTIME_VER(10, 15, 0), 0, 0))
      if (!motionManager)
         motionManager = [[CMMotionManager alloc] init];
#endif

#if TARGET_OS_IOS
   if (     apple_runtime_available(0, APPLE_RUNTIME_VER(14, 0, 0), 0)
         && cocoa_keypress_haptics())
      apple_rt_send_void(cocoa_keypress_haptics(), sel_registerName("startEngine"));
   else if (apple_runtime_available(0, APPLE_RUNTIME_VER(10, 0, 0), 0))
   {
      if (!feedbackGenerator)
         feedbackGenerator = [[apple_rt_class("UISelectionFeedbackGenerator") alloc] init];
      apple_rt_send_void(feedbackGenerator, sel_registerName("prepare"));
   }
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
#if !TARGET_OS_IPHONE
   float   backing_scale_factor = cocoa_screen_get_backing_scale_factor();
#else
   int     backing_scale_factor = 1;
#endif

   if (!apple)
      return;

   {
      uint32_t pos        = apple->window_pos;
      uint32_t last       = apple->mouse_last;
      apple->mouse_rel    = COCOA_POS_PACK(
            COCOA_POS_X(pos) - COCOA_POS_X(last),
            COCOA_POS_Y(pos) - COCOA_POS_Y(last));
      apple->mouse_last   = pos;
   }

   for (i = 0; i < apple->touch_count || i == 0; i++)
   {
      struct video_viewport vp;
      cocoa_touch_data_t *touch = &apple->touches[i];
      int screen_x              = COCOA_POS_X(touch->screen_pos) * backing_scale_factor;
      int screen_y              = COCOA_POS_Y(touch->screen_pos) * backing_scale_factor;

      memset(&vp, 0, sizeof(vp));

      /* each position is written whole by the translation, and left as
       * it was if that fails */
      input_driver_translate_coord_viewport_confined_wrap(
            &vp, screen_x, screen_y,
            &touch->confined_pos, &touch->full_pos);

      input_driver_translate_coord_viewport_wrap(
            &vp, screen_x, screen_y,
            &touch->fixed_pos, &touch->full_pos);
   }
}

static int16_t cocoa_lightgun_aiming_state(
      cocoa_input_data_t *apple, unsigned idx, unsigned id)
{
   struct video_viewport vp    = {0};
   uint32_t res_pos            = 0;
   uint32_t res_screen_pos     = 0;

   int16_t x = COCOA_POS_X(apple->window_pos);
   int16_t y = COCOA_POS_Y(apple->window_pos);

#if !TARGET_OS_IPHONE
   x *= cocoa_screen_get_backing_scale_factor();
   y *= cocoa_screen_get_backing_scale_factor();
#endif

   if (input_driver_translate_coord_viewport_wrap(
               &vp, x, y,
               &res_pos, &res_screen_pos))
   {
      switch (id)
      {
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            return VIDEO_POS_X(res_pos);
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            return VIDEO_POS_Y(res_pos);
         case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
            return input_driver_pointer_is_offscreen(VIDEO_POS_X(res_pos), VIDEO_POS_Y(res_pos));
         default:
            break;
      }
   }

   return 0;
}

/* What the mouse is holding, for the controls bound to its buttons.
 * There is the one mouse, and it is the port's whose Mouse Index is 0:
 * the rule the lightgun's buttons went by here. Left, right and the
 * wheel; the other buttons are not kept. */
static unsigned cocoa_bind_mouse_buttons(void *data, unsigned port)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;
   unsigned held             = 0;

   if (!apple || input_config_get_mouse_index(port) != 0)
      return 0;
   if (apple->mouse_buttons & 1) held |= INPUT_POINTER_LEFT;
   if (apple->mouse_buttons & 2) held |= INPUT_POINTER_RIGHT;
   if (apple->mouse_wu)          held |= INPUT_POINTER_WHEEL_UP;
   if (apple->mouse_wd)          held |= INPUT_POINTER_WHEEL_DOWN;
   if (apple->mouse_wl)          held |= INPUT_POINTER_HWHEEL_UP;
   if (apple->mouse_wr)          held |= INPUT_POINTER_HWHEEL_DOWN;
   return held;
}

/* Which of @keys are down: bit n of @down for keys[n]. */
static void cocoa_keys_down(void *data, unsigned port,
      const uint16_t *keys, const uint8_t *bind, unsigned count,
      uint32_t *down)
{
   unsigned i;
   (void)data;
   (void)port;
   (void)bind;
   for (i = 0; i < count; i++)
      if (apple_key_state[rarch_keysym_lut[keys[i]]])
         down[i >> 5] |= (1u << (i & 31));
}

static int16_t cocoa_input_state(
      void *data,
      const input_device_driver_t *joypad,
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
      /* The RetroPad's buttons, the hotkeys and a stick's axes, where
       * they are bound to keys or mouse buttons, are the frontend's to
       * answer: it asks cocoa_keys_down() for the keys once a poll. */
      case RETRO_DEVICE_KEYBOARD:
         return (id && id < RETROK_LAST) && apple_key_state[rarch_keysym_lut[(enum retro_key)id]];
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         switch (id)
         {
         case RETRO_DEVICE_ID_MOUSE_X:
            if (device == RARCH_DEVICE_MOUSE_SCREEN)
            {
#if TARGET_OS_IPHONE
               return COCOA_POS_X(apple->window_pos);
#else
               return COCOA_POS_X(apple->window_pos) * cocoa_screen_get_backing_scale_factor();
#endif
            }
            return COCOA_POS_X(apple->mouse_rel);
         case RETRO_DEVICE_ID_MOUSE_Y:
            if (device == RARCH_DEVICE_MOUSE_SCREEN)
            {
#if TARGET_OS_IPHONE
               return COCOA_POS_Y(apple->window_pos);
#else
               return COCOA_POS_Y(apple->window_pos) * cocoa_screen_get_backing_scale_factor();
#endif
            }
            return COCOA_POS_Y(apple->mouse_rel);
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
                           return (COCOA_POS_X(touch->full_pos)  != -0x8000) && (COCOA_POS_Y(touch->full_pos)  != -0x8000); /* Inside? */
                        return    (COCOA_POS_X(touch->fixed_pos) != -0x8000) && (COCOA_POS_Y(touch->fixed_pos) != -0x8000); /* Inside? */
                     case RETRO_DEVICE_ID_POINTER_X:
                        return (device == RARCH_DEVICE_POINTER_SCREEN) ? COCOA_POS_X(touch->full_pos) : COCOA_POS_X(touch->confined_pos);
                     case RETRO_DEVICE_ID_POINTER_Y:
                        return (device == RARCH_DEVICE_POINTER_SCREEN) ? COCOA_POS_Y(touch->full_pos) : COCOA_POS_Y(touch->confined_pos);
                     case RETRO_DEVICE_ID_POINTER_COUNT:
                        return apple->touch_count;
                     case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
                        return input_driver_pointer_is_offscreen(COCOA_POS_X(touch->fixed_pos), COCOA_POS_Y(touch->fixed_pos));
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
            /* The buttons are what they are bound to - a pad's
             * button or axis, a key, a mouse button - and are the
             * frontend's to answer: it has the pad and the keys, and
             * asks cocoa_bind_mouse_buttons() for the mouse. */
            default:
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

#if TARGET_OS_IOS
   if (     apple_runtime_available(0, APPLE_RUNTIME_VER(14, 0, 0), 0)
         && cocoa_keypress_haptics())
      apple_rt_send_void(cocoa_keypress_haptics(), sel_registerName("stopEngine"));
   else if (feedbackGenerator)
   {
      RARCH_RELEASE(feedbackGenerator);
      feedbackGenerator = nil;
   }
#endif

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
   if (apple_runtime_available(APPLE_RUNTIME_VER(11, 0, 0), APPLE_RUNTIME_VER(14, 0, 0), APPLE_RUNTIME_VER(14, 0, 0)))
   {
      for (GCController *controller in [GCController controllers])
      {
         if (!controller || controller.playerIndex != port)
            continue;
         if (!controller.motion)
            break;
         /* GCMotion's activation API is macOS 11 / iOS 14, sent by
          * selector behind the check above */
         if (action == RETRO_SENSOR_ACCELEROMETER_ENABLE
               && !apple_rt_get_bool(controller.motion,
                  sel_registerName("hasGravityAndUserAcceleration")))
            break;
         if (action == RETRO_SENSOR_GYROSCOPE_ENABLE
               && !(  apple_rt_get_bool(controller.motion, sel_registerName("hasAttitude"))
                   && apple_rt_get_bool(controller.motion, sel_registerName("hasRotationRate"))))
            break;
         if (apple_rt_get_bool(controller.motion,
                  sel_registerName("sensorsRequireManualActivation")))
         {
            /* This is a bug, we assume if you turn on/off either
             * you want both on/off */
            apple_rt_send_bool(controller.motion,
                  sel_registerName("setSensorsActive:"),
                     (action == RETRO_SENSOR_ACCELEROMETER_ENABLE)
                  || (action == RETRO_SENSOR_GYROSCOPE_ENABLE));
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

#if TARGET_OS_IOS && defined(HAVE_COREMOTION)
/* Rotate a 2D sensor vector from the device's hardware coordinate frame
 * to the current screen coordinate frame.  Accelerometer and gyroscope
 * X/Y axes are fixed to the hardware (portrait) orientation, so they need
 * remapping when the interface is in landscape or upside-down. */
static void cocoa_sensor_rotate_xy(float *x, float *y)
{
   float rawX = *x, rawY = *y;
   UIInterfaceOrientation orient;
   if (apple_runtime_available(0, APPLE_RUNTIME_VER(16, 0, 0), 0)) {
      /* -[UIWindowScene effectiveGeometry].interfaceOrientation, both
       * iOS 16; the selectors are looked up once. */
      static SEL sel_geometry;
      static SEL sel_orientation;
      id geometry;
      UIWindow *window = [[UIApplication sharedApplication] delegate].window;
      if (!window) {
         return;
      }
      if (!sel_geometry)
      {
         sel_geometry    = sel_registerName("effectiveGeometry");
         sel_orientation = sel_registerName("interfaceOrientation");
      }
      geometry = apple_rt_get_id(apple_rt_get_id(window,
               sel_registerName("windowScene")), sel_geometry);
      orient   = (UIInterfaceOrientation)apple_rt_get_long(geometry,
            sel_orientation);
   } else {
      /* Deprecated in iOS 13 and the only source before it */
      orient = (UIInterfaceOrientation)apple_rt_get_long(
            [UIApplication sharedApplication],
            sel_registerName("statusBarOrientation"));
   }
   switch (orient)
   {
      case UIInterfaceOrientationLandscapeLeft:
         *x =  rawY;
         *y = -rawX;
         break;
      case UIInterfaceOrientationLandscapeRight:
         *x = -rawY;
         *y =  rawX;
         break;
      case UIInterfaceOrientationPortraitUpsideDown:
         *x = -rawX;
         *y = -rawY;
         break;
      default:
         break;
   }
}
#endif

#ifdef HAVE_MFI
/* -[GCMotion acceleration] (macOS 11 / iOS 14) returns a GCAcceleration,
 * three doubles; the layout is restated here so the file does not need
 * an SDK that declares the type. */
typedef struct
{
   double x, y, z;
} cocoa_input_accel_t;

static cocoa_input_accel_t cocoa_input_motion_acceleration(id motion)
{
   static SEL sel;
   if (!sel)
      sel = sel_registerName("acceleration");
   return apple_rt_get_large_struct(cocoa_input_accel_t, motion, sel);
}
#endif

static float cocoa_input_get_sensor_input(void *data, unsigned port, unsigned id)
{
#ifdef HAVE_MFI
   if (apple_runtime_available(APPLE_RUNTIME_VER(11, 0, 0), APPLE_RUNTIME_VER(14, 0, 0), APPLE_RUNTIME_VER(14, 0, 0)))
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
               return cocoa_input_motion_acceleration(controller.motion).x;
            case RETRO_SENSOR_ACCELEROMETER_Y:
               return cocoa_input_motion_acceleration(controller.motion).y;
            case RETRO_SENSOR_ACCELEROMETER_Z:
               return cocoa_input_motion_acceleration(controller.motion).z;
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
         case RETRO_SENSOR_ACCELEROMETER_Y:
         {
            float x = motionManager.deviceMotion.gravity.x
                  + motionManager.deviceMotion.userAcceleration.x;
            float y = motionManager.deviceMotion.gravity.y
                  + motionManager.deviceMotion.userAcceleration.y;
#if TARGET_OS_IOS
            cocoa_sensor_rotate_xy(&x, &y);
#endif
            return (id == RETRO_SENSOR_ACCELEROMETER_X) ? x : y;
         }
         case RETRO_SENSOR_ACCELEROMETER_Z:
            return motionManager.deviceMotion.gravity.z
                  + motionManager.deviceMotion.userAcceleration.z;
         case RETRO_SENSOR_GYROSCOPE_X:
         case RETRO_SENSOR_GYROSCOPE_Y:
         {
            float x = motionManager.deviceMotion.rotationRate.x;
            float y = motionManager.deviceMotion.rotationRate.y;
#if TARGET_OS_IOS
            cocoa_sensor_rotate_xy(&x, &y);
#endif
            return (id == RETRO_SENSOR_GYROSCOPE_X) ? x : y;
         }
         case RETRO_SENSOR_GYROSCOPE_Z:
            return motionManager.deviceMotion.rotationRate.z;
      }
   }
#endif

   return 0.0f;
}

#if TARGET_OS_IOS
#ifdef RARCH_SDK_COREHAPTICS
@implementation RAKeypressHaptics

+ (void)startEngine
{
   if (!keypressHapticEngine && CHHapticEngine.capabilitiesForHardware.supportsHaptics)
   {
      NSError *error;
      keypressHapticEngine = [[CHHapticEngine alloc] initAndReturnError:&error];
      if (!error)
      {
         [keypressHapticEngine startAndReturnError:&error];
         if (!error)
         {
            keypressHapticEngine.stoppedHandler = ^(CHHapticEngineStoppedReason reason) {
               /* Engine stopped (backgrounding/interruption) - clear player but keep engine */
               keypressHapticPlayer = nil;
            };
            keypressHapticEngine.resetHandler = ^{
               if (keypressHapticEngine)
                  [keypressHapticEngine startAndReturnError:nil];
            };
         }
      }
   }
}

+ (void)vibrate
{
   /* Reinitialize engine if iOS stopped it (e.g., during backgrounding) */
   if (!keypressHapticEngine)
      [self startEngine];

   if (!keypressHapticEngine)
      return;

   /* Ensure engine is started (may have been stopped by backgrounding) */
   NSError *error;
   [keypressHapticEngine startAndReturnError:&error];
   if (error)
   {
      /* Engine couldn't start - recreate it */
      keypressHapticEngine = nil;
      keypressHapticPlayer = nil;
      [self startEngine];
      if (!keypressHapticEngine)
         return;
   }
   unsigned rumble_gain = input_config_get_rumble_gain();
   float intensity = (float)rumble_gain / 100.0f;

   /* Create player on first use */
   if (!keypressHapticPlayer)
   {
      CHHapticEventParameter *intense;
      CHHapticEventParameter *sharp;
      CHHapticEvent *event;
      CHHapticPattern *pattern;

      intense = [[CHHapticEventParameter alloc]
                 initWithParameterID:CHHapticEventParameterIDHapticIntensity
                 value:intensity];
      sharp   = [[CHHapticEventParameter alloc]
                 initWithParameterID:CHHapticEventParameterIDHapticSharpness
                 value:1.0];
      event   = [[CHHapticEvent alloc]
               initWithEventType:CHHapticEventTypeHapticTransient
               parameters:[NSArray arrayWithObjects:intense, sharp, nil]
               relativeTime:0];
      pattern = [[CHHapticPattern alloc]
                 initWithEvents:[NSArray arrayWithObject:event]
                 parameters:[[NSArray alloc] init]
                 error:&error];

      if (error)
         return;

      keypressHapticPlayer = [keypressHapticEngine createPlayerWithPattern:pattern error:&error];
      if (error)
         return;
   }
   else
   {
      /* Update intensity for existing player */
      if (keypressHapticPlayer)
      {
         CHHapticDynamicParameter *param = [[CHHapticDynamicParameter alloc]
            initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
                          value:intensity
                   relativeTime:0];
         [keypressHapticPlayer sendParameters:[NSArray arrayWithObject:param] atTime:0 error:&error];
      }
   }

   if (keypressHapticPlayer)
      [keypressHapticPlayer startAtTime:0 error:&error];
}

+ (void)stopEngine
{
   if (keypressHapticEngine)
   {
      keypressHapticEngine.stoppedHandler = ^(CHHapticEngineStoppedReason reason) {};
      keypressHapticEngine.resetHandler = ^{};
      [keypressHapticEngine stopWithCompletionHandler:^(NSError *error) {
         keypressHapticPlayer = nil;
         keypressHapticEngine = nil;
      }];
   }
}

@end
#endif

static void cocoa_input_keypress_vibrate(void)
{
   if (     apple_runtime_available(0, APPLE_RUNTIME_VER(14, 0, 0), 0)
         && cocoa_keypress_haptics())
      apple_rt_send_void(cocoa_keypress_haptics(), sel_registerName("vibrate"));
   else if (feedbackGenerator)
   {
      apple_rt_send_void(feedbackGenerator, sel_registerName("selectionChanged"));
      apple_rt_send_void(feedbackGenerator, sel_registerName("prepare"));
   }
}
#endif

#if TARGET_OS_OSX
static void cocoa_input_grab_mouse(void *data, bool state)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   if (state)
   {
      NSWindow *window      = (BRIDGE NSWindow*)ui_companion_cocoa.get_main_window(nil);
      /* NSWindow's frame method is declared as a plain getter (not
       * @property) on the 10.5-10.9 SDKs, so dot-syntax fails on
       * GCC 4.0.  And on 32-bit Darwin, NSPoint and CGPoint are
       * separate incompatible types — only unified on LP64.  Use
       * bracket syntax and build a CGPoint from the float fields
       * directly. */
      NSRect window_frame   = [window frame];
      CGPoint window_center = CGPointMake(
            window_frame.origin.x + window_frame.size.width  / 2.0f,
            window_frame.origin.y + window_frame.size.height / 2.0f);
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

   if (apple_runtime_available(0, APPLE_RUNTIME_VER(14, 0, 0), 0))
      apple_rt_send_void([CocoaView get],
            sel_registerName("setNeedsUpdateOfPrefersPointerLocked"));
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
#if TARGET_OS_OSX || TARGET_OS_IOS
   cocoa_input_grab_mouse,
#else
   NULL,                         /* grab_mouse */
#endif
   NULL,                         /* grab_stdin */
#if TARGET_OS_IOS
   cocoa_input_keypress_vibrate,
#else
   NULL,                         /* vibrate */
#endif
   NULL,                         /* survives_video */
   cocoa_keys_down,
   cocoa_bind_mouse_buttons
};

/* What the Apple UI hands the Cocoa input driver.
 *
 * The UI gets the platform's mouse, pointer and touch events and used
 * to write them into this driver's data itself, which it took out of
 * the input state. It calls these instead: the data is the driver's,
 * and only the driver writes it. Each does what the UI's code did.
 * All of them are on the main thread, where the UI's events arrive,
 * and do nothing while the Cocoa driver is not the one in use. */
static cocoa_input_data_t *cocoa_input_current(void)
{
   return (cocoa_input_data_t*)input_driver_current_data();
}

/* The mouse moved by @dx, @dy and is now at @x, @y in the window. */
void cocoa_input_mouse_moved(int16_t dx, int16_t dy, int16_t x, int16_t y)
{
   cocoa_input_data_t *apple = cocoa_input_current();
   if (!apple)
      return;
   /* Relative */
   apple->mouse_rel           = COCOA_POS_PACK(
         COCOA_POS_X(apple->mouse_rel) + dx,
         COCOA_POS_Y(apple->mouse_rel) + dy);
   /* Absolute */
   apple->touches[0].screen_pos = COCOA_POS_PACK(x, y);
   if (apple->mouse_grabbed)
      apple->window_pos       = COCOA_POS_PACK(
            COCOA_POS_X(apple->window_pos) + dx,
            COCOA_POS_Y(apple->window_pos) + dy);
   else
      apple->window_pos       = COCOA_POS_PACK(x, y);
}

/* A mouse with no position of its own moved the pointer by @dx, @dy
 * (iOS, GCMouse). */
void cocoa_input_mouse_moved_by(int16_t dx, int16_t dy)
{
   cocoa_input_data_t *apple = cocoa_input_current();
   if (!apple)
      return;
   apple->window_pos = COCOA_POS_PACK(
         COCOA_POS_X(apple->window_pos) + dx,
         COCOA_POS_Y(apple->window_pos) + dy);
}

/* Mouse button @number went down or up. With @as_touch it is the one
 * touch of a pointer too, as a click is on macOS. */
void cocoa_input_mouse_button(unsigned number, bool down, bool as_touch)
{
   cocoa_input_data_t *apple = cocoa_input_current();
   if (!apple || number >= 32)
      return;
   if (down)
      apple->mouse_buttons |=  (1U << number);
   else
      apple->mouse_buttons &= ~(1U << number);
   if (as_touch)
      apple->touch_count     = down ? 1 : 0;
}

/* The pointer hovers at @x, @y, with nothing pressed (iOS, a trackpad
 * or a mouse over the view). */
void cocoa_input_pointer_at(int16_t x, int16_t y)
{
   cocoa_input_data_t *apple = cocoa_input_current();
   if (!apple)
      return;
   apple->touches[0].screen_pos = COCOA_POS_PACK(x, y);
   apple->window_pos          = COCOA_POS_PACK(x, y);
}

/* The touches on screen are given anew: none, then one call of
 * cocoa_input_touch_add() for each, which says false once there is no
 * room for more. */
void cocoa_input_touches_begin(void)
{
   cocoa_input_data_t *apple = cocoa_input_current();
   if (apple)
      apple->touch_count = 0;
}

bool cocoa_input_touch_add(int16_t x, int16_t y)
{
   cocoa_input_data_t *apple = cocoa_input_current();
   if (!apple || apple->touch_count >= MAX_TOUCHES)
      return false;
   apple->touches[apple->touch_count++].screen_pos = COCOA_POS_PACK(x, y);
   return true;
}

/* Every touch is let go and forgotten: the application lost the
 * screen and will not see them end. */
void cocoa_input_touches_reset(void)
{
   cocoa_input_data_t *apple = cocoa_input_current();
   if (!apple)
      return;
   apple->touch_count = 0;
   memset(apple->touches, 0, sizeof(apple->touches));
}

/* Whether the mouse is held in the window. */
bool cocoa_input_mouse_grabbed(void)
{
   cocoa_input_data_t *apple = cocoa_input_current();
   return apple && apple->mouse_grabbed;
}
