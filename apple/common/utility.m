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

#include <sys/stat.h>

#include "RetroArch_Apple.h"
#include "setting_data.h"

#include "general.h"
#include "file.h"

void apple_display_alert(NSString* message, NSString* title)
{
#ifdef IOS
   UIAlertView* alert = [[UIAlertView alloc] initWithTitle:title ? title : @"RetroArch"
                                             message:message
                                             delegate:nil
                                             cancelButtonTitle:@"OK"
                                             otherButtonTitles:nil];
   [alert show];
#else
   NSAlert* alert = [NSAlert new];
   alert.messageText = title ? title : @"RetroArch";
   alert.informativeText = message;
   alert.alertStyle = NSInformationalAlertStyle;
   [alert beginSheetModalForWindow:RetroArch_OSX.get->window
          modalDelegate:apple_platform
          didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
          contextInfo:nil];
   [[NSApplication sharedApplication] runModalForWindow:alert.window];
#endif
}

// Fetch a value from a config file, returning defaultValue if the value is not present
NSString* objc_get_value_from_config(config_file_t* config, NSString* name, NSString* defaultValue)
{
   char* data = 0;
   if (config)
      config_get_string(config, [name UTF8String], &data);
   
   NSString* result = data ? @(data) : defaultValue;
   free(data);
   return result;
}

// Get a core ID as an NSString
NSString *apple_get_core_id(const core_info_t *core)
{
   char buf[PATH_MAX];
   return @(apple_core_info_get_id(core, buf, sizeof(buf)));
}

NSString *apple_get_core_display_name(NSString *core_id)
{
   const core_info_t *core = apple_core_info_list_get_by_id(core_id.UTF8String);
   return core ? @(core->display_name) : core_id;
}

// Number formatter class for setting strings
@implementation RANumberFormatter
- (id)initWithSetting:(const rarch_setting_t*)setting
{
   if ((self = [super init]))
   {
      self.allowsFloats = (setting->type == ST_FLOAT);
      
      if (setting->min != setting->max)
      {
         self.minimum = @(setting->min);
         self.maximum = @(setting->max);
      }
      else
      {
         if (setting->type == ST_INT)
         {
            self.minimum = @(INT_MIN);
            self.maximum = @(INT_MAX);
         }
         else if (setting->type == ST_UINT)
         {
            self.minimum = @(0);
            self.maximum = @(UINT_MAX);
         }
         else if (setting->type == ST_FLOAT)
         {
            self.minimum = @(FLT_MIN);
            self.maximum = @(FLT_MAX);
         }
      }
   }
   
   return self;
}

- (BOOL)isPartialStringValid:(NSString*)partialString newEditingString:(NSString**)newString errorDescription:(NSString**)error
{
   bool hasDot = false;

   if (partialString.length)
      for (int i = 0; i != partialString.length; i ++)
      {
         unichar ch = [partialString characterAtIndex:i];
         
         if (i == 0 && (!self.minimum || self.minimum.intValue < 0) && ch == '-')
            continue;
         else if (self.allowsFloats && !hasDot && ch == '.')
            hasDot = true;
         else if (!isdigit(ch))
            return NO;
      }

   return YES;
}

#ifdef IOS
- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
   NSString* text = [textField.text stringByReplacingCharactersInRange:range withString:string];
   return [self isPartialStringValid:text newEditingString:nil errorDescription:nil];
}
#endif

@end
