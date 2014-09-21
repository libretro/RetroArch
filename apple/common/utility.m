/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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
#include "../../settings_data.h"

#include "../../general.h"
#include "../../file.h"

void apple_display_alert(const char *message, const char *title)
{
#ifdef IOS
   UIAlertView* alert = [[UIAlertView alloc] initWithTitle:title ? BOXSTRING(title) : BOXSTRING("RetroArch")
                                             message:BOXSTRING(message)
                                             delegate:nil
                                             cancelButtonTitle:BOXSTRING("OK")
                                             otherButtonTitles:nil];
   [alert show];
#else
   NSAlert* alert = [[NSAlert new] autorelease];
   
   [alert setMessageText:(*title) ? BOXSTRING(title) : BOXSTRING("RetroArch")];
   [alert setInformativeText:BOXSTRING(message)];
   [alert setAlertStyle:NSInformationalAlertStyle];
   [alert beginSheetModalForWindow:[RetroArch_OSX get].window
          modalDelegate:apple_platform
          didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
          contextInfo:nil];
   [[NSApplication sharedApplication] runModalForWindow:[alert window]];
#endif
}

// Number formatter class for setting strings
@implementation RANumberFormatter
- (id)initWithSetting:(const rarch_setting_t*)setting
{
   if ((self = [super init]))
   {
      [self setAllowsFloats:(setting->type == ST_FLOAT)];
      
      if (setting->flags & SD_FLAG_HAS_RANGE)
      {
         [self setMinimum:BOXFLOAT(setting->min)];
         [self setMaximum:BOXFLOAT(setting->max)];      
      }
   }
   
   return self;
}

- (BOOL)isPartialStringValid:(NSString*)partialString newEditingString:(NSString**)newString errorDescription:(NSString**)error
{
   NSUInteger i;
   bool hasDot = false;

   if (partialString.length)
      for (i = 0; i < partialString.length; i ++)
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
   NSString* text = (NSString*)[[textField text] stringByReplacingCharactersInRange:range withString:string];
   return [self isPartialStringValid:text newEditingString:nil errorDescription:nil];
}
#endif

@end
