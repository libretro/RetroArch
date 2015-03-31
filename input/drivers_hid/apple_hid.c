/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include "../connect/joypad_connection.h"
#include "../drivers/apple_input.h"

typedef struct apple_hid
{
    IOHIDManagerRef hid_ptr;
    joypad_connection_t *slots;
} apple_hid_t;

static apple_hid_t *hid_apple;

struct pad_connection
{
   int v_id;
   int p_id;
   uint32_t slot;
   IOHIDDeviceRef device_handle;
   uint8_t data[2048];
};

static bool apple_hid_joypad_button(unsigned port, uint16_t joykey)
{
    driver_t *driver = driver_get_ptr();
    apple_input_data_t *apple = (apple_input_data_t*)driver->input_data;
    uint64_t buttons          = pad_connection_get_buttons(&hid_apple->slots[port], port);
    
    if (!apple || joykey == NO_BTN)
        return false;
    
    /* Check hat. */
    if (GET_HAT_DIR(joykey))
        return false;
    
    /* Check the button. */
    if ((port < MAX_USERS) && (joykey < 32))
        return ((apple->buttons[port] & (1 << joykey)) != 0) ||
        ((buttons & (1 << joykey)) != 0);
    return false;
}

static uint64_t apple_hid_joypad_get_buttons(unsigned port)
{
    return pad_connection_get_buttons(&hid_apple->slots[port], port);
}

static bool apple_hid_joypad_rumble(unsigned pad,
                                enum retro_rumble_effect effect, uint16_t strength)
{
    return pad_connection_rumble(&hid_apple->slots[pad], pad, effect, strength);
}

static int16_t apple_hid_joypad_axis(unsigned port, uint32_t joyaxis)
{
    driver_t *driver = driver_get_ptr();
    apple_input_data_t *apple = (apple_input_data_t*)driver->input_data;
    int16_t val = 0;
    
    if (!apple || joyaxis == AXIS_NONE)
        return 0;
    
    if (AXIS_NEG_GET(joyaxis) < 4)
    {
        val = apple->axes[port][AXIS_NEG_GET(joyaxis)];
        val += pad_connection_get_axis(&hid_apple->slots[port], port, AXIS_NEG_GET(joyaxis));
        
        if (val >= 0)
            val = 0;
    }
    else if(AXIS_POS_GET(joyaxis) < 4)
    {
        val = apple->axes[port][AXIS_POS_GET(joyaxis)];
        val += pad_connection_get_axis(&hid_apple->slots[port], port, AXIS_POS_GET(joyaxis));
        
        if (val <= 0)
            val = 0;
    }
    
    return val;
}

static void hid_pad_connection_send_control(void *data, uint8_t* data_buf, size_t size)
{
   struct pad_connection* connection = (struct pad_connection*)data;

   if (connection)
      IOHIDDeviceSetReport(connection->device_handle,
            kIOHIDReportTypeOutput, 0x01, data_buf + 1, size - 1);
}

/* NOTE: I pieced this together through trial and error,
 * any corrections are welcome. */

static void hid_device_input_callback(void* context, IOReturn result,
      void* sender, IOHIDValueRef value)
{
   driver_t *driver = driver_get_ptr();
   apple_input_data_t *apple         = (apple_input_data_t*)driver->input_data;
   struct pad_connection* connection = (struct pad_connection*)context;
   IOHIDElementRef element           = IOHIDValueGetElement(value);
   uint32_t type                     = IOHIDElementGetType(element);
   uint32_t page                     = IOHIDElementGetUsagePage(element);
   uint32_t use                      = IOHIDElementGetUsage(element);

   if (type != kIOHIDElementTypeInput_Misc)
      if (type != kIOHIDElementTypeInput_Button)
         if (type != kIOHIDElementTypeInput_Axis)
            return;

   /* Joystick handler.
    * TODO: Can GamePad work the same? */

   switch (page)
   {
      case kHIDPage_GenericDesktop:
         switch (type)
         {
            case kIOHIDElementTypeInput_Misc:
               switch (use)
               {
                  case kHIDUsage_GD_Hatswitch:
                     break;
                  default:
                     {
                        int i;
                        static const uint32_t axis_use_ids[4] = { 48, 49, 50, 53 };

                        for (i = 0; i < 4; i ++)
                        {
                           CFIndex min   = IOHIDElementGetPhysicalMin(element);
                           CFIndex max   = IOHIDElementGetPhysicalMax(element) - min;
                           CFIndex state = IOHIDValueGetIntegerValue(value) - min;
                           float val     = (float)state / (float)max;

                           if (use != axis_use_ids[i])
                              continue;

                           apple->axes[connection->slot][i] =
                              ((val * 2.0f) - 1.0f) * 32767.0f;
                        }
                     }
                     break;
               }
               break;
         }
         break;
      case kHIDPage_Button:
         switch (type)
         {
            case kIOHIDElementTypeInput_Button:
               {
                  CFIndex state = IOHIDValueGetIntegerValue(value);
                  unsigned id = use - 1;

                  if (state)
                     BIT64_SET(apple->buttons[connection->slot], id);
                  else
                     BIT64_CLEAR(apple->buttons[connection->slot], id);
               }
               break;
         }
         break;
   }
}

