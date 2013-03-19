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

#define GSEVENT_TYPE 2
#define GSEVENT_FLAGS 12
#define GSEVENTKEY_KEYCODE 15
#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11

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
      int* eventMem = (int *)(void*)CFBridgingRetain([event performSelector:@selector(_gsEvent)]);
      int eventType = eventMem ? eventMem[GSEVENT_TYPE] : 0;
      
      if (eventMem && (eventType == GSEVENT_TYPE_KEYDOWN || eventType == GSEVENT_TYPE_KEYUP))
      {
         // Read keycode from GSEventKey
         int tmp = eventMem[GSEVENTKEY_KEYCODE];
         UniChar *keycode = (UniChar *)&tmp;
         
         if (keycode[0] < MAX_KEYS)
            ios_key_list[keycode[0]] = (eventType == GSEVENT_TYPE_KEYDOWN) ? 1 : 0;
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

