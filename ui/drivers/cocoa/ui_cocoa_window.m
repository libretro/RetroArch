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

void ui_window_cocoa_destroy(void *data)
{
}

void ui_window_cocoa_set_focused(void *data)
{
    ui_window_cocoa_t *cocoa = (ui_window_cocoa_t*)data;
    CocoaView *cocoa_view    = (CocoaView*)cocoa->data;
    [[cocoa_view window] makeKeyAndOrderFront:nil];
}

void ui_window_cocoa_set_visible(void *data,
        bool set_visible)
{
    ui_window_cocoa_t *cocoa = (ui_window_cocoa_t*)data;
    CocoaView *cocoa_view    = (CocoaView*)cocoa->data;
    if (set_visible)
        [[cocoa_view window] makeKeyAndOrderFront:nil];
    else
        [[cocoa_view window] orderOut:nil];
}

void ui_window_cocoa_set_title(void *data, char *buf)
{
   ui_window_cocoa_t *cocoa = (ui_window_cocoa_t*)data;
   CocoaView *cocoa_view    = (CocoaView*)cocoa->data;
   const char* const text   = buf; /* < Can't access buffer directly in the block */
   [[cocoa_view window] setTitle:[NSString stringWithCString:text encoding:NSUTF8StringEncoding]];
}

const ui_window_t ui_window_cocoa = {
   ui_window_cocoa_destroy,
   ui_window_cocoa_set_focused,
   ui_window_cocoa_set_visible,
   ui_window_cocoa_set_title,
   "cocoa"
};
