/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include <unistd.h>
#include "../../input/input_common.h"
#include "../../performance.h"
#include "../../general.h"
#include "../../driver.h"

#ifdef WIIMOTE
#include "BTStack/wiimote.h"
#endif

#define MAX_TOUCH 16
#define MAX_KEYS 256

extern NSString* const GSEventKeyDownNotification;
extern NSString* const GSEventKeyUpNotification;
extern NSString* const RATouchNotification;
static const struct rarch_key_map rarch_key_map_hidusage[];

struct
{
   bool is_down;
   int16_t screen_x, screen_y;
   int16_t fixed_x, fixed_y;
   int16_t full_x, full_y;
} ios_touches[MAX_TOUCH];

uint32_t ios_current_touch_count = 0;


// Input responder
static bool ios_keys[MAX_KEYS];

@interface RAInputResponder : NSObject
@end

static RAInputResponder* input_driver;

@implementation RAInputResponder
-(id)init
{
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyPressed:) name: GSEventKeyDownNotification object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyReleased:) name: GSEventKeyUpNotification object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleTouches:) name: RATouchNotification object:nil];
   return self;
}

-(void)dealloc
{
   [[NSNotificationCenter defaultCenter] removeObserver:self];
}

-(void)keyPressed: (NSNotification*)notification
{
   int keycode = [[notification.userInfo objectForKey:@"keycode"] intValue];
   if (keycode < MAX_KEYS) ios_keys[keycode] = true;
}

-(void)keyReleased: (NSNotification*)notification
{
   int keycode = [[notification.userInfo objectForKey:@"keycode"] intValue];
   if (keycode < MAX_KEYS) ios_keys[keycode] = false;
}

-(void)handleTouches:(NSNotification*)notification
{
   UIEvent* event = [notification.userInfo objectForKey:@"event"];
   NSArray* touches = [[event allTouches] allObjects];

   ios_current_touch_count = [touches count];
   
   for(int i = 0; i != ios_current_touch_count; i ++)
   {
      UITouch *touch = [touches objectAtIndex:i];
      CGPoint coord = [touch locationInView:touch.view];
      float scale = [[UIScreen mainScreen] scale];

      ios_touches[i].is_down = (touch.phase != UITouchPhaseEnded) && (touch.phase != UITouchPhaseCancelled);
      ios_touches[i].screen_x = coord.x * scale;
      ios_touches[i].screen_y = coord.y * scale;
   }
}
@end

static bool l_ios_is_key_pressed(enum retro_key key)
{
   if ((int)key >= 0 && key < RETROK_LAST)
   {
      int hidkey = input_translate_rk_to_keysym(key);
      return (hidkey < MAX_KEYS) ? ios_keys[hidkey] : false;
   }
   
   return false;
}

static int16_t l_ios_joypad_device_state(const struct retro_keybind **binds_,
      unsigned port_num, unsigned id)
{
   const struct retro_keybind *binds = binds_[port_num];
   if (id < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[id];
      return bind->valid && l_ios_is_key_pressed(bind->key);
   }
   else
      return 0;
}

static void *ios_input_init(void)
{
   input_init_keyboard_lut(rarch_key_map_hidusage);

   if (!input_driver)
      input_driver = [RAInputResponder new];

   ios_current_touch_count = 0;
   memset(ios_touches, 0, sizeof(ios_touches));
   memset(ios_keys, 0, sizeof(ios_keys));
   return (void*)-1;
}

static void ios_input_free_input(void *data)
{
   (void)data;
   input_driver = nil;
}

static void ios_input_poll(void *data)
{
   for (int i = 0; i != ios_current_touch_count; i ++)
   {
      input_translate_coord_viewport(ios_touches[i].screen_x, ios_touches[i].screen_y,
         &ios_touches[i].fixed_x, &ios_touches[i].fixed_y,
         &ios_touches[i].full_x, &ios_touches[i].full_y);
   }
}

static int16_t ios_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
#ifdef WIIMOTE
         if (myosd_num_of_joys > 0)
         {
            struct wiimote_t* wm = &joys[0];
         
            switch (id)
            {
               case RETRO_DEVICE_ID_JOYPAD_A: return IS_PRESSED(wm, WIIMOTE_BUTTON_TWO);
               case RETRO_DEVICE_ID_JOYPAD_B: return IS_PRESSED(wm, WIIMOTE_BUTTON_ONE);
               case RETRO_DEVICE_ID_JOYPAD_START: return IS_PRESSED(wm, WIIMOTE_BUTTON_PLUS);
               case RETRO_DEVICE_ID_JOYPAD_SELECT: return IS_PRESSED(wm, WIIMOTE_BUTTON_MINUS);
               case RETRO_DEVICE_ID_JOYPAD_UP: return IS_PRESSED(wm, WIIMOTE_BUTTON_RIGHT);
               case RETRO_DEVICE_ID_JOYPAD_DOWN: return IS_PRESSED(wm, WIIMOTE_BUTTON_LEFT);
               case RETRO_DEVICE_ID_JOYPAD_LEFT: return IS_PRESSED(wm, WIIMOTE_BUTTON_UP);
               case RETRO_DEVICE_ID_JOYPAD_RIGHT: return IS_PRESSED(wm, WIIMOTE_BUTTON_DOWN);
            }
         }
