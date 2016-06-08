/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "cocoa_common.h"

#include "../../ui_companion_driver.h"

static bool ui_browser_window_cocoa_open(ui_browser_window_state_t *state)
{
   NSOpenPanel* panel    = (NSOpenPanel*)[NSOpenPanel openPanel];
    NSArray *filetypes    = NULL;
    
    if (state->filters && !string_is_empty(state->filters))
        filetypes = [[NSArray alloc] initWithObjects:BOXSTRING(state->filters), BOXSTRING(state->filters_title), nil];
   [panel setAllowedFileTypes:filetypes];
#if defined(MAC_OS_X_VERSION_10_6)
   [panel setMessage:BOXSTRING(state->title)];
   if ([panel runModalForDirectory:BOXSTRING(state->startdir) file:nil] != 1)
        return false;
#else
    [panel setTitle:NSLocalizedString(BOXSTRING(state->title), BOXSTRING("open panel"))];
    [panel setDirectory:BOXSTRING(state->startdir)];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];
    [panel setAllowsMultipleSelection:NO];
    [panel setTreatsFilePackagesAsDirectories:NO];
    NSInteger result = [panel runModal];
    if (result != 1)
        return false;
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

const ui_browser_window_t ui_browser_window_cocoa = {
   ui_browser_window_cocoa_open,
   ui_browser_window_cocoa_save,
   "cocoa"
};
