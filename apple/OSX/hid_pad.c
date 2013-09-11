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
#include "apple/common/hidpad/hidpad.h"

#include "apple/common/hidpad/hidpad_ps3.c"
#include "apple/common/hidpad/hidpad_wii.c"
#include "apple/common/hidpad/wiimote.c"

struct hidpad_connection
{
   uint32_t slot;

   struct hidpad_interface* interface;
   void* hidpad;

   IOHIDDeviceRef device;
   
   uint8_t data[2048];
};

static IOHIDManagerRef g_hid_manager;
static struct hidpad_connection g_connected_pads[MAX_PADS];

void hidpad_send_control(struct hidpad_connection* connection, uint8_t* data, size_t size)
{
   IOHIDDeviceSetReport(connection->device, kIOHIDReportTypeOutput, 0x01, data, size);
}

// NOTE: I pieced this together through trial and error, any corrections are welcome
static void hid_device_input_callback(void* context, IOReturn result, void* sender, IOHIDValueRef value)
{
   struct hidpad_connection* connection = context;

   IOHIDElementRef element = IOHIDValueGetElement(value);
   uint32_t type = IOHIDElementGetType(element);
   uint32_t page = IOHIDElementGetUsagePage(element);
   uint32_t use = IOHIDElementGetUsage(element);

   // Mouse handler
   if (!connection)
   {
      if (type == kIOHIDElementTypeInput_Button && page == kHIDPage_Button)
      {
         CFIndex state = IOHIDValueGetIntegerValue(value);
      
         if (state)  g_current_input_data.mouse_buttons |= (1 << (use - 1));
         else        g_current_input_data.mouse_buttons &= ~(1 << (use - 1));
      }
      else if (type == kIOHIDElementTypeInput_Misc && page == kHIDPage_GenericDesktop)
      {
         static const uint32_t axis_use_ids[2] = { 48, 49 };

         for (int i = 0; i < 2; i ++)
            if (use == axis_use_ids[i])
               g_current_input_data.mouse_delta[i] += IOHIDValueGetIntegerValue(value);
      }
   }
   // Joystick handler: TODO: Can GamePad work the same?
   else if (IOHIDDeviceConformsTo(connection->device, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick))
   {
      if (type == kIOHIDElementTypeInput_Button && page == kHIDPage_Button)
      {
         CFIndex state = IOHIDValueGetIntegerValue(value);

         if (state)  g_current_input_data.pad_buttons[connection->slot] |= (1 << (use - 1));
         else        g_current_input_data.pad_buttons[connection->slot] &= ~(1 << (use - 1));
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
               CFIndex state = IOHIDValueGetIntegerValue(value) - min;
            
               float val = (float)state / (float)max;
               g_current_input_data.pad_axis[connection->slot][i] = ((val * 2.0f) - 1.0f) * 32767.0f;
            }
         }
      }
   }
}

static void hid_device_removed(void* context, IOReturn result, void* sender)
{
   struct hidpad_connection* connection = (struct hidpad_connection*)context;

   if (connection && connection->slot < MAX_PADS)
   {
      g_current_input_data.pad_buttons[connection->slot] = 0;
      memset(g_current_input_data.pad_axis[connection->slot], 0, sizeof(g_current_input_data.pad_axis));
      
      if (connection->interface)
         connection->interface->disconnect(connection->hidpad);
      memset(connection, 0, sizeof(*connection));
   }

   IOHIDDeviceClose(sender, kIOHIDOptionsTypeNone);
}

static void hid_device_report(void* context, IOReturn result, void *sender, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex reportLength)
{
   struct hidpad_connection* connection = (struct hidpad_connection*)context;
   connection->interface->packet_handler(connection->hidpad, report, reportLength);
}

static void hid_manager_device_attached(void* context, IOReturn result, void* sender, IOHIDDeviceRef device)
{
   bool is_pad = (IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick) ||
                  IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad));

   struct hidpad_connection* connection = 0;
   
   if (is_pad)
   {
      for (int i = 0; i != MAX_PADS; i ++)
      {
         if (!g_connected_pads[i].device)
         {
            connection = &g_connected_pads[i];
            connection->device = device;
            connection->slot = i;
            break;
         }
      }

      if (!connection)
         return;
   }

   IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
   IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
   IOHIDDeviceRegisterRemovalCallback(device, hid_device_removed, connection);

   CFStringRef device_name = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
   if (is_pad && device_name)
   {
      static const struct { const char* name; struct hidpad_interface* iface; } hidpad_map[] = {
         { "Nintendo RVL-CNT-01",         &hidpad_wii },
         { "PLAYSTATION(R)3 Controller",  &hidpad_ps3 },
         { 0, 0} };
   
      char buffer[1024];
      CFStringGetCString(device_name, buffer, 1024, kCFStringEncodingUTF8);

      for (int i = 0; hidpad_map[i].name; i ++)
      {
         if (strstr(buffer, hidpad_map[i].name))
         {
            connection->interface = hidpad_map[i].iface;
            IOHIDDeviceRegisterInputReportCallback(device, connection->data, 2048, hid_device_report, connection);
            connection->hidpad = connection->interface->connect(connection, connection->slot);
            return;
         }
      }
   }
 
   IOHIDDeviceRegisterInputValueCallback(device, hid_device_input_callback, connection);
}

static void append_matching_dictionary(CFMutableArrayRef array, uint32_t page, uint32_t use)
{
   CFMutableDictionaryRef matcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

   CFNumberRef pagen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
   CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsagePageKey), pagen);
   CFRelease(pagen);

   CFNumberRef usen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &use);
   CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsageKey), usen);
   CFRelease(usen);

   CFArrayAppendValue(array, matcher);
   CFRelease(matcher);
}

void osx_pad_init()
{
   if (!g_hid_manager)
   {
      g_hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

      CFMutableArrayRef matcher = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
      append_matching_dictionary(matcher, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse);
      append_matching_dictionary(matcher, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
      append_matching_dictionary(matcher, kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad);

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

