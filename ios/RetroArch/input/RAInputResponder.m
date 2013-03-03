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

#import "RAInputResponder.h"
#include "input/input_common.h"

extern NSString* const GSEventKeyDownNotification;
extern NSString* const GSEventKeyUpNotification;
extern NSString* const RATouchNotification;

@implementation RAInputResponder
{
   unsigned _touchCount;
   touch_data_t _touches[MAX_TOUCHES];
   bool _keys[MAX_KEYS];
}

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

- (void)poll
{
   for (int i = 0; i != _touchCount; i ++)
   {
      input_translate_coord_viewport(_touches[i].screen_x, _touches[i].screen_y,
         &_touches[i].fixed_x, &_touches[i].fixed_y,
         &_touches[i].full_x, &_touches[i].full_y);
   }
}

- (bool)isKeyPressed:(unsigned)index
{
   return (index < MAX_KEYS) ? _keys[index] : NO;
}

- (const touch_data_t*)getTouchDataAtIndex:(unsigned)index
{
   return (index < MAX_TOUCHES && _touches[index].is_down) ? &_touches[index] : 0;
}

// Response handlers
- (void)keyPressed:(NSNotification*)notification
{
   int keycode = [[notification.userInfo objectForKey:@"keycode"] intValue];
   if (keycode < MAX_KEYS) _keys[keycode] = true;
}

- (void)keyReleased:(NSNotification*)notification
{
   int keycode = [[notification.userInfo objectForKey:@"keycode"] intValue];
   if (keycode < MAX_KEYS) _keys[keycode] = false;
}

- (void)handleTouches:(NSNotification*)notification
{
   UIEvent* event = [notification.userInfo objectForKey:@"event"];
   NSArray* touches = [[event allTouches] allObjects];

   _touchCount = [touches count];
   
   for(int i = 0; i != _touchCount; i ++)
   {
      UITouch *touch = [touches objectAtIndex:i];
      CGPoint coord = [touch locationInView:touch.view];
      float scale = [[UIScreen mainScreen] scale];

      _touches[i].is_down = (touch.phase != UITouchPhaseEnded) && (touch.phase != UITouchPhaseCancelled);
      _touches[i].screen_x = coord.x * scale;
      _touches[i].screen_y = coord.y * scale;
   }
}
@end
