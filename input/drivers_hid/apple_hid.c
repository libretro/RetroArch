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
#include "../input_hid_driver.h"

typedef struct apple_hid
{
    IOHIDManagerRef hid_ptr;
    joypad_connection_t *slots;
} apple_hid_t;

struct apple_hid_adapter
{
   uint32_t slot;
   IOHIDDeviceRef handle;
   char name[PATH_MAX_LENGTH];
   uint8_t data[2048];
};

static bool apple_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *apple_hid_joypad_name(void *data, unsigned pad)
{
   /* TODO/FIXME - implement properly */
   if (pad >= MAX_USERS)
      return NULL;

   return NULL;
}

static uint64_t apple_hid_joypad_get_buttons(void *data, unsigned port)
{
   apple_hid_t        *hid   = (apple_hid_t*)data;
   if (hid)
      return pad_connection_get_buttons(&hid->slots[port], port);
   return 0;
}

static bool apple_hid_joypad_button(void *data, unsigned port, uint16_t joykey)
{
    driver_t          *driver = driver_get_ptr();
    apple_input_data_t *apple = (apple_input_data_t*)driver->input_data;
    uint64_t buttons          = apple_hid_joypad_get_buttons(data, port);
    
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

static bool apple_hid_joypad_rumble(void *data, unsigned pad,
                                enum retro_rumble_effect effect, uint16_t strength)
{
   apple_hid_t        *hid   = (apple_hid_t*)data;
   if (!hid)
       return false;
   return pad_connection_rumble(&hid->slots[pad], pad, effect, strength);
}

static int16_t apple_hid_joypad_axis(void *data, unsigned port, uint32_t joyaxis)
{
    driver_t          *driver = driver_get_ptr();
    apple_hid_t        *hid   = (apple_hid_t*)data;
    apple_input_data_t *apple = (apple_input_data_t*)driver->input_data;
    int16_t val = 0;
    
    if (!apple || joyaxis == AXIS_NONE)
        return 0;
    
    if (AXIS_NEG_GET(joyaxis) < 4)
    {
        val = apple->axes[port][AXIS_NEG_GET(joyaxis)];
        val += pad_connection_get_axis(&hid->slots[port], port, AXIS_NEG_GET(joyaxis));
        
        if (val >= 0)
            val = 0;
    }
    else if(AXIS_POS_GET(joyaxis) < 4)
    {
        val = apple->axes[port][AXIS_POS_GET(joyaxis)];
        val += pad_connection_get_axis(&hid->slots[port], port, AXIS_POS_GET(joyaxis));
        
        if (val <= 0)
            val = 0;
    }
    
    return val;
}

static void apple_hid_device_send_control(void *data, uint8_t* data_buf, size_t size)
{
   struct apple_hid_adapter *adapter = (struct apple_hid_adapter*)data;

   if (adapter)
      IOHIDDeviceSetReport(adapter->handle,
            kIOHIDReportTypeOutput, 0x01, data_buf + 1, size - 1);
}

static void apple_hid_device_report(void *data, IOReturn result, void *sender,
                                    IOHIDReportType type, uint32_t reportID, uint8_t *report,
                                    CFIndex reportLength)
{
    struct apple_hid_adapter *adapter = (struct apple_hid_adapter*)data;
    driver_t *driver = driver_get_ptr();
    apple_hid_t *hid = driver ? (apple_hid_t*)driver->hid_data : NULL;
    
    if (adapter)
        pad_connection_packet(&hid->slots[adapter->slot], adapter->slot,
                              adapter->data, reportLength + 1);
}

/* NOTE: I pieced this together through trial and error,
 * any corrections are welcome. */

static void apple_hid_device_input_callback(void *data, IOReturn result,
      void* sender, IOHIDValueRef value)
{
   driver_t                  *driver = driver_get_ptr();
   apple_input_data_t         *apple = (apple_input_data_t*)driver->input_data;
   struct apple_hid_adapter *adapter = (struct apple_hid_adapter*)data;
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

                           apple->axes[adapter->slot][i] =
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
                     BIT64_SET(apple->buttons[adapter->slot], id);
                  else
                     BIT64_CLEAR(apple->buttons[adapter->slot], id);
               }
               break;
         }
         break;
   }
}

