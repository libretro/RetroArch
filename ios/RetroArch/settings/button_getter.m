//
//  settings_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "settings.h"

static const struct
{
   const char* const keyname;
   const uint32_t hid_id;
} ios_key_name_map[] = {
   { "left", 0x50 },          { "right", 0x4F },
   { "up", 0x52 },            { "down", 0x51 },
   { "enter", 0x28 },         { "kp_enter", 0x58 },
   { "tab", 0x2B },           { "insert", 0x49 },
   { "del", 0x4C },           { "end", 0x4D },
   { "home", 0x4A },          { "rshift", 0xE5 },
   { "shift", 0xE1 },         { "ctrl", 0xE0 },
   { "alt", 0xE2 },           { "space", 0x2C },
   { "escape", 0x29 },        { "backspace", 0x2A },
   { "backquote", 0x35 },     { "pause", 0x48 },

   
   { "add", 0x57 },           { "subtract", 0x56 }, /*kp_minus?*/
   { "multiply", 0x55 },      { "divide", 0x54 },
   { "kp_plus", 0x57 },       { "kp_minus", 0x56 },

   { "f1", 0x3A },            { "f2", 0x3B },
   { "f3", 0x3C },            { "f4", 0x3D },
   { "f5", 0x3E },            { "f6", 0x3F },
   { "f7", 0x40 },            { "f8", 0x41 },
   { "f9", 0x42 },            { "f10", 0x43 },
   { "f11", 0x44 },           { "f12", 0x45 },

   { "num0", 0x27 },          { "num1", 0x1E },
   { "num2", 0x1F },          { "num3", 0x20 },
   { "num4", 0x21 },          { "num5", 0x22 },
   { "num6", 0x23 },          { "num7", 0x24 },
   { "num8", 0x25 },          { "num9", 0x26 },
   
   { "pageup", 0x48 },        { "pagedown", 0x4E },
   { "keypad0", 0x62 },       { "keypad1", 0x59 },
   { "keypad2", 0x5A },       { "keypad3", 0x5B },
   { "keypad4", 0x5C },       { "keypad5", 0x5D },
   { "keypad6", 0x5E },       { "keypad7", 0x5F },
   { "keypad8", 0x60 },       { "keypad9", 0x61 },
   
   /*{ "period", RETROK_PERIOD },
   { "capslock", RETROK_CAPSLOCK },    { "numlock", RETROK_NUMLOCK },
   { "print_screen", RETROK_PRINT },
   { "scroll_lock", RETROK_SCROLLOCK },*/

   { "a", 0x04 }, { "b", 0x05 }, { "c", 0x06 }, { "d", 0x07 },
   { "e", 0x08 }, { "f", 0x09 }, { "g", 0x0A }, { "h", 0x0B },
   { "i", 0x0C }, { "j", 0x0D }, { "k", 0x0E }, { "l", 0x0F },
   { "m", 0x10 }, { "n", 0x11 }, { "o", 0x12 }, { "p", 0x13 },
   { "q", 0x14 }, { "r", 0x15 }, { "s", 0x16 }, { "t", 0x17 },
   { "u", 0x18 }, { "v", 0x19 }, { "w", 0x1A }, { "x", 0x1B },
   { "y", 0x1C }, { "z", 0x1D },

   { "nul", 0x00},
};

static const NSString* get_key_config_name(uint32_t hid_id)
{
   for (int i = 0; ios_key_name_map[i].hid_id; i ++)
   {
      if (hid_id == ios_key_name_map[i].hid_id)
      {
         return [NSString stringWithUTF8String:ios_key_name_map[i].keyname];
      }
   }
   
   return @"nul";
}

@implementation button_getter
{
   button_getter* me;
   NSMutableDictionary* value;
   UIAlertView* alert;
   UITableView* view;
}

- (id)initWithSetting:(NSMutableDictionary*)setting fromTable:(UITableView*)table
{
   self = [super init];

   value = setting;
   view = table;
   me = self;

   alert = [[UIAlertView alloc] initWithTitle:@"RetroArch"
                                message:[value objectForKey:@"LABEL"]
                                delegate:self
                                cancelButtonTitle:@"Cancel"
                                otherButtonTitles:nil];
   [alert show];
   
   [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(keyReleased:) name: GSEventKeyUpNotification object: nil];

   return self;
}

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
   [[NSNotificationCenter defaultCenter] removeObserver:self];
   me = nil;
}

- (void)keyReleased:(NSNotification*) notification
{
   int keycode = [[notification.userInfo objectForKey:@"keycode"] intValue];

   [value setObject:get_key_config_name(keycode) forKey:@"VALUE"];

   [alert dismissWithClickedButtonIndex:0 animated:YES];
   [view reloadData];
}

@end

