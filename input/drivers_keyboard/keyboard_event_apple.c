/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "../input_common.h"
#include "../input_keymaps.h"
#include "../keyboard_line.h"
#include "../drivers/cocoa_input.h"
#include "../../general.h"
#include "../../driver.h"

#include "../drivers/apple_keycode.h"

#if defined(HAVE_COCOATOUCH)

#define HIDKEY(X) X

#elif defined(HAVE_COCOA)

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
#endif

#if TARGET_OS_IPHONE
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
   driver_t *driver = driver_get_ptr();
   cocoa_input_data_t *apple = (cocoa_input_data_t*)driver->input_data;
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
      apple->small_keyboard_active = down;
      *code = 0;
      return true;
   }
   
   translated_code = (*code < 128) ? mapping[*code] : 0;
   
   /* Allow old keys to be released. */
   if (!down && apple->key_state[*code])
      return false;

   if ((!down && apple->key_state[translated_code]) ||
       apple->small_keyboard_active)
   {
      *code = translated_code;
      return true;
   }

   return false;
}

extern const struct rarch_key_map rarch_key_map_apple_hid[];

typedef struct icade_map
{
    bool up;
    enum retro_key key;
} icade_map_t;

#define MAX_ICADE_PROFILES 2
#define MAX_ICADE_KEYS     0x100

static icade_map_t icade_maps[MAX_ICADE_PROFILES][MAX_ICADE_KEYS];

static bool handle_icade_event(unsigned *code, bool *keydown)
{
    static bool initialized = false;
    bool ret = false;
    unsigned kb_type_idx = 1;
    
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
        
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_a)].key  = RETROK_LEFT;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_c)].key  = RETROK_RIGHT;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_d)].key  = RETROK_RIGHT;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_e)].key  = RETROK_UP;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_f)].key  = RETROK_z;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_h)].key = RETROK_x;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_i)].key = RETROK_q;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_j)].key = RETROK_a;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_k)].key = RETROK_w;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_m)].key = RETROK_q;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_n)].key = RETROK_a;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_p)].key = RETROK_w;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_q)].key = RETROK_LEFT;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_r)].key = RETROK_x;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_u)].key = RETROK_z;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_w)].key = RETROK_UP;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_x)].key = RETROK_DOWN;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_z)].key = RETROK_DOWN;
            
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_c)].up   = true;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_e)].up   = true;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_f)].up   = true;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_m)].up  = true;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_n)].up  = true;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_p)].up  = true;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_q)].up  = true;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_r)].up  = true;
        icade_maps[j][input_keymaps_translate_rk_to_keysym(RETROK_z)].up  = true;
        
        initialized = true;
    }

   if ((*code < 0x20) && (icade_maps[kb_type_idx][*code].key != RETROK_UNKNOWN))
   {
       *keydown     = icade_maps[kb_type_idx][*code].up ? false : true;
       ret          = true;
       *code        = input_keymaps_translate_rk_to_keysym(icade_maps[kb_type_idx][*code].key);
   }
    
    return ret;
}
#endif

void cocoa_input_keyboard_event(bool down,
      unsigned code, uint32_t character, uint32_t mod, unsigned device)
{
   driver_t *driver = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   cocoa_input_data_t *apple = (cocoa_input_data_t*)driver->input_data;

   if (!apple)
      return;

   code = HIDKEY(code);

#if TARGET_OS_IPHONE
   if (settings->input.icade_enable)
   {
      if (handle_icade_event(&code, &down))
          character = 0;
      else
          code      = 0;
   }
   else if (settings->input.small_keyboard_enable)
   {
      if (handle_small_keyboard(&code, down))
         character = 0;
   }
#endif
   
   if (code == 0 || code >= MAX_KEYS)
      return;

   apple->key_state[code] = down;
    
   input_keyboard_event(down,
         input_keymaps_translate_keysym_to_rk(code),
         character, (enum retro_mod)mod, device);
}
