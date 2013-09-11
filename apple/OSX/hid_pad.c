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
#include "apple/common/apple_input.h"

// NOTE: I pieced this together through trial and error, any corrections are welcome

static IOHIDManagerRef g_hid_manager;
static uint32_t g_pad_slots;

#define HID_ISSET(t, x)  (t  &  (1 << x))
#define HID_SET(t, x)   { t |=  (1 << x); }
#define HID_CLEAR(t, x) { t &= ~(1 << x); }

// Set the LEDs on PS3 controllers, if slot >= MAX_PADS the LEDs will be cleared
static void osx_pad_set_leds(IOHIDDeviceRef device, uint32_t slot)
{
   char buffer[1024];

   CFStringRef device_name = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
   if (device_name)
   {
      CFStringGetCString(device_name, buffer, 1024, kCFStringEncodingUTF8);

      if (strncmp(buffer, "PLAYSTATION(R)3 Controller", 1024) == 0)
      {
         static uint8_t report_buffer[] = {
            0x01,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x27,
            0x10, 0x00, 0x32, 0xff, 0x27, 0x10, 0x00, 0x32, 0xff, 0x27, 0x10, 0x00,
            0x32, 0xff, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
         };
         report_buffer[10] = (slot >= MAX_PADS) ? 0 : (1 << (slot + 1));
      
         IOHIDDeviceSetReport(device, kIOHIDReportTypeOutput, 0x01, report_buffer, sizeof(report_buffer));
      }
   }
}

static void hid_device_input_callback(void* inContext, IOReturn inResult, void* inSender, IOHIDValueRef inIOHIDValueRef)
{
   IOHIDElementRef element = IOHIDValueGetElement(inIOHIDValueRef);
   IOHIDDeviceRef device = IOHIDElementGetDevice(element);

   uint32_t type = IOHIDElementGetType(element);
   uint32_t page = IOHIDElementGetUsagePage(element);
   uint32_t use = IOHIDElementGetUsage(element);

   // Mouse handler
   if (IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse))
   {
      if (type == kIOHIDElementTypeInput_Button && page == kHIDPage_Button)
      {
         CFIndex state = IOHIDValueGetIntegerValue(inIOHIDValueRef);
      
         if (state)  g_current_input_data.mouse_buttons |= (1 << (use - 1));
         else        g_current_input_data.mouse_buttons &= ~(1 << (use - 1));
      }
      else if (type == kIOHIDElementTypeInput_Misc && page == kHIDPage_GenericDesktop)
      {
         static const uint32_t axis_use_ids[2] = { 48, 49 };

         for (int i = 0; i < 2; i ++)
            if (use == axis_use_ids[i])
               g_current_input_data.mouse_delta[i] += IOHIDValueGetIntegerValue(inIOHIDValueRef);
      }
   }
   // Joystick handler
   else if (IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick))
   {
      uint32_t slot = (uint32_t)inContext;
      if (slot >= 4)
         return;

      if (type == kIOHIDElementTypeInput_Button && page == kHIDPage_Button)
      {
         CFIndex state = IOHIDValueGetIntegerValue(inIOHIDValueRef);

         if (state)  g_current_input_data.pad_buttons[slot] |= (1 << (use - 1));
         else        g_current_input_data.pad_buttons[slot] &= ~(1 << (use - 1));
      }
      else if (type == kIOHIDElementTypeInput_Misc && page == kHIDPage_GenericDesktop)
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
}

static void hid_device_removed(void* inContext, IOReturn inResult, void* inSender)
{
   IOHIDDeviceRef inDevice = (IOHIDDeviceRef)inSender;

   if (IOHIDDeviceConformsTo(inDevice, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick))
   {
      uint32_t pad_index = (uint32_t)inContext;

      if (pad_index < MAX_PADS)
      {
         HID_CLEAR(g_pad_slots, pad_index);

         g_current_input_data.pad_buttons[pad_index] = 0;
         memset(g_current_input_data.pad_axis[pad_index], 0, sizeof(g_current_input_data.pad_axis));
      }
   }

   osx_pad_set_leds(inDevice, MAX_PADS);
   IOHIDDeviceClose(inDevice, kIOHIDOptionsTypeNone);
}

static void hid_manager_device_attached(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef inDevice)
{
   uint32_t pad_index = 0;
   if (IOHIDDeviceConformsTo(inDevice, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick))
   {
      if ((g_pad_slots & 0xF) == 0xF)
         return;
      
      for (pad_index = 0; pad_index != MAX_PADS; pad_index ++)
         if (!HID_ISSET(g_pad_slots, pad_index))
         {
            HID_SET(g_pad_slots, pad_index);
            break;
         }
   }
   
   IOHIDDeviceOpen(inDevice, kIOHIDOptionsTypeNone);
   IOHIDDeviceScheduleWithRunLoop(inDevice, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
   IOHIDDeviceRegisterInputValueCallback(inDevice, hid_device_input_callback, (void*)pad_index);
   IOHIDDeviceRegisterRemovalCallback(inDevice, hid_device_removed, (void*)pad_index);

   osx_pad_set_leds(inDevice, pad_index);
}

static CFMutableDictionaryRef build_matching_dictionary(uint32_t page, uint32_t use)
{
   CFMutableDictionaryRef matcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

   CFNumberRef pagen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
   CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsagePageKey), pagen);
   CFRelease(pagen);

   CFNumberRef usen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &use);
   CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsageKey), usen);
   CFRelease(usen);

   return matcher;
}

void osx_pad_init()
{
   if (!g_hid_manager)
   {
      g_hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

      CFMutableArrayRef matcher = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

      CFMutableDictionaryRef mouse = build_matching_dictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse);
      CFArrayAppendValue(matcher, mouse);
      CFRelease(mouse);

      CFMutableDictionaryRef joystick = build_matching_dictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
      CFArrayAppendValue(matcher, joystick);
      CFRelease(joystick);

      IOHIDManagerSetDeviceMatchingMultiple(g_hid_manager, matcher);
      CFRelease(matcher);

      IOHIDManagerRegisterDeviceMatchingCallback(g_hid_manager, hid_manager_device_attached, 0);
      IOHIDManagerScheduleWithRunLoop(g_hid_manager, CFRunLoopGetMain(), kCFRunLoopCommonModes);

      IOHIDManagerOpen(g_hid_manager, kIOHIDOptionsTypeNone);
   }
}

void osx_pad_quit()
{
   if (g_hid_manager)
   {
      IOHIDManagerClose(g_hid_manager, kIOHIDOptionsTypeNone);
      IOHIDManagerUnscheduleFromRunLoop(g_hid_manager, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
      
      CFRelease(g_hid_manager);
   }

   g_hid_manager = 0;
}

