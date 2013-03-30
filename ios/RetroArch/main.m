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

#import <UIKit/UIKit.h>
#include "input/ios_input.h"
#include "input/keycode.h"
#include "libretro.h"

#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11
#define GSEVENT_TYPE_MODS 12
#define GSEVENT_MOD_CMD (1 << 16)
#define GSEVENT_MOD_SHIFT (1 << 17)
#define GSEVENT_MOD_ALT (1 << 19)
#define GSEVENT_MOD_CTRL (1 << 20)

uint32_t ios_key_list[MAX_KEYS];
uint32_t ios_touch_count;
touch_data_t ios_touch_list[MAX_TOUCHES];

// Input helpers
static uint32_t translate_mods(uint32_t flags)
{
   uint32_t result = 0;
   if (flags & GSEVENT_MOD_ALT)   result |= RETROKMOD_ALT;
   if (flags & GSEVENT_MOD_CMD)   result |= RETROKMOD_META;
   if (flags & GSEVENT_MOD_SHIFT) result |= RETROKMOD_SHIFT;
   if (flags & GSEVENT_MOD_CTRL)  result |= RETROKMOD_CTRL;
   return result;
}

@interface RApplication : UIApplication
@end

@implementation RApplication

- (void)sendEvent:(UIEvent *)event
{
   [super sendEvent:event];
   
   if ([[event allTouches] count])
   {
      NSArray* touches = [[event allTouches] allObjects];
      const int numTouches = [touches count];
      const float scale = [[UIScreen mainScreen] scale];

      ios_touch_count = 0;
   
      for(int i = 0; i != numTouches && ios_touch_count < MAX_TOUCHES; i ++)
      {
         UITouch* touch = [touches objectAtIndex:i];
         const CGPoint coord = [touch locationInView:touch.view];

         if (touch.phase != UITouchPhaseEnded && touch.phase != UITouchPhaseCancelled)
         {
            ios_touch_list[ios_touch_count   ].screen_x = coord.x * scale;
            ios_touch_list[ios_touch_count ++].screen_y = coord.y * scale;
         }
      }
   }
   // Stolen from: http://nacho4d-nacho4d.blogspot.com/2012/01/catching-keyboard-events-in-ios.html
   else if ([event respondsToSelector:@selector(_gsEvent)])
   {
      uint8_t* eventMem = (uint8_t*)(void*)CFBridgingRetain([event performSelector:@selector(_gsEvent)]);
      int eventType = eventMem ? *(int*)&eventMem[8] : 0;
      
      if (eventType == GSEVENT_TYPE_KEYDOWN || eventType == GSEVENT_TYPE_KEYUP)
      {
         uint16_t* data = (uint16_t*)&eventMem[0x3C];

         if (data[0] < MAX_KEYS)
            ios_key_list[data[0]] = (eventType == GSEVENT_TYPE_KEYDOWN) ? 1 : 0;

         // Key events
         ios_add_key_event(eventType == GSEVENT_TYPE_KEYDOWN, data[0], data[1], translate_mods(*(uint32_t*)&eventMem[0x30]));
         // printf("%d %d %d %08X\n", data[0], data[1], data[2], *(uint32_t*)&eventMem[0x30]);
      }
      else if(eventType == GSEVENT_TYPE_MODS)
      {
         static const struct
         {
            unsigned key;
            unsigned retrokey;
            uint32_t hidid;
         }  modmap[] =
         {
            { 0x37, RETROK_LMETA, KEY_LeftGUI },
            { 0x36, RETROK_RMETA, KEY_RightGUI },
            { 0x38, RETROK_LSHIFT, KEY_LeftShift },
            { 0x3C, RETROK_RSHIFT, KEY_RightShift },
            { 0x3A, RETROK_LALT, KEY_LeftAlt },
            { 0x3D, RETROK_RALT, KEY_RightAlt },
            { 0x3B, RETROK_LCTRL, KEY_LeftControl },
            { 0x3E, RETROK_RCTRL, KEY_RightControl },
            { 0x39, RETROK_CAPSLOCK, KEY_CapsLock },
            { 0, RETROK_UNKNOWN, 0}
         };
         
         static bool keystate[9];
         
         // TODO: Not sure how to add this.
         //       The key value indicates the key that was pressed or released.
         //       The flags indicates the current modifier state.
         //       There is no way to determine if this is a keydown or a keyup event,
         //       except to look at the flags, but the bits in flags are shared between
         //       the left and right versions of a given key pair.
         //       The current method assumes that all key up and down events are processed,
         //       otherwise it may become confused.
         const uint32_t key = *(uint32_t*)&eventMem[0x3C];
         
         for (int i = 0; i < 9; i ++)
         {
            if (key == modmap[i].key)
            {
               keystate[i] = !keystate[i];
               ios_key_list[modmap[i].hidid] = keystate[i];
               ios_add_key_event(keystate[i], modmap[i].retrokey, 0, translate_mods(*(uint32_t*)&eventMem[0x30]));
            }
         }
      }

      CFBridgingRelease(eventMem);
   }
}

@end

int main(int argc, char *argv[])
{
    @autoreleasepool {
        return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
    }
}

