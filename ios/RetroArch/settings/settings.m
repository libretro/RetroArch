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

#include "../input/ios_input.h"
#include "../input/keycode.h"
#include "../input/BTStack/btpad.h"

@implementation RASettingData
- (id)initWithType:(enum SettingTypes)aType label:(NSString*)aLabel name:(NSString*)aName
{
   self.type = aType;
   self.label = aLabel;
   self.name = aName;
   return self;
}
@end

static RASettingData* boolean_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue)
{
   RASettingData* result = [[RASettingData alloc] initWithType:BooleanSetting label:label name:name];
   result.value = ios_get_value_from_config(config, name, defaultValue);
   return result;
}

static RASettingData* button_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue)
{
   RASettingData* result = [[RASettingData alloc] initWithType:ButtonSetting label:label name:name];
   result.msubValues = [NSMutableArray arrayWithObjects:
                        ios_get_value_from_config(config, name, defaultValue),
                        ios_get_value_from_config(config, [name stringByAppendingString:@"_btn"], @""),
                        ios_get_value_from_config(config, [name stringByAppendingString:@"_axis"], @""),
                        nil];
   return result;
}

static RASettingData* group_setting(NSString* label, NSArray* settings)
{
   RASettingData* result = [[RASettingData alloc] initWithType:GroupSetting label:label name:nil];
   result.subValues = settings;
   return result;
}

static RASettingData* enumeration_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue, NSArray* values)
{
   RASettingData* result = [[RASettingData alloc] initWithType:EnumerationSetting label:label name:name];
   result.value = ios_get_value_from_config(config, name, defaultValue);
   result.subValues = values;
   return result;
}

static RASettingData* subpath_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue, NSString* path, NSString* extension)
{
   NSString* value = ios_get_value_from_config(config, name, defaultValue);
   value = [value stringByReplacingOccurrencesOfString:path withString:@""];

   NSArray* values = [[NSFileManager defaultManager] subpathsOfDirectoryAtPath:path error:nil];
   values = [values pathsMatchingExtensions:[NSArray arrayWithObject:extension]];

   RASettingData* result = [[RASettingData alloc] initWithType:FileListSetting label:label name:name];
   result.value = value;
   result.subValues = values;
   result.path = path;
   result.haveNoneOption = true;
   return result;
}

static RASettingData* range_setting(config_file_t* config, NSString* name, NSString* label, NSString* defaultValue, double minValue, double maxValue)
{
   RASettingData* result = [[RASettingData alloc] initWithType:RangeSetting label:label name:name];
   result.value = ios_get_value_from_config(config, name, defaultValue);
   result.rangeMin = minValue;
   result.rangeMax = maxValue;
   return result;
}

static RASettingData* aspect_setting(config_file_t* config, NSString* label)
{
   // Why does this need to be so difficult?

   RASettingData* result = [[RASettingData alloc] initWithType:AspectSetting label:label name:@"fram"];
   result.subValues = [NSArray arrayWithObjects:@"Fill Screen", @"Game Aspect", @"Pixel Aspect", @"4:3", @"16:9", nil];

   bool videoForceAspect = true;
   bool videoAspectAuto = false;
   double videoAspect = -1.0;

   if (config)
   {
      config_get_bool(config, "video_force_aspect", &videoForceAspect);
      config_get_bool(config, "video_aspect_auto", &videoAspectAuto);
      config_get_double(config, "video_aspect_ratio", &videoAspect);
   }
   
   if (!videoForceAspect)
      result.value = @"Fill Screen";
   else if (videoAspect < 0.0)
      result.value = videoAspectAuto ? @"Game Aspect" : @"Pixel Aspect";
   else
      result.value = (videoAspect < 1.5) ? @"4:3" : @"16:9";
   
   return result;
}

