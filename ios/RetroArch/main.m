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

#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11
#define GSEVENT_MOD_SHIFT = (1 << 17)
#define GSEVENT_MOD_CTRL = (1 << 18)
#define GSEVENT_MOD_ALT = (1 << 19)
#define GSEVENT_MOD_CMD = (1 << 20)

uint32_t ios_key_list[MAX_KEYS];
uint32_t ios_touch_count;
touch_data_t ios_touch_list[MAX_TOUCHES];

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
    
      printf("%d\n", eventType);
      
      if (eventType == GSEVENT_TYPE_KEYDOWN || eventType == GSEVENT_TYPE_KEYUP)
      {
         uint16_t* data = (uint16_t*)&eventMem[0x3C];

         if (data[0] < MAX_KEYS)
            ios_key_list[data[0]] = (eventType == GSEVENT_TYPE_KEYDOWN) ? 1 : 0;

         // HACK: These line up for now
         const uint32_t mods = (*(uint32_t*)&eventMem[0x30] >> 17) & 0xF;
         ios_add_key_event(eventType == GSEVENT_TYPE_KEYDOWN, data[0], data[1], mods);
         // printf("%d %d %d %08X\n", data[0], data[1], data[2], *(uint32_t*)&eventMem[0x30]);
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