static void apple_hid_device_remove(void *data, IOReturn result, void* sender)
{
   driver_t                  *driver = driver_get_ptr();
   apple_input_data_t         *apple = (apple_input_data_t*)driver->input_data;
   struct apple_hid_adapter *adapter = (struct apple_hid_adapter*)data;
   apple_hid_t                  *hid = driver ? (apple_hid_t*)driver->hid_data : NULL;

   if (adapter && (adapter->slot < MAX_USERS))
   {
      char msg[PATH_MAX_LENGTH];

      snprintf(msg, sizeof(msg), "Joypad #%u (%s) disconnected.",
            adapter->slot, adapter->name);
      rarch_main_msg_queue_push(msg, 0, 60, false);

      apple->buttons[adapter->slot] = 0;
      memset(apple->axes[adapter->slot], 0, sizeof(apple->axes));

      pad_connection_pad_deinit(&hid->slots[adapter->slot], adapter->slot);
      free(adapter);
   }
}

static int32_t apple_hid_device_get_int_property(IOHIDDeviceRef device, CFStringRef key)
{
    int32_t value;
    CFNumberRef ref = IOHIDDeviceGetProperty(device, key);
    
    if (ref)
    {
        if (CFGetTypeID(ref) == CFNumberGetTypeID())
        {
            CFNumberGetValue((CFNumberRef)ref, kCFNumberIntType, &value);
            return value;
        }
    }
    
    return 0;
}

static uint16_t apple_hid_device_get_vendor_id(IOHIDDeviceRef device)
{
   return apple_hid_device_get_int_property(device, CFSTR(kIOHIDVendorIDKey));
}

static uint16_t apple_hid_device_get_product_id(IOHIDDeviceRef device)
{
   return apple_hid_device_get_int_property(device, CFSTR(kIOHIDProductIDKey));
}

static void apple_hid_device_get_product_string(IOHIDDeviceRef device, char *buf, size_t len)
{
    CFStringRef ref = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
    
    if (ref)
       CFStringGetCString(ref, buf, len, kCFStringEncodingUTF8);
}

static void apple_hid_device_add_autodetect(unsigned idx,
   const char *device_name, const char *driver_name,
   uint16_t dev_vid, uint16_t dev_pid)
{
   autoconfig_params_t params = {{0}};
    
   params.idx = idx;
   params.vid = dev_vid;
   params.pid = dev_pid;
    
   strlcpy(params.name, device_name, sizeof(params.name));
   strlcpy(params.driver, driver_name, sizeof(params.driver));
    
   input_config_autoconfigure_joypad(&params);
   RARCH_LOG("Port %d: %s.\n", idx, device_name);
}

static void apple_hid_device_add(void *data, IOReturn result,
      void* sender, IOHIDDeviceRef device)
{
   IOReturn ret;
   uint16_t dev_vid, dev_pid;

   settings_t *settings = config_get_ptr();
   driver_t   *driver   = driver_get_ptr();
   apple_hid_t     *hid = driver ? (apple_hid_t*)driver->hid_data : NULL;
   struct apple_hid_adapter *adapter = (struct apple_hid_adapter*)
      calloc(1, sizeof(*adapter));
    
   if (!adapter || !hid)
       return;

   adapter->handle        = device;
   adapter->slot          = MAX_USERS;

   ret = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
    
   if (ret != kIOReturnSuccess)
       goto error;

   /* Move the device's run loop to this thread. */
   IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(),
         kCFRunLoopCommonModes);
   IOHIDDeviceRegisterRemovalCallback(device, apple_hid_device_remove, adapter);

#ifndef IOS
    apple_hid_device_get_product_string(device, adapter->name,
                                 sizeof(adapter->name));