static RASettingData* custom_action(NSString* action, id data)
{
   RASettingData* result = [[RASettingData alloc] initWithType:CustomAction label:action name:nil];
   
   if (data != nil)
      objc_setAssociatedObject(result, "USERDATA", data, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   
   return result;
}

@implementation RASettingsList
{
   RAModuleInfo* _module;
}

+ (void)refreshModuleConfig:(RAModuleInfo*)module;
{
   (void)[[RASettingsList alloc] initWithModule:module];
}

- (id)initWithModule:(RAModuleInfo*)module
{
   _module = module;

   config_file_t* config = config_file_new([_module.configPath UTF8String]);

   NSString* overlay_path = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/overlays/"];
   NSString* shader_path = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/shaders_glsl/"];

   NSArray* settings = [NSArray arrayWithObjects:
      [NSArray arrayWithObjects:@"Core",
         custom_action(@"Core Info", nil),
         nil],

      [NSArray arrayWithObjects:@"Video",
         boolean_setting(config, @"video_smooth", @"Bilinear filtering", @"true"),
         boolean_setting(config, @"video_crop_overscan", @"Crop Overscan", @"true"),
         boolean_setting(config, @"video_scale_integer", @"Integer Scaling", @"false"),
         aspect_setting(config, @"Aspect Ratio"),
         nil],
         
      [NSArray arrayWithObjects:@"GPU Shader",         
         boolean_setting(config, @"video_shader_enable", @"Enable Shader", @"false"),
         subpath_setting(config, @"video_shader", @"Shader", @"", shader_path, @"glsl"),
         nil],

      [NSArray arrayWithObjects:@"Audio",
         boolean_setting(config, @"audio_enable", @"Enable Output", @"true"),
         boolean_setting(config, @"audio_sync", @"Sync on Audio", @"true"),
         boolean_setting(config, @"audio_rate_control", @"Rate Control", @"true"),
         nil],

      [NSArray arrayWithObjects:@"Input",
         subpath_setting(config, @"input_overlay", @"Input Overlay", @"", overlay_path, @"cfg"),
         range_setting(config, @"input_overlay_opacity", @"Overlay Opacity", @"1.0", 0.0, 1.0),
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

               button_setting(config, @"input_player1_l_y_minus", @"Left Stick Up", @""),
               button_setting(config, @"input_player1_l_y_plus", @"Left Stick Down", @""),
               button_setting(config, @"input_player1_l_x_minus", @"Left Stick Left", @""),
               button_setting(config, @"input_player1_l_x_plus", @"Left Stick Right", @""),
               button_setting(config, @"input_player1_r_y_minus", @"Right Stick Up", @""),
               button_setting(config, @"input_player1_r_y_plus", @"Right Stick Down", @""),
               button_setting(config, @"input_player1_r_x_minus", @"Right Stick Left", @""),
               button_setting(config, @"input_player1_r_x_plus", @"Right Stick Right", @""),
               nil],
            nil]),
         group_setting(@"System Keys", [NSArray arrayWithObjects:
            // TODO: Many of these strings will be cut off on an iPhone
            [NSArray arrayWithObjects:@"System Keys",
               button_setting(config, @"input_menu_toggle", @"Show RGUI", @"F1"),
               button_setting(config, @"input_disk_eject_toggle", @"Insert/Eject Disk", @""),
               button_setting(config, @"input_disk_next", @"Cycle Disks", @""),
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
               button_setting(config, @"input_enable_hotkey", @"Hotkey Enable (Always on if not set)", @""),             
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

   self = [super initWithSettings:settings title:_module.displayName];
   return self;
}

- (void)dealloc
{
   config_file_t* config = config_file_new([_module.configPath UTF8String]);
    
    if (!config)
        config = config_file_new(0);
   
   config_set_string(config, "system_directory", [[RetroArch_iOS get].systemDirectory UTF8String]);
   [self writeSettings:nil toConfig:config];
   if (config)
      config_file_write(config, [_module.configPath UTF8String]);
   config_file_free(config);

   [[RetroArch_iOS get] refreshConfig];
}

- (void)handleCustomAction:(RASettingData*)setting
{
   if ([@"Core Info" isEqualToString:setting.label])
      [[RetroArch_iOS get] pushViewController:[[RAModuleInfoList alloc] initWithModuleInfo:_module] animated:YES];
}

@end

@implementation RASystemSettingsList
- (id)init
{
   config_file_t* config = config_file_new([[RetroArch_iOS get].systemConfigPath UTF8String]);

   NSMutableArray* modules = [NSMutableArray array];
   [modules addObject:@"Cores"];

   NSArray* module_data = [RAModuleInfo getModules];
   for (int i = 0; i != module_data.count; i ++)
   {
      RAModuleInfo* info = (RAModuleInfo*)module_data[i];
      [modules addObject:custom_action(info.displayName, info)];
   }

   NSArray* settings = [NSArray arrayWithObjects:
      [NSArray arrayWithObjects:@"Frontend",
         custom_action(@"Diagnostic Log", nil),
         nil],
      [NSArray arrayWithObjects:@"Bluetooth",
         // TODO: Note that with this turned off the native bluetooth is expected to be a real keyboard
         boolean_setting(config, @"ios_use_icade", @"Native BT is iCade", @"false"),
         // TODO: Make this option only if BTstack is available
         boolean_setting(config, @"ios_use_btstack", @"Enable BTstack", @"false"),
         nil],
      [NSArray arrayWithObjects:@"Orientations",
         boolean_setting(config, @"ios_allow_portrait", @"Portrait", @"true"),
         boolean_setting(config, @"ios_allow_portrait_upside_down", @"Portrait Upside Down", @"true"),
         boolean_setting(config, @"ios_allow_landscape_left", @"Landscape Left", @"true"),
         boolean_setting(config, @"ios_allow_landscape_right", @"Landscape Right", @"true"),
         nil],
      modules,
      nil
   ];

   self = [super initWithSettings:settings title:@"RetroArch Settings"];
   return self;
}

- (void)dealloc
{
   config_file_t* config = config_file_new([[RetroArch_iOS get].systemConfigPath UTF8String]);
   
    if (!config)
        config = config_file_new(0);
   
   [self writeSettings:nil toConfig:config];
   
   if (config)
      config_file_write(config, [[RetroArch_iOS get].systemConfigPath UTF8String]);
   config_file_free(config);
   
   [[RetroArch_iOS get] refreshSystemConfig];
}

- (void)handleCustomAction:(RASettingData*)setting
{
   if ([@"Diagnostic Log" isEqualToString:setting.label])
      [[RetroArch_iOS get] pushViewController:[RALogView new] animated:YES];
   else if ([@"Enable BTstack" isEqualToString:setting.label])
   {
      if ([@"true" isEqualToString:setting.value])
         [RetroArch_iOS.get startBluetooth];
      else
         [RetroArch_iOS.get stopBluetooth];
   }
   else
   {
      id data = objc_getAssociatedObject(setting, "USERDATA");
      if (data)
         [RetroArch_iOS.get pushViewController:[[RASettingsList alloc] initWithModule:(RAModuleInfo*)data] animated:YES];
   }
}

@end

@implementation RASettingsSubList
{
   NSArray* settings;
};

- (id)initWithSettings:(NSArray*)values title:(NSString*)title
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   settings = values;
  
   [self setTitle:title];
   return self;
}

- (bool)isSettingsView
{
   return true;
}

- (void)handleCustomAction:(RASettingData*)setting
{

}

- (void)writeSettings:(NSArray*)settingList toConfig:(config_file_t*)config
{
   if (!config)
      return;

   NSArray* list = settingList ? settingList : settings;

   for (int i = 0; i < [list count]; i++)
   {
      NSArray* group = [list objectAtIndex:i];
   
      for (int j = 1; j < [group count]; j ++)
      {
         RASettingData* setting = [group objectAtIndex:j];
         
         switch (setting.type)
         {         
            case GroupSetting:
               [self writeSettings:setting.subValues toConfig:config];
               break;
               
            case FileListSetting:
               if ([setting.value length] > 0)
                  config_set_string(config, [setting.name UTF8String], [[setting.path stringByAppendingPathComponent:setting.value] UTF8String]);
               else
                  config_set_string(config, [setting.name UTF8String], "");
               break;

            case ButtonSetting:
               if (setting.msubValues[0])
                  config_set_string(config, [setting.name UTF8String], [setting.msubValues[0] UTF8String]);
               if (setting.msubValues[1])
                  config_set_string(config, [[setting.name stringByAppendingString:@"_btn"] UTF8String], [setting.msubValues[1] UTF8String]);
               if (setting.msubValues[2])
                  config_set_string(config, [[setting.name stringByAppendingString:@"_axis"] UTF8String], [setting.msubValues[2] UTF8String]);
               break;

            case AspectSetting:
               config_set_string(config, "video_force_aspect", [@"Fill Screen" isEqualToString:setting.value] ? "false" : "true");
               config_set_string(config, "video_aspect_ratio_auto", [@"Game Aspect" isEqualToString:setting.value] ? "true" : "false");
               config_set_string(config, "video_aspect_ratio", "-1.0");
               if([@"4:3" isEqualToString:setting.value])
                  config_set_string(config, "video_aspect_ratio", "1.33333333");
               else if([@"16:9" isEqualToString:setting.value])
                  config_set_string(config, "video_aspect_ratio", "1.77777777");
               break;

            case CustomAction:
               break;

            default:
               config_set_string(config, [setting.name UTF8String], [setting.value UTF8String]);
               break;
         }
      }
   }
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   RASettingData* setting = [[settings objectAtIndex:indexPath.section] objectAtIndex:indexPath.row + 1];
   
   switch (setting.type)
   {
      case EnumerationSetting:
      case FileListSetting:
      case AspectSetting:
         [[RetroArch_iOS get] pushViewController:[[RASettingEnumerationList alloc] initWithSetting:setting fromTable:(UITableView*)self.view] animated:YES];
         break;
         
      case ButtonSetting:
         (void)[[RAButtonGetter alloc] initWithSetting:setting fromTable:(UITableView*)self.view];
         break;
         
      case GroupSetting:
         [[RetroArch_iOS get] pushViewController:[[RASettingsSubList alloc] initWithSettings:setting.subValues title:setting.label] animated:YES];
         break;
         
      default:
         break;
   }
   
   [self handleCustomAction:setting];
}

- (void)handleBooleanSwitch:(UISwitch*)swt
{
   RASettingData* setting = objc_getAssociatedObject(swt, "SETTING");
   setting.value = (swt.on ? @"true" : @"false");
   
   [self handleCustomAction:setting];
}

- (void)handleSlider:(UISlider*)sld
{
   RASettingData* setting = objc_getAssociatedObject(sld, "SETTING");
   setting.value = [NSString stringWithFormat:@"%f", sld.value];

   [self handleCustomAction:setting];
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   RASettingData* setting = [[settings objectAtIndex:indexPath.section] objectAtIndex:indexPath.row + 1];
  
   UITableViewCell* cell = nil;

   switch (setting.type)
   {
      case BooleanSetting:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"boolean"];

         if (cell == nil)
         {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"boolean"];
         
            UISwitch* accessory = [[UISwitch alloc] init];
            [accessory addTarget:self action:@selector(handleBooleanSwitch:) forControlEvents:UIControlEventValueChanged];
            cell.accessoryView = accessory;
            
            [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
         }
      
         UISwitch* swt = (UISwitch*)cell.accessoryView;
         swt.on = [setting.value isEqualToString:@"true"];
         objc_setAssociatedObject(swt, "SETTING", setting, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      }
      break;

      case RangeSetting:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"range"];
         
         if (cell == nil)
         {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"range"];

            UISlider* accessory = [UISlider new];
            [accessory addTarget:self action:@selector(handleSlider:) forControlEvents:UIControlEventValueChanged];
            accessory.continuous = NO;
            cell.accessoryView = accessory;

            [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
         }
         
         UISlider* sld = (UISlider*)cell.accessoryView;
         sld.minimumValue = setting.rangeMin;
         sld.maximumValue = setting.rangeMax;
         sld.value = [setting.value doubleValue];
         objc_setAssociatedObject(sld, "SETTING", setting, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      }
      break;
         
      case EnumerationSetting:
      case FileListSetting:
      case ButtonSetting:
      case CustomAction:
      case AspectSetting:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"enumeration"];
         cell = cell ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"enumeration"];
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      }
      break;

      case GroupSetting:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"group"];
   
         if (cell == nil)
         {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"group"];
            cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
         }
      }
      break;
   }

   cell.textLabel.text = setting.label;
   
   if (setting.type != ButtonSetting)
      cell.detailTextLabel.text = setting.value;
   else
      cell.detailTextLabel.text = [NSString stringWithFormat:@"[KB:%@] [JS:%@] [AX:%@]",
            [setting.msubValues[0] length] ? setting.msubValues[0] : @"N/A",
            [setting.msubValues[1] length] ? setting.msubValues[1] : @"N/A",
            [setting.msubValues[2] length] ? setting.msubValues[2] : @"N/A"];

   return cell;
}