static void remove_device(void* context, IOReturn result, void* sender)
{
   driver_t *driver = driver_get_ptr();
   apple_input_data_t *apple = (apple_input_data_t*)driver->input_data;
   struct pad_connection* connection = (struct pad_connection*)context;

   if (connection && (connection->slot < MAX_USERS))
   {
      char msg[PATH_MAX_LENGTH];

      snprintf(msg, sizeof(msg), "Joypad #%u (%s) disconnected.",
            connection->slot, "N/A");
      rarch_main_msg_queue_push(msg, 0, 60, false);
      RARCH_LOG("[apple_input]: %s\n", msg);

      apple->buttons[connection->slot] = 0;
      memset(apple->axes[connection->slot], 0, sizeof(apple->axes));

      pad_connection_pad_deinit(&hid_apple->slots[connection->slot], connection->slot);
      free(connection);
   }
}

static void hid_device_report(void* context, IOReturn result, void *sender,
      IOHIDReportType type, uint32_t reportID, uint8_t *report,
      CFIndex reportLength)
{
   struct pad_connection* connection = (struct pad_connection*)context;

   if (!connection)
      return;

   pad_connection_packet(&hid_apple->slots[connection->slot], connection->slot,
         connection->data, reportLength + 1);
}

static void add_device(void* context, IOReturn result,
      void* sender, IOHIDDeviceRef device)
{
   char device_name[PATH_MAX_LENGTH];
   CFStringRef device_name_ref;
   CFNumberRef vendorID, productID;
   autoconfig_params_t params = {{0}};
   settings_t *settings = config_get_ptr();
   struct pad_connection* connection = (struct pad_connection*)
      calloc(1, sizeof(*connection));

   connection->device_handle = device;
   connection->slot          = MAX_USERS;

   IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);

   /* Move the device's run loop to this thread. */
   IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(),
         kCFRunLoopCommonModes);
   IOHIDDeviceRegisterRemovalCallback(device, remove_device, connection);

#ifndef IOS
   device_name_ref = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
   CFStringGetCString(device_name_ref, device_name,
         sizeof(device_name), kCFStringEncodingUTF8);
#endif

   vendorID = (CFNumberRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey));
   CFNumberGetValue(vendorID, kCFNumberIntType, &connection->v_id);

   productID = (CFNumberRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey));
   CFNumberGetValue(productID, kCFNumberIntType, &connection->p_id);

   connection->slot = pad_connection_pad_init(hid_apple->slots, device_name,
         connection, &hid_pad_connection_send_control);

   if (pad_connection_has_interface(hid_apple->slots, connection->slot))
      IOHIDDeviceRegisterInputReportCallback(device,
            connection->data + 1, sizeof(connection->data) - 1,
            hid_device_report, connection);
   else
      IOHIDDeviceRegisterInputValueCallback(device,
            hid_device_input_callback, connection);

   if (device_name[0] == '\0')
      return;

   strlcpy(settings->input.device_names[connection->slot],
         device_name, sizeof(settings->input.device_names));

   params.idx = connection->slot;
   strlcpy(params.name, device_name, sizeof(params.name));
   params.vid = connection->v_id;
   params.pid = connection->p_id;
   strlcpy(params.driver, apple_hid_joypad.ident, sizeof(params.driver));

   input_config_autoconfigure_joypad(&params);
   RARCH_LOG("Port %d: %s.\n", connection->slot, device_name);
}

static void append_matching_dictionary(CFMutableArrayRef array,
      uint32_t page, uint32_t use)
{
   CFNumberRef usen, pagen;
   CFMutableDictionaryRef matcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
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

static bool apple_hid_init(void)
{
    CFMutableArrayRef matcher;
    
    hid_apple = (apple_hid_t*)calloc(1, sizeof(*hid_apple));
    
    if (!hid_apple)
        return false;
    
    if (!(hid_apple->hid_ptr = IOHIDManagerCreate(
        kCFAllocatorDefault, kIOHIDOptionsTypeNone)))
        goto error;
    
    IOHIDManagerSetDeviceMatching(hid_apple->hid_ptr, NULL);
    IOHIDManagerScheduleWithRunLoop(hid_apple->hid_ptr, CFRunLoopGetCurrent(),
                                    kCFRunLoopDefaultMode);
    
    matcher = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                   &kCFTypeArrayCallBacks);
    
    append_matching_dictionary(matcher, kHIDPage_GenericDesktop,
                               kHIDUsage_GD_Joystick);
    append_matching_dictionary(matcher, kHIDPage_GenericDesktop,
                               kHIDUsage_GD_GamePad);
    
    IOHIDManagerSetDeviceMatchingMultiple(hid_apple->hid_ptr, matcher);
    CFRelease(matcher);
    
    IOHIDManagerRegisterDeviceMatchingCallback(hid_apple->hid_ptr,
                                               add_device, 0);
    
    hid_apple->slots = (joypad_connection_t*)pad_connection_init(MAX_USERS);
    
    return true;
    
error:
    if (hid_apple)
        free(hid_apple);
    return false;
}

static void apple_hid_free(void)
{
    if (!hid_apple || !hid_apple->hid_ptr)
        return;
    
   pad_connection_destroy(hid_apple->slots);
    
    IOHIDManagerClose(hid_apple->hid_ptr, kIOHIDOptionsTypeNone);
    IOHIDManagerUnscheduleFromRunLoop(hid_apple->hid_ptr,
                                      CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    
    CFRelease(hid_apple->hid_ptr);
    
    hid_apple->hid_ptr = NULL;
    
    free(hid_apple);
    hid_apple = NULL;
}