#endif
      
         return l_ios_joypad_device_state(binds, port, id);
      case RETRO_DEVICE_ANALOG:
         return 0;
      case RETRO_DEVICE_MOUSE:
         return 0;
      case RETRO_DEVICE_POINTER:
         return 0;
      case RARCH_DEVICE_POINTER_SCREEN:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return (index < ios_current_touch_count) ? ios_touches[index].full_x : 0;
            case RETRO_DEVICE_ID_POINTER_Y:
               return (index < ios_current_touch_count) ? ios_touches[index].full_y : 0;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (index < ios_current_touch_count) ? ios_touches[index].is_down : 0;
            default:
               return 0;
         }
      case RETRO_DEVICE_KEYBOARD:
         return 0;
      case RETRO_DEVICE_LIGHTGUN:
         return 0;

      default:
         return 0;
   }
}

static bool ios_input_key_pressed(void *data, int key)
{
   const struct retro_keybind *binds = g_settings.input.binds[0];

   if (key >= 0 && key < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[key];
      return l_ios_is_key_pressed(bind->key);
   }

   return false;
}

const input_driver_t input_ios = {
   ios_input_init,
   ios_input_poll,
   ios_input_state,
   ios_input_key_pressed,
   ios_input_free_input,
   "ios_input",
};

// Key table
static const struct rarch_key_map rarch_key_map_hidusage[] = {
   { 0x50, RETROK_LEFT },
   { 0x4F, RETROK_RIGHT },
   { 0x52, RETROK_UP },
   { 0x51, RETROK_DOWN },
   { 0x28, RETROK_RETURN },
   { 0x2B, RETROK_TAB },
   { 0x49, RETROK_INSERT },
   { 0x4C, RETROK_DELETE },
   { 0xE5, RETROK_RSHIFT },
   { 0xE1, RETROK_LSHIFT },
   { 0xE0, RETROK_LCTRL },
   { 0x4D, RETROK_END },
   { 0x4A, RETROK_HOME },
   { 0x4E, RETROK_PAGEDOWN },
   { 0x4B, RETROK_PAGEUP },
   { 0xE2, RETROK_LALT },
   { 0x2C, RETROK_SPACE },
   { 0x29, RETROK_ESCAPE },
   { 0x2A, RETROK_BACKSPACE },
   { 0x58, RETROK_KP_ENTER },
   { 0x57, RETROK_KP_PLUS },
   { 0x56, RETROK_KP_MINUS },
   { 0x55, RETROK_KP_MULTIPLY },
   { 0x54, RETROK_KP_DIVIDE },
   { 0x35, RETROK_BACKQUOTE },
   { 0x48, RETROK_PAUSE },
   { 0x62, RETROK_KP0 },
   { 0x59, RETROK_KP1 },
   { 0x5A, RETROK_KP2 },
   { 0x5B, RETROK_KP3 },
   { 0x5C, RETROK_KP4 },
   { 0x5D, RETROK_KP5 },
   { 0x5E, RETROK_KP6 },
   { 0x5F, RETROK_KP7 },
   { 0x60, RETROK_KP8 },
   { 0x61, RETROK_KP9 },
   { 0x27, RETROK_0 },
   { 0x1E, RETROK_1 },
   { 0x1F, RETROK_2 },
   { 0x20, RETROK_3 },
   { 0x21, RETROK_4 },
   { 0x22, RETROK_5 },
   { 0x23, RETROK_6 },
   { 0x24, RETROK_7 },
   { 0x25, RETROK_8 },
   { 0x26, RETROK_9 },
   { 0x3A, RETROK_F1 },
   { 0x3B, RETROK_F2 },
   { 0x3C, RETROK_F3 },
   { 0x3D, RETROK_F4 },
   { 0x3E, RETROK_F5 },
   { 0x3F, RETROK_F6 },
   { 0x40, RETROK_F7 },
   { 0x41, RETROK_F8 },
   { 0x42, RETROK_F9 },
   { 0x43, RETROK_F10 },
   { 0x44, RETROK_F11 },
   { 0x45, RETROK_F12 },
   { 0x04, RETROK_a },
   { 0x05, RETROK_b },
   { 0x06, RETROK_c },
   { 0x07, RETROK_d },
   { 0x08, RETROK_e },
   { 0x09, RETROK_f },
   { 0x0A, RETROK_g },
   { 0x0B, RETROK_h },
   { 0x0C, RETROK_i },
   { 0x0D, RETROK_j },
   { 0x0E, RETROK_k },
   { 0x0F, RETROK_l },
   { 0x10, RETROK_m },
   { 0x11, RETROK_n },
   { 0x12, RETROK_o },
   { 0x13, RETROK_p },
   { 0x14, RETROK_q },
   { 0x15, RETROK_r },
   { 0x16, RETROK_s },
   { 0x17, RETROK_t },
   { 0x18, RETROK_u },
   { 0x19, RETROK_v },
   { 0x1A, RETROK_w },
   { 0x1B, RETROK_x },
   { 0x1C, RETROK_y },
   { 0x1D, RETROK_z },
   { 0, RETROK_UNKNOWN }
};
