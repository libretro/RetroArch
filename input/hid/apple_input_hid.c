/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include <IOKit/hid/IOHIDKeys.h>
#include "../apple_input.h"

struct apple_pad_connection
{
    int v_id;
    int p_id;
    uint32_t slot;
    IOHIDDeviceRef device_handle;
    uint8_t data[2048];
};

static IOHIDManagerRef g_hid_manager;

static void apple_pad_send_control(void *connect_data,
      uint8_t* data, size_t size)
{
   struct apple_pad_connection* connection = 
      (struct apple_pad_connection*)connect_data;

   if (connection)
      IOHIDDeviceSetReport(connection->device_handle,
            kIOHIDReportTypeOutput, 0x01, data + 1, size - 1);
}

/* NOTE: I pieced this together through trial and error,
 * any corrections are welcome. */

static void hid_device_input_callback(void* context, IOReturn result,
      void* sender, IOHIDValueRef value)
{
   struct apple_pad_connection* connection = (struct apple_pad_connection*)
      context;
   IOHIDElementRef element = IOHIDValueGetElement(value);
   uint32_t type    = IOHIDElementGetType(element);
   uint32_t page    = IOHIDElementGetUsagePage(element);
   uint32_t use     = IOHIDElementGetUsage(element);

   /* Joystick handler.
    * TODO: Can GamePad work the same? */

   if ((type == kIOHIDElementTypeInput_Button)
         && page == kHIDPage_Button)
   {
      CFIndex state = IOHIDValueGetIntegerValue(value);

      if (state)
         g_current_input_data.pad_buttons[connection->slot] |= (1 << (use - 1));
      else
         g_current_input_data.pad_buttons[connection->slot] &= ~(1 << (use - 1));
   }
   else if (
         (type == kIOHIDElementTypeInput_Misc) &&
         (page == kHIDPage_GenericDesktop))
   {
      static const uint32_t axis_use_ids[4] = { 48, 49, 50, 53 };
      int i;

      for (i = 0; i < 4; i ++)
      {
         if (use == axis_use_ids[i])
         {
            CFIndex min, max, state;
            float val;

            min = IOHIDElementGetPhysicalMin(element);
            max = IOHIDElementGetPhysicalMax(element) - min;
            state = IOHIDValueGetIntegerValue(value) - min;

            val = (float)state / (float)max;
            g_current_input_data.pad_axis[connection->slot][i] =
               ((val * 2.0f) - 1.0f) * 32767.0f;
         }
      }
   }
}

static void hid_device_removed(void* context, IOReturn result, void* sender)
{
    struct apple_pad_connection* connection = (struct apple_pad_connection*)
    context;
    
    if (connection && connection->slot < MAX_PLAYERS)
    {
        char msg[512];
        snprintf(msg, sizeof(msg), "Joypad #%u (%s) disconnected.",
                 connection->slot, "N/A");
        msg_queue_push(g_extern.msg_queue, msg, 0, 60);
        RARCH_LOG("[apple_input]: %s\n", msg);
        
        g_current_input_data.pad_buttons[connection->slot] = 0;
        memset(g_current_input_data.pad_axis[connection->slot],
               0, sizeof(g_current_input_data.pad_axis));
        
        apple_joypad_disconnect(connection->slot);
        free(connection);
    }
    
    IOHIDDeviceClose(sender, kIOHIDOptionsTypeSeizeDevice);
}

static void hid_device_report(void* context, IOReturn result, void *sender,
      IOHIDReportType type, uint32_t reportID, uint8_t *report,
      CFIndex reportLength)
{
    struct apple_pad_connection* connection = (struct apple_pad_connection*)
    context;
    apple_joypad_packet(connection->slot, connection->data, reportLength + 1);
}

static void hid_manager_device_attached(void* context, IOReturn result,
                                        void* sender, IOHIDDeviceRef device)
{
   char device_name[PATH_MAX];
   CFStringRef device_name_ref;
   CFNumberRef vendorID, productID;
   struct apple_pad_connection* connection = (struct apple_pad_connection*)
      calloc(1, sizeof(*connection));

   connection->device_handle = device;
   connection->slot          = MAX_PLAYERS;

   IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);

   /* Move the device's run loop to this thread. */
   IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(),
         kCFRunLoopCommonModes);
   IOHIDDeviceRegisterRemovalCallback(device, hid_device_removed, connection);

#ifndef IOS
   device_name_ref = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
   CFStringGetCString(device_name_ref, device_name,
         sizeof(device_name), kCFStringEncodingUTF8);
#endif

   vendorID = (CFNumberRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey));
   CFNumberGetValue(vendorID, kCFNumberIntType, &connection->v_id);

   productID = (CFNumberRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey));
   CFNumberGetValue(productID, kCFNumberIntType, &connection->p_id);

   connection->slot = apple_joypad_connect(device_name, connection);

   if (apple_joypad_has_interface(connection->slot))
      IOHIDDeviceRegisterInputReportCallback(device,
            connection->data + 1, sizeof(connection->data) - 1,
            hid_device_report, connection);
   else
      IOHIDDeviceRegisterInputValueCallback(device,
            hid_device_input_callback, connection);

   if (device_name[0] != '\0')
   {
      strlcpy(g_settings.input.device_names[connection->slot],
            device_name, sizeof(g_settings.input.device_names));
      input_config_autoconfigure_joypad(connection->slot,
            device_name, apple_joypad.ident);
      RARCH_LOG("Port %d: %s.\n", connection->slot, device_name);
   }
}

static void append_matching_dictionary(CFMutableArrayRef array,
                                       uint32_t page, uint32_t use)
{
   CFNumberRef pagen, usen;
   CFMutableDictionaryRef matcher;

   matcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

   pagen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
   CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsagePageKey), pagen);
   CFRelease(pagen);

   usen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &use);
   CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsageKey), usen);
   CFRelease(usen);

   CFArrayAppendValue(array, matcher);
   CFRelease(matcher);
}

static bool hid_init_manager(void)
{
   CFMutableArrayRef matcher;

   g_hid_manager = IOHIDManagerCreate(
         kCFAllocatorDefault, kIOHIDOptionsTypeNone);

   if (!g_hid_manager)
      return false;

   matcher = CFArrayCreateMutable(kCFAllocatorDefault, 0,
         &kCFTypeArrayCallBacks);
   append_matching_dictionary(matcher, kHIDPage_GenericDesktop,
         kHIDUsage_GD_Joystick);
   append_matching_dictionary(matcher, kHIDPage_GenericDesktop,
         kHIDUsage_GD_GamePad);

   IOHIDManagerSetDeviceMatchingMultiple(g_hid_manager, matcher);
   CFRelease(matcher);

   IOHIDManagerRegisterDeviceMatchingCallback(g_hid_manager,
         hid_manager_device_attached, 0);
   IOHIDManagerScheduleWithRunLoop(g_hid_manager, CFRunLoopGetMain(),
         kCFRunLoopCommonModes);

   IOHIDManagerOpen(g_hid_manager, kIOHIDOptionsTypeNone);

   return true;
}

static int hid_exit(void)
{
   if (!g_hid_manager)
      return -1;

   IOHIDManagerClose(g_hid_manager, kIOHIDOptionsTypeNone);
   IOHIDManagerUnscheduleFromRunLoop(g_hid_manager,
         CFRunLoopGetCurrent(), kCFRunLoopCommonModes);

   CFRelease(g_hid_manager);

   g_hid_manager = NULL;

   return 0;
}
