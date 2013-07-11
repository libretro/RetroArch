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

static void hid_input_callback(void* inContext, IOReturn inResult, void* inSender, IOHIDValueRef inIOHIDValueRef)
{
   IOHIDElementRef element = IOHIDValueGetElement(inIOHIDValueRef);

   uint32_t slot = (uint32_t)inContext;
   if (slot >= 4)
      return;

   uint32_t type = IOHIDElementGetType(element);
   uint32_t page = IOHIDElementGetUsagePage(element);
   uint32_t use = IOHIDElementGetUsage(element);

   if (type == 2 && page == 9)
   {
      CFIndex state = IOHIDValueGetIntegerValue(inIOHIDValueRef);

      if (state)  g_current_input_data.pad_buttons[slot] |= (1 << (use - 1));
      else        g_current_input_data.pad_buttons[slot] &= ~(1 << (use - 1));
   }
   else if (page == 1)
   {
      static const uint32_t axis_use_ids[4] = { 48, 49, 50, 53 };
      for (int i = 0; i < 4; i ++)
      {
         if (use == axis_use_ids[i])
         {
            CFIndex min = IOHIDElementGetPhysicalMin(element);
            CFIndex max = IOHIDElementGetPhysicalMax(element) - min;
            CFIndex state = IOHIDValueGetIntegerValue(inIOHIDValueRef) - min;
            
            float val = (float)state / (float)max;
            g_current_input_data.pad_axis[slot][i] = ((val * 2.0f) - 1.0f) * 32767.0f;
         }
      }
   }
}

static void hid_device_attached(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inDevice)
{
   IOHIDDeviceOpen(inDevice, kIOHIDOptionsTypeNone);
   IOHIDDeviceScheduleWithRunLoop(inDevice, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
   IOHIDDeviceRegisterInputValueCallback(inDevice, hid_input_callback, 0);
}

static void hid_device_removed(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inDevice)
{
   IOHIDDeviceClose(inDevice, kIOHIDOptionsTypeNone);
}

void osx_pad_init()
{
   if (!g_hid_manager)
   {
      g_hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

      CFMutableDictionaryRef matcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      uint32_t page = kHIDPage_GenericDesktop;
      uint32_t use = kHIDUsage_GD_Joystick;

      CFNumberRef pagen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
      CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsagePageKey), pagen);
      CFRelease(pagen);

      CFNumberRef usen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &use);
      CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsageKey), usen);
      CFRelease(usen);

      IOHIDManagerSetDeviceMatching(g_hid_manager, matcher);
      CFRelease(matcher);

      IOHIDManagerRegisterDeviceMatchingCallback(g_hid_manager, hid_device_attached, 0);
      IOHIDManagerRegisterDeviceRemovalCallback(g_hid_manager, hid_device_removed, 0);
      IOHIDManagerScheduleWithRunLoop(g_hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

      IOHIDManagerOpen(g_hid_manager, kIOHIDOptionsTypeNone);
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

