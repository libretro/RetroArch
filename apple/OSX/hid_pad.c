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

#include <IOKit/hid/IOHIDManager.h>
#include "../RetroArch/apple_input.h"

static IOHIDManagerRef g_hid_manager;

static CFMutableDictionaryRef hu_CreateDeviceMatchingDictionary( UInt32 inUsagePage, UInt32 inUsage )
{
   // create a dictionary to add usage page/usages to
   CFMutableDictionaryRef result = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
   if (!result) return 0;

   CFNumberRef pageCFNumberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &inUsagePage);
   CFDictionarySetValue(result, CFSTR(kIOHIDDeviceUsagePageKey), pageCFNumberRef);
   CFRelease(pageCFNumberRef);

   CFNumberRef usageCFNumberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &inUsage);
   CFDictionarySetValue(result, CFSTR(kIOHIDDeviceUsageKey), usageCFNumberRef);
   CFRelease(usageCFNumberRef);

   return result;
}

static void hid_device_attached(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inDevice)
{
}

static void hid_device_removed(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inDevice)
{
}

static void hid_input_callback(void* inContext, IOReturn inResult, void* inSender, IOHIDValueRef inIOHIDValueRef)
{
   IOHIDElementRef ref = IOHIDValueGetElement(inIOHIDValueRef);

   uint32_t type = IOHIDElementGetType(ref);
   uint32_t page = IOHIDElementGetUsagePage(ref);
   uint32_t use = IOHIDElementGetUsage(ref);

   if (type == 2 && page == 9)
   {
      int state = (int)IOHIDValueGetIntegerValue(inIOHIDValueRef);

      if (state)  g_current_input_data.pad_buttons[0] |= (1 << use);
      else        g_current_input_data.pad_buttons[0] &= ~(1 << use);
   }
   else if (page == 1)
   {
/*      static const uint32_t axis_use_ids[4] = { 48, 49, 50, 53 };
      for (int i = 0; i < 4; i ++)
      {
         if (use == axis_use_ids[i])
         {
            int state = (int)IOHIDValueGetIntegerValue(inIOHIDValueRef);
            printf("axis %d %d\n", i, state);
         }
      }*/
   }
}

void osx_pad_init()
{
   if (!g_hid_manager)
   {
      g_hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

      CFDictionaryRef matchingCFDictRef = hu_CreateDeviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
      IOHIDManagerSetDeviceMatching(g_hid_manager, matchingCFDictRef);
      CFRelease(matchingCFDictRef);

      IOHIDManagerRegisterDeviceMatchingCallback(g_hid_manager, hid_device_attached, 0);
      IOHIDManagerRegisterDeviceRemovalCallback(g_hid_manager, hid_device_removed, 0);
      IOHIDManagerScheduleWithRunLoop(g_hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

      IOHIDManagerOpen(g_hid_manager, kIOHIDOptionsTypeNone);
      IOHIDManagerRegisterInputValueCallback(g_hid_manager, hid_input_callback, 0);
   }
}

void osx_pad_quit()
{
   if (g_hid_manager)
   {
      IOHIDManagerClose(g_hid_manager, kIOHIDOptionsTypeNone);
      IOHIDManagerUnscheduleFromRunLoop(g_hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
      
      CFRelease(g_hid_manager);
   }

   g_hid_manager = 0;
}