// UITableView item counts
- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return [settings count];
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
   return [[settings objectAtIndex:section] count] -1;
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
   return [[settings objectAtIndex:section] objectAtIndex:0];
}

@end

@implementation RASettingEnumerationList
{
   RASettingData* _value;
   UITableView* _view;
   unsigned _mainSection;
};

- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   
   _value = setting;
   _view = table;
   _mainSection = _value.haveNoneOption ? 1 : 0;
   
   [self setTitle: _value.label];
   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return _value.haveNoneOption ? 2 : 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
   return (section == _mainSection) ? _value.subValues.count : 1;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"option"];
   cell = cell ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"option"];

   cell.textLabel.text = (indexPath.section == _mainSection) ? _value.subValues[indexPath.row] : @"None";

   return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   _value.value = (indexPath.section == _mainSection) ? _value.subValues[indexPath.row] : @"";
   
   [_view reloadData];
   [[RetroArch_iOS get] popViewControllerAnimated:YES];
}

@end

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
                                 otherButtonTitles:@"Clear Keyboard", @"Clear Joystick", @"Clear Axis", nil];
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

      [_alert dismissWithClickedButtonIndex:_alert.cancelButtonIndex animated:YES];
      [_view reloadData];
   
      _me = nil;
   }
}

- (void)alertView:(UIAlertView*)alertView willDismissWithButtonIndex:(NSInteger)buttonIndex
{
   if (buttonIndex == _alert.firstOtherButtonIndex)
      _value.msubValues[0] = @"";
   else if(buttonIndex == _alert.firstOtherButtonIndex + 1)
      _value.msubValues[1] = @"";
   else if(buttonIndex == _alert.firstOtherButtonIndex + 2)
      _value.msubValues[2] = @"";

   [self finish];
}

- (void)checkInput
{
   ios_input_data_t data;
   ios_copy_input(&data);

   // Keyboard
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
      { "numlock", KP_NumLock },          { "print_screen", KEY_PrintScreen },
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
   
   
   for (int i = 0; ios_key_name_map[i].hid_id; i++)
   {
      if (data.keys[ios_key_name_map[i].hid_id])
      {
         _value.msubValues[0] = [NSString stringWithUTF8String:ios_key_name_map[i].keyname];
         [self finish];
         return;
      }
   }

   // Pad Buttons
   for (int i = 0; data.pad_buttons && i < sizeof(data.pad_buttons) * 8; i++)
   {
      if (data.pad_buttons & (1 << i))
      {
         _value.msubValues[1] = [NSString stringWithFormat:@"%d", i];
         [self finish];
         return;
      }
   }

   // Pad Axis
   for (int i = 0; i < 4; i++)
   {
      int16_t value = data.pad_axis[i];
      
      if (abs(value) > 0x1000)
      {
         _value.msubValues[2] = [NSString stringWithFormat:@"%s%d", (value > 0x1000) ? "+" : "-", i];
         [self finish];
         break;
      }
   }
}

@end

