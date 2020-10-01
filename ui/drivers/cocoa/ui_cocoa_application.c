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
#include <limits.h>

#include <objc/objc.h>
#include <objc/message.h>
#include <objc/NSObjCRuntime.h>
#include "cocoa_defines.h"
#include "../../ui_companion_driver.h"

extern id NSApp;
extern id const NSDefaultRunLoopMode;

static void* ui_application_cocoa_initialize(void)
{
   return NULL;
}

static void ui_application_cocoa_process_events(void)
{
    SEL sel_nextEventMatchingMask_untilDate_inMode_dequeue =
    sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:");
    Class class_NSDate  = objc_getClass("NSDate");
    SEL sel_distantPast = sel_registerName("distantPast");
    SEL sel_sendEvent   = sel_registerName("sendEvent:");
    id distant_past     = ((id (*)(Class, SEL))objc_msgSend)(class_NSDate, sel_distantPast);
    for (;;)
    {
        id event = ((id (*)(id, SEL, NSUInteger, id, id, BOOL))objc_msgSend)(NSApp, sel_nextEventMatchingMask_untilDate_inMode_dequeue,
                                NSUIntegerMax,
                                distant_past,
                                NSDefaultRunLoopMode,
                                YES);
        if (event == nil)
            break;
        ((id (*)(id, SEL, id))objc_msgSend)(NSApp, sel_sendEvent, event);
    }
}

ui_application_t ui_application_cocoa = {
   ui_application_cocoa_initialize,
   ui_application_cocoa_process_events,
   NULL,
   false,
   "cocoa"
};
