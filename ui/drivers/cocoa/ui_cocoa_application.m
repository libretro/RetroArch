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

#include <objc/objc-runtime.h>
#include "cocoa_defines.h"
#include "cocoa_common.h"
#include "../../ui_companion_driver.h"

static void* ui_application_cocoa_initialize(void)
{
   return NULL;
}

static void ui_application_cocoa_process_events(void)
{
    while (1)
    {
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
        if (!event)
            break;
#ifndef HAVE_COCOA_METAL
        [event retain];
#endif
        [NSApp sendEvent: event];
#ifndef HAVE_COCOA_METAL
        [event retain];
#endif
    }
}

ui_application_t ui_application_cocoa = {
   ui_application_cocoa_initialize,
   ui_application_cocoa_process_events,
   NULL,
   false,
   "cocoa"
};
