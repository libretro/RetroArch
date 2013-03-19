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

#import "settings.h"
#include "../input/ios_input.h"
#include "../input/keycode.h"
#include "../input/BTStack/wiimote.h"
#include "../input/BTStack/WiiMoteHelper.h"

static const struct
{
   const char* const keyname;
   const uint32_t hid_id;
} ios_key_name_map[] = {
   { "left", KEY_Left },               { "right", KEY_Right },
   { "up", KEY_Up },                   { "down", KEY_Down },
   { "enter", KEY_Enter },             { "kp_enter", KP_Enter },
   { "space", KEY_Space },             { "tab", KEY_Tab },
   { "shift", KEY_LeftShift },         { "rshift", KEY_RightShift },
   { "ctrl", KEY_LeftControl },        { "alt", KEY_LeftAlt },
   { "escape", KEY_Escape },           { "backspace", KEY_DeleteForward },
   { "backquote", KEY_Grave },         { "pause", KEY_Pause },

   { "f1", KEY_F1 },                   { "f2", KEY_F2 },
   { "f3", KEY_F3 },                   { "f4", KEY_F4 },
   { "f5", KEY_F5 },                   { "f6", KEY_F6 },
   { "f7", KEY_F7 },                   { "f8", KEY_F8 },
   { "f9", KEY_F9 },                   { "f10", KEY_F10 },
   { "f11", KEY_F11 },                 { "f12", KEY_F12 },

   { "num0", KEY_0 },                  { "num1", KEY_1 },
   { "num2", KEY_2 },                  { "num3", KEY_3 },
   { "num4", KEY_4 },                  { "num5", KEY_5 },
   { "num6", KEY_6 },                  { "num7", KEY_7 },
   { "num8", KEY_8 },                  { "num9", KEY_9 },
   
   { "insert", KEY_Insert },           { "del", KEY_DeleteForward },
   { "home", KEY_Home },               { "end", KEY_End },
   { "pageup", KEY_PageUp },           { "pagedown", KEY_PageDown },
   
   { "add", KP_Add },                  { "subtract", KP_Subtract },
   { "multiply", KP_Multiply },        { "divide", KP_Divide },
   { "keypad0", KP_0 },                { "keypad1", KP_1 },
   { "keypad2", KP_2 },                { "keypad3", KP_3 },
   { "keypad4", KP_4 },                { "keypad5", KP_5 },
   { "keypad6", KP_6 },                { "keypad7", KP_7 },
   { "keypad8", KP_8 },                { "keypad9", KP_9 },
   
   { "period", KEY_Period },           { "capslock", KEY_CapsLock },
   { "numlock", KP_NumLock },          { "print", KEY_PrintScreen },
   { "scroll_lock", KEY_ScrollLock },
   
   { "a", KEY_A }, { "b", KEY_B }, { "c", KEY_C }, { "d", KEY_D },
   { "e", KEY_E }, { "f", KEY_F }, { "g", KEY_G }, { "h", KEY_H },
   { "i", KEY_I }, { "j", KEY_J }, { "k", KEY_K }, { "l", KEY_L },
   { "m", KEY_M }, { "n", KEY_N }, { "o", KEY_O }, { "p", KEY_P },
   { "q", KEY_Q }, { "r", KEY_R }, { "s", KEY_S }, { "t", KEY_T },
   { "u", KEY_U }, { "v", KEY_V }, { "w", KEY_W }, { "x", KEY_X },
   { "y", KEY_Y }, { "z", KEY_Z },

   { "nul", 0x00},
};

@implementation RAButtonGetter
{
   RAButtonGetter* _me;
   RASettingData* _value;
   UIAlertView* _alert;
   UITableView* _view;
   bool _finished;
   NSTimer* _btTimer;
}

- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table
{
   self = [super init];

   _value = setting;
   _view = table;
   _me = self;

   _alert = [[UIAlertView alloc] initWithTitle:@"RetroArch"
                                 message:_value.label
                                 delegate:self
                                 cancelButtonTitle:@"Cancel"
                                 otherButtonTitles:nil];
   [_alert show];
   
   _btTimer = [NSTimer scheduledTimerWithTimeInterval:.05f target:self selector:@selector(checkInput) userInfo:nil repeats:YES];
   return self;
}

- (void)finish
{
   if (!_finished)
   {
      _finished = true;
   
      [_btTimer invalidate];

      [_alert dismissWithClickedButtonIndex:0 animated:YES];
      [_view reloadData];
   
      _me = nil;
   }
}

- (void)alertView:(UIAlertView*)alertView willDismissWithButtonIndex:(NSInteger)buttonIndex
{
   [self finish];
}

- (void)checkInput
{
   // Keyboard
   for (int i = 0; ios_key_name_map[i].hid_id; i ++)
   {
      if (ios_key_list[ios_key_name_map[i].hid_id])
      {
         _value.msubValues[0] = [NSString stringWithUTF8String:ios_key_name_map[i].keyname];
         [self finish];
         return;
      }
   }

   // WiiMote
   for (int i = 0; i != myosd_num_of_joys; i ++)
   {
      uint32_t buttons = joys[i].btns;
      buttons |= (joys[i].exp.type == EXP_CLASSIC) ? (joys[i].exp.classic.btns << 16) : 0;

      for (int j = 0; j != sizeof(buttons) * 8; j ++)
      {
         if (buttons & (1 << j))
         {
            _value.msubValues[1] = [NSString stringWithFormat:@"%d", j];
            [self finish];
            return;
         }
      }
   }
}

@end

