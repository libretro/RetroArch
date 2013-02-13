//
//  settings_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import <objc/runtime.h>
#import "settings.h"
#include "config_file.h"

@implementation RASettingData
@end


static NSString* get_value_from_config(config_file_t* config, NSString* name, NSString* defaultValue)
{
   NSString* value = nil;

   char* v = 0;
   if (config && config_get_string(config, [name UTF8String], &v))
   {
      value = [[NSString alloc] initWithUTF8String:v];
      free(v);
   }
   else
   {
      value = defaultValue;
   }

   return value;
}

static RASettingData* boolean_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue)
{
   RASettingData* result = [[RASettingData alloc] init];
   result.type = BooleanSetting;
   result.label = label;
   result.name = name;
   result.value = get_value_from_config(config, name, defaultValue);
   return result;
}

static RASettingData* button_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue)
{
   RASettingData* result = [[RASettingData alloc] init];
   result.type = ButtonSetting;
   result.label = label;
   result.name = name;
   result.value = get_value_from_config(config, name, defaultValue);
   return result;
}

static RASettingData* group_setting(NSString* label, NSArray* settings)
{
   RASettingData* result = [[RASettingData alloc] init];
   result.type = GroupSetting;
   result.label = label;
   result.subValues = settings;
   return result;
}

static RASettingData* enumeration_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue, NSArray* values)
{
   RASettingData* result = [[RASettingData alloc] init];
   result.type = EnumerationSetting;
   result.label = label;
   result.name = name;
   result.value = get_value_from_config(config, name, defaultValue);
   result.subValues = values;
   return result;
}

static RASettingData* subpath_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue, NSString* path, NSString* extension)
{
   NSString* value = get_value_from_config(config, name, defaultValue);
   value = [value stringByReplacingOccurrencesOfString:path withString:@""];

   NSArray* values = [[NSFileManager defaultManager] subpathsOfDirectoryAtPath:path error:nil];
   values = [values pathsMatchingExtensions:[NSArray arrayWithObject:extension]];

   RASettingData* result = [[RASettingData alloc] init];
   result.type = FileListSetting;
   result.label = label;
   result.name = name;
   result.value = value;
   result.subValues = values;
   result.path = path;
   return result;
}

@implementation RASettingsList
- (id)init
{
   config_file_t* config = config_file_new([[RetroArch_iOS get].config_file_path UTF8String]);

   NSString* overlay_path = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/overlays/"];
   NSString* shader_path = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/shaders/"];

   NSArray* settings = [NSArray arrayWithObjects:
      [NSArray arrayWithObjects:@"Video",
         boolean_setting(config, @"video_smooth", @"Smooth Video", @"true"),
         boolean_setting(config, @"video_crop_overscan", @"Crop Overscan", @"false"),
         subpath_setting(config, @"video_bsnes_shader", @"Shader", @"", shader_path, @"shader"),
         nil],

      [NSArray arrayWithObjects:@"Audio",
         boolean_setting(config, @"audio_enable", @"Enable Output", @"true"),
         boolean_setting(config, @"audio_sync", @"Sync on Audio Stream", @"true"),
         boolean_setting(config, @"audio_rate_control", @"Adjust for Better Sync", @"true"),
         nil],

      [NSArray arrayWithObjects:@"Input",
         subpath_setting(config, @"input_overlay", @"Input Overlay", @"", overlay_path, @"cfg"),
         group_setting(@"Player 1 Keys", [NSArray arrayWithObjects:
            [NSArray arrayWithObjects:@"Player 1",
               button_setting(config, @"input_player1_up", @"Up", @"up"),
               button_setting(config, @"input_player1_down", @"Down", @"down"),
               button_setting(config, @"input_player1_left", @"Left", @"left"),
               button_setting(config, @"input_player1_right", @"Right", @"right"),

               button_setting(config, @"input_player1_start", @"Start", @"enter"),
               button_setting(config, @"input_player1_select", @"Select", @"rshift"),

               button_setting(config, @"input_player1_b", @"B", @"z"),
               button_setting(config, @"input_player1_a", @"A", @"x"),
               button_setting(config, @"input_player1_x", @"X", @"s"),
               button_setting(config, @"input_player1_y", @"Y", @"a"),

               button_setting(config, @"input_player1_l", @"L", @"q"),
               button_setting(config, @"input_player1_r", @"R", @"w"),
               button_setting(config, @"input_player1_l2", @"L2", @""),
               button_setting(config, @"input_player1_r2", @"R2", @""),
               button_setting(config, @"input_player1_l3", @"L3", @""),
               button_setting(config, @"input_player1_r3", @"R3", @""),
               nil],
            nil]),
         group_setting(@"System Keys", [NSArray arrayWithObjects:
            [NSArray arrayWithObjects:@"System Keys",
               button_setting(config, @"input_save_state", @"Save State", @"f2"),
               button_setting(config, @"input_load_state", @"Load State", @"f4"),
               button_setting(config, @"input_state_slot_increase", @"Next State Slot", @"f7"),
               button_setting(config, @"input_state_slot_decrease", @"Previous State Slot", @"f6"),
               button_setting(config, @"input_toggle_fast_forward", @"Toggle Fast Forward", @"space"),
               button_setting(config, @"input_hold_fast_forward", @"Hold Fast Forward", @"l"),
               button_setting(config, @"input_rewind", @"Rewind", @"r"),
               button_setting(config, @"input_slowmotion", @"Slow Motion", @"e"),
               button_setting(config, @"input_reset", @"Reset", @"h"),
               button_setting(config, @"input_exit_emulator", @"Close Game", @"escape"),
               nil],
            nil]),
         nil],
      
        
      [NSArray arrayWithObjects:@"Save States",
         boolean_setting(config, @"rewind_enable", @"Enable Rewinding", @"false"),
         boolean_setting(config, @"block_sram_overwrite", @"Disable SRAM on Load", @"false"),
         boolean_setting(config, @"savestate_auto_save", @"Auto Save on Exit", @"false"),
         boolean_setting(config, @"savestate_auto_load", @"Auto Load on Startup", @"true"),
         nil],
      nil
   ];
   
   if (config)
      config_file_free(config);

   self = [super initWithSettings:settings title:@"RetroArch Settings"];
   return self;
}

- (void)dealloc
{
   [self writeToDisk];
}

+ (void)refreshConfigFile
{
   [[[RASettingsList alloc] init] writeToDisk];
}

- (void)writeToDisk
{
   config_file_t* config = config_file_new([[RetroArch_iOS get].config_file_path UTF8String]);
   config = config ? config : config_file_new(0);

   config_set_string(config, "system_directory", [[RetroArch_iOS get].system_directory UTF8String]);

   [self writeSettings:nil toConfig:config];

   config_file_write(config, [[RetroArch_iOS get].config_file_path UTF8String]);
   config_file_free(config);
}

@end
