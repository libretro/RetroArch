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

#import <objc/runtime.h>
#import "settings.h"

@implementation RASettingData
- (id)initWithType:(enum SettingTypes)aType label:(NSString*)aLabel name:(NSString*)aName
{
   self.type = aType;
   self.label = aLabel;
   self.name = aName;
   return self;
}
@end

static NSString* get_value_from_config(RAConfig* config, NSString* name, NSString* defaultValue)
{
   return [config getStringNamed:name withDefault:defaultValue];
}

static RASettingData* boolean_setting(RAConfig* config, NSString* name, NSString* label, NSString* defaultValue)
{
   RASettingData* result = [[RASettingData alloc] initWithType:BooleanSetting label:label name:name];
   result.value = get_value_from_config(config, name, defaultValue);
   return result;
}

static RASettingData* button_setting(RAConfig* config, NSString* name, NSString* label, NSString* defaultValue)
{
   RASettingData* result = [[RASettingData alloc] initWithType:ButtonSetting label:label name:name];
   result.msubValues = [NSMutableArray arrayWithObjects:
                        get_value_from_config(config, name, defaultValue),
                        get_value_from_config(config, [name stringByAppendingString:@"_btn"], @""),
                        nil];
   return result;
}

static RASettingData* group_setting(NSString* label, NSArray* settings)
{
   RASettingData* result = [[RASettingData alloc] initWithType:GroupSetting label:label name:nil];
   result.subValues = settings;
   return result;
}

static RASettingData* enumeration_setting(RAConfig* config, NSString* name, NSString* label, NSString* defaultValue, NSArray* values)
{
   RASettingData* result = [[RASettingData alloc] initWithType:EnumerationSetting label:label name:name];
   result.value = get_value_from_config(config, name, defaultValue);
   result.subValues = values;
   return result;
}

static RASettingData* subpath_setting(RAConfig* config, NSString* name, NSString* label, NSString* defaultValue, NSString* path, NSString* extension)
{
   NSString* value = get_value_from_config(config, name, defaultValue);
   value = [value stringByReplacingOccurrencesOfString:path withString:@""];

   NSArray* values = [[NSFileManager defaultManager] subpathsOfDirectoryAtPath:path error:nil];
   values = [values pathsMatchingExtensions:[NSArray arrayWithObject:extension]];

   RASettingData* result = [[RASettingData alloc] initWithType:FileListSetting label:label name:name];
   result.value = value;
   result.subValues = values;
   result.path = path;
   return result;
}

static RASettingData* aspect_setting(RAConfig* config, NSString* label)
{
   // Why does this need to be so difficult?

   RASettingData* result = [[RASettingData alloc] initWithType:AspectSetting label:label name:@"fram"];
   result.subValues = [NSArray arrayWithObjects:@"Fill Screen", @"Game Aspect", @"Pixel Aspect", @"4:3", @"16:9", nil];

   bool videoForceAspect = [config getBoolNamed:@"video_force_aspect" withDefault:true];
   bool videoAspectAuto = [config getBoolNamed:@"video_aspect_ratio_auto" withDefault:false];
   double videoAspect = [config getDoubleNamed:@"video_aspect_ratio" withDefault:0.0];
   
   if (!videoForceAspect)
      result.value = @"Fill Screen";
   else if (videoAspect < 0.0)
      result.value = videoAspectAuto ? @"Game Aspect" : @"Pixel Aspect";
   else
      result.value = (videoAspect < 1.5) ? @"4:3" : @"16:9";
   
   return result;
}

static RASettingData* custom_action(NSString* action)
{
   return [[RASettingData alloc] initWithType:CustomAction label:action name:nil];
}

@implementation RASettingsList
+ (void)refreshConfigFile
{
   (void)[[RASettingsList alloc] init];
}

- (id)init
{
   RAConfig* config = [[RAConfig alloc] initWithPath:[RetroArch_iOS get].moduleInfo.configPath];

   NSString* overlay_path = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/overlays/"];
   NSString* shader_path = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/shaders/"];

   NSArray* settings = [NSArray arrayWithObjects:
      [NSArray arrayWithObjects:@"Frontend",
         custom_action(@"Module Info"),
#ifdef WIIMOTE
         boolean_setting(config, @"ios_auto_bluetooth", @"Auto Enable Bluetooth", @"false"),
#endif
         nil],

      [NSArray arrayWithObjects:@"Video",
         boolean_setting(config, @"video_smooth", @"Smooth Video", @"true"),
         boolean_setting(config, @"video_crop_overscan", @"Crop Overscan", @"true"),
         boolean_setting(config, @"video_scale_integer", @"Integer Scaling", @"false"),
         aspect_setting(config, @"Aspect Ratio"),
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

   self = [super initWithSettings:settings title:@"RetroArch Settings"];
   return self;
}

- (void)dealloc
{
   RAConfig* config = [[RAConfig alloc] initWithPath:[RetroArch_iOS get].moduleInfo.configPath];
   [config putStringNamed:@"system_directory" value:[RetroArch_iOS get].system_directory];
   [self writeSettings:nil toConfig:config];
   [config writeToFile:[RetroArch_iOS get].moduleInfo.configPath];
   [[RetroArch_iOS get] refreshConfig];
}

- (void)handleCustomAction:(NSString*)action
{
   if ([@"Module Info" isEqualToString:action])
      [[RetroArch_iOS get] pushViewController:[[RAModuleInfoList alloc] initWithModuleInfo:[RetroArch_iOS get].moduleInfo] animated:YES];
}

@end
