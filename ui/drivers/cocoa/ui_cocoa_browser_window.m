/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>

#include "cocoa_common.h"
#include "../../ui_companion_driver.h"

static bool ui_browser_window_cocoa_open(ui_browser_window_state_t *state)
{
   NSOpenPanel *panel = [NSOpenPanel openPanel];

   if (!string_is_empty(state->filters))
   {
#ifdef HAVE_COCOA_METAL
      [panel setAllowedFileTypes:@[BOXSTRING(state->filters), BOXSTRING(state->filters_title)]];
#else
      NSArray *filetypes = [[NSArray alloc] initWithObjects:BOXSTRING(state->filters), BOXSTRING(state->filters_title), nil];
      [panel setAllowedFileTypes:filetypes];
#endif
   }

#if defined(MAC_OS_X_VERSION_10_5)
   [panel setMessage:BOXSTRING(state->title)];
   if ([panel runModalForDirectory:BOXSTRING(state->startdir) file:nil] != 1)
      return false;
#else
   panel.title                           = NSLocalizedString(BOXSTRING(state->title), BOXSTRING("open panel"));
   panel.directoryURL                    = [NSURL fileURLWithPath:BOXSTRING(state->startdir)];
   panel.canChooseDirectories            = NO;
   panel.canChooseFiles                  = YES;
   panel.allowsMultipleSelection         = NO;
   panel.treatsFilePackagesAsDirectories = NO;

#if defined(HAVE_COCOA_METAL)
   NSModalResponse result = [panel runModal];
   if (result != NSModalResponseOK)
       return false;
#elif defined(HAVE_COCOA)
   NSInteger result       = [panel runModal];
   if (result != 1)
       return false;
#endif
#endif

   NSURL *url           = (NSURL*)panel.URL;
   const char *res_path = [url.path UTF8String];
   state->result        = strdup(res_path);

   return true;
}

static bool ui_browser_window_cocoa_save(ui_browser_window_state_t *state)
{
   return false;
}

ui_browser_window_t ui_browser_window_cocoa = {
   ui_browser_window_cocoa_open,
   ui_browser_window_cocoa_save,
   "cocoa"
};