#endif

   dev_vid = apple_hid_device_get_vendor_id  (device);
   dev_pid = apple_hid_device_get_product_id (device);

   adapter->slot = pad_connection_pad_init(hid->slots,
        adapter->name, adapter, &apple_hid_device_send_control);

   if (pad_connection_has_interface(hid->slots, adapter->slot))
      IOHIDDeviceRegisterInputReportCallback(device,
            adapter->data + 1, sizeof(adapter->data) - 1,
            apple_hid_device_report, adapter);
   else
      IOHIDDeviceRegisterInputValueCallback(device,
            apple_hid_device_input_callback, adapter);

   if (adapter->name[0] == '\0')
      return;

   strlcpy(settings->input.device_names[adapter->slot],
         adapter->name, sizeof(settings->input.device_names));
    
    apple_hid_device_add_autodetect(adapter->slot,
        adapter->name, apple_hid.ident, dev_vid, dev_pid);
    
error:
   return;
}

static void apple_hid_append_matching_dictionary(CFMutableArrayRef array,
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

static int apple_hid_manager_init(apple_hid_t *hid)
{
    if (!hid)
        return -1;
    if (hid->hid_ptr) /* already initialized. */
        return 0;
    
    hid->hid_ptr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    
    if (hid->hid_ptr)
    {
        IOHIDManagerSetDeviceMatching(hid->hid_ptr, NULL);
        IOHIDManagerScheduleWithRunLoop(hid->hid_ptr, CFRunLoopGetCurrent(),
                                        kCFRunLoopDefaultMode);
        return 0;
    }
    
    return -1;
}


static int apple_hid_manager_free(apple_hid_t *hid)
{
    if (!hid || !hid->hid_ptr)
        return -1;
    
    IOHIDManagerUnscheduleFromRunLoop(hid->hid_ptr,
                                      CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    IOHIDManagerClose(hid->hid_ptr, kIOHIDOptionsTypeNone);
    CFRelease(hid->hid_ptr);
    hid->hid_ptr = NULL;
    
    return 0;
}

static int apple_hid_manager_set_device_matching(apple_hid_t *hid)
{
    CFMutableArrayRef matcher = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                   &kCFTypeArrayCallBacks);
    
    if (!matcher)
        return -1;
    
    apple_hid_append_matching_dictionary(matcher, kHIDPage_GenericDesktop,
                                         kHIDUsage_GD_Joystick);
    apple_hid_append_matching_dictionary(matcher, kHIDPage_GenericDesktop,
                                         kHIDUsage_GD_GamePad);
    
    IOHIDManagerSetDeviceMatchingMultiple(hid->hid_ptr, matcher);
    IOHIDManagerRegisterDeviceMatchingCallback(hid->hid_ptr,
                                               apple_hid_device_add, 0);
    
    CFRelease(matcher);
    
    return 0;
}

static void *apple_hid_init(void)
{
    apple_hid_t *hid_apple = (apple_hid_t*)calloc(1, sizeof(*hid_apple));
    
    if (!hid_apple)
        goto error;
    if (apple_hid_manager_init(hid_apple) == -1)
        goto error;
    if (apple_hid_manager_set_device_matching(hid_apple) == -1)
        goto error;
    
    hid_apple->slots = (joypad_connection_t*)pad_connection_init(MAX_USERS);
    
    return hid_apple;
    
error:
    if (hid_apple)
        free(hid_apple);
    return NULL;
}

static void apple_hid_free(void *data)
{
   apple_hid_t *hid_apple = (apple_hid_t*)data;
    
   if (!hid_apple || !hid_apple->hid_ptr)
        return;
    
   pad_connection_destroy(hid_apple->slots);
   apple_hid_manager_free(hid_apple);
    
   if (hid_apple)
      free(hid_apple);
}

static void apple_hid_poll(void *data)
{
   (void)data;
}

hid_driver_t apple_hid = {
   apple_hid_init,
   apple_hid_joypad_query,
   apple_hid_free,
   apple_hid_joypad_button,
   apple_hid_joypad_get_buttons,
   apple_hid_joypad_axis,
   apple_hid_poll,
   apple_hid_joypad_rumble,
   apple_hid_joypad_name,
   "apple",
};
