/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Courtesy Contributor - Olivier Parra
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

#include <stdlib.h>

#include <string/stdstring.h>

#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>

#include <retro_miscellaneous.h>

#include "../input_defines.h"
#include "../input_driver.h"

#include "../connect/joypad_connection.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

typedef struct apple_input_rec
{
   IOHIDElementCookie cookie;
   uint32_t id;
   struct apple_input_rec *next;
} apple_input_rec_t;

typedef struct apple_hid
{
   IOHIDManagerRef ptr;
   joypad_connection_t *slots;
   uint32_t buttons[MAX_USERS];
   int16_t axes[MAX_USERS][11];
   int8_t hats[MAX_USERS][2]; /* MacOS only supports 1 hat AFAICT */
} iohidmanager_hid_t;

struct iohidmanager_hid_adapter
{
   uint32_t slot;
   IOHIDDeviceRef handle;
   uint32_t locationId;
   char name[PATH_MAX_LENGTH];
   apple_input_rec_t *axes;
   apple_input_rec_t *hats;
   apple_input_rec_t *buttons;
   uint8_t data[2048];
};

enum IOHIDReportType translate_hid_report_type(
   int report_type)
{
   switch (report_type)
   {
      case HID_REPORT_FEATURE:
         return kIOHIDReportTypeFeature;
      case HID_REPORT_INPUT:
         return kIOHIDReportTypeInput;
      case HID_REPORT_OUTPUT:
         return kIOHIDReportTypeOutput;
      case HID_REPORT_COUNT:
         return kIOHIDReportTypeCount;
   }
}

CFComparisonResult iohidmanager_sort_elements(const void *val1, const void *val2, void *context)
{
   uint32_t page1   = (uint32_t)IOHIDElementGetUsagePage((IOHIDElementRef)val1);
   uint32_t page2   = (uint32_t)IOHIDElementGetUsagePage((IOHIDElementRef)val2);
   uint32_t use1    = (uint32_t)IOHIDElementGetUsage((IOHIDElementRef)val1);
   uint32_t use2    = (uint32_t)IOHIDElementGetUsage((IOHIDElementRef)val2);
   uint32_t cookie1 = (uint32_t)IOHIDElementGetCookie((IOHIDElementRef)val1);
   uint32_t cookie2 = (uint32_t)IOHIDElementGetCookie((IOHIDElementRef)val2);

   if (page1 != page2)
      return (CFComparisonResult)(page1 > page2);

   if (use1 != use2)
       return (CFComparisonResult)(use1 > use2);

   return (CFComparisonResult)(cookie1 > cookie2);
}

static bool iohidmanager_check_for_id(apple_input_rec_t *rec, uint32_t id)
{
   while (rec)
   {
      if (rec->id == id)
         return true;
      rec = rec->next;
   }
   return false;
}

static void iohidmanager_append_record(apple_input_rec_t *rec, apple_input_rec_t *b)
{
   apple_input_rec_t *tmp = rec;
   while (tmp->next)
      tmp = tmp->next;
   tmp->next = b;
}

/* Insert a new detected button into a button ordered list.
 * Button list example with Nimbus Controller:
 *                  +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
 * "id" list member |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  | 144 | 145 | 146 | 147 | 547 |
 *  Final Button ID |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  | 10  | 11  | 12  |
 *                  +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
 *           Ranges |<         X/Y/A/B/L1/L2/R1/R2 buttons         >|<        D-PAD        >|MENU |
 * In that way, HID button IDs allocation:
 *   - becomes robust and determinist
 *   - remains compatible with previous algorithm (i.e. btn->id = (uint32_t)(use - 1)) and so
 *             compatible with previous autoconfig files.
 */
static void iohidmanager_append_record_ordered(apple_input_rec_t **p_rec, apple_input_rec_t *b)
{
    apple_input_rec_t *tmp = *p_rec;
    while (tmp && (tmp->id <= b->id))
    {
       p_rec = &tmp->next;
       tmp   = tmp->next;
    }
    b->next  = tmp;
    *p_rec   = b;
}

static bool iohidmanager_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *iohidmanager_hid_joypad_name(void *data, unsigned pad)
{
   /* TODO/FIXME - implement properly */
   if (pad >= MAX_USERS)
      return NULL;

   return NULL;
}

static void iohidmanager_hid_joypad_get_buttons(void *data,
      unsigned port, input_bits_t *state)
{
  iohidmanager_hid_t        *hid   = (iohidmanager_hid_t*)data;
  if (hid)
    return pad_connection_get_buttons(&hid->slots[port], port, state);
  else
    BIT256_CLEAR_ALL_PTR(state);
}

static int16_t iohidmanager_hid_joypad_button(void *data,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir;
   input_bits_t buttons;
   iohidmanager_hid_t *hid              = (iohidmanager_hid_t*)data;

   if (port >= DEFAULT_MAX_PADS)
      return 0;

   iohidmanager_hid_joypad_get_buttons(data, port, &buttons);

   hat_dir                  = GET_HAT_DIR(joykey);

   /* Check hat. */
   if (hat_dir)
   {
      unsigned h = GET_HAT(joykey);
      if (h >= 1)
         return 0;

      switch (hat_dir)
      {
         case HAT_LEFT_MASK:
            return (hid->hats[port][0] < 0);
         case HAT_RIGHT_MASK:
            return (hid->hats[port][0] > 0);
         case HAT_UP_MASK:
            return (hid->hats[port][1] < 0);
         case HAT_DOWN_MASK:
            return (hid->hats[port][1] > 0);
         default:
            break;
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < 32)
      return ((BIT256_GET(buttons, joykey) != 0)
            || ((hid->buttons[port] & (1 << joykey)) != 0));
   return 0;
}

static int16_t iohidmanager_hid_joypad_axis(void *data,
      unsigned port, uint32_t joyaxis)
{
   iohidmanager_hid_t   *hid = (iohidmanager_hid_t*)data;

   if (AXIS_NEG_GET(joyaxis) < 11)
   {
      int16_t val  = hid->axes[port][AXIS_NEG_GET(joyaxis)]
         + pad_connection_get_axis(&hid->slots[port],
               port, AXIS_NEG_GET(joyaxis));

      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < 11)
   {
      int16_t val = hid->axes[port][AXIS_POS_GET(joyaxis)]
         + pad_connection_get_axis(&hid->slots[port],
               port, AXIS_POS_GET(joyaxis));

      if (val > 0)
         return val;
   }
   return 0;
}


static int16_t iohidmanager_hid_joypad_state(
      void *data,
      rarch_joypad_info_t *joypad_info,
      const void *binds_data,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   const struct retro_keybind *binds    = (const struct retro_keybind*)binds_data;
   uint16_t port_idx                    = joypad_info->joy_idx;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN
            && iohidmanager_hid_joypad_button(data, port_idx, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(iohidmanager_hid_joypad_axis(data, port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static bool iohidmanager_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   iohidmanager_hid_t        *hid   = (iohidmanager_hid_t*)data;
   if (!hid)
      return false;
   return pad_connection_rumble(&hid->slots[pad], pad, effect, strength);
}

static void iohidmanager_hid_device_send_control(void *data,
      uint8_t* data_buf, size_t size)
{
   struct iohidmanager_hid_adapter *adapter =
      (struct iohidmanager_hid_adapter*)data;

   if (adapter)
      IOHIDDeviceSetReport(adapter->handle,
            kIOHIDReportTypeOutput, 0x01, data_buf + 1, size - 1);
}

static void iohidmanager_hid_device_report(void *data,
      IOReturn result, void *sender,
      IOHIDReportType type, uint32_t reportID, uint8_t *report,
      CFIndex reportLength)
{
   struct iohidmanager_hid_adapter *adapter =
      (struct iohidmanager_hid_adapter*)data;
   iohidmanager_hid_t *hid = (iohidmanager_hid_t*)hid_driver_get_data();

   if (hid && adapter)
      pad_connection_packet(&hid->slots[adapter->slot], adapter->slot,
            adapter->data, (uint32_t)(reportLength + 1));
}

/* NOTE: I pieced this together through trial and error,
 * any corrections are welcome. */

static void iohidmanager_hid_device_input_callback(void *data, IOReturn result,
      void* sender, IOHIDValueRef value)
{
   iohidmanager_hid_t *hid                  = (iohidmanager_hid_t*)hid_driver_get_data();
   struct iohidmanager_hid_adapter *adapter = (struct iohidmanager_hid_adapter*)data;
   IOHIDElementRef element                  = IOHIDValueGetElement(value);
   uint32_t type                            = (uint32_t)IOHIDElementGetType(element);
   uint32_t page                            = (uint32_t)IOHIDElementGetUsagePage(element);
   uint32_t use                             = (uint32_t)IOHIDElementGetUsage(element);
   uint32_t cookie                          = (uint32_t)IOHIDElementGetCookie(element);
   apple_input_rec_t *tmp                   = NULL;

   if (type != kIOHIDElementTypeInput_Misc)
      if (type != kIOHIDElementTypeInput_Button)
         if (type != kIOHIDElementTypeInput_Axis)
            return;

   /* Joystick handler.
    * TODO: Can GamePad work the same? */

   int pushed_button = 0;

   switch (page)
   {
      case kHIDPage_GenericDesktop:
         switch (type)
         {
            case kIOHIDElementTypeInput_Misc:
               switch (use)
               {
                  case kHIDUsage_GD_Hatswitch:
                     {
                        tmp = adapter->hats;

                        while (tmp && tmp->cookie != (IOHIDElementCookie)cookie)
                           tmp = tmp->next;

                        if (tmp && tmp->cookie == (IOHIDElementCookie)cookie)
                        {
                           CFIndex min = IOHIDElementGetLogicalMin(element);
                           CFIndex range = IOHIDElementGetLogicalMax(element) - min;
                           CFIndex val   = IOHIDValueGetIntegerValue(value);

                           if (range == 3)
                              val *= 2;

                           if(min == 1)
                              val--;

                           switch(val)
                           {
                              case 0:
                                 /* pos = up */
                                 hid->hats[adapter->slot][0] = 0;
                                 hid->hats[adapter->slot][1] = -1;
                                 break;
                              case 1:
                                 /* pos = up+right */
                                 hid->hats[adapter->slot][0] = 1;
                                 hid->hats[adapter->slot][1] = -1;
                                 break;
                              case 2:
                                 /* pos = right */
                                 hid->hats[adapter->slot][0] = 1;
                                 hid->hats[adapter->slot][1] = 0;
                                 break;
                              case 3:
                                 /* pos = down+right */
                                 hid->hats[adapter->slot][0] = 1;
                                 hid->hats[adapter->slot][1] = 1;
                                 break;
                              case 4:
                                 /* pos = down */
                                 hid->hats[adapter->slot][0] = 0;
                                 hid->hats[adapter->slot][1] = 1;
                                 break;
                              case 5:
                                 /* pos = down+left */
                                 hid->hats[adapter->slot][0] = -1;
                                 hid->hats[adapter->slot][1] = 1;
                                 break;
                              case 6:
                                 /* pos = left */
                                 hid->hats[adapter->slot][0] = -1;
                                 hid->hats[adapter->slot][1] = 0;
                                 break;
                              case 7:
                                 /* pos = up_left */
                                 hid->hats[adapter->slot][0] = -1;
                                 hid->hats[adapter->slot][1] = -1;
                                 break;
                              default:
                                 /* pos = centered */
                                 hid->hats[adapter->slot][0] = 0;
                                 hid->hats[adapter->slot][1] = 0;
                                 break;
                           }
                        }
                     }
                     break;
                  default:
                     tmp = adapter->axes;

                     while (tmp && tmp->cookie != (IOHIDElementCookie)cookie)
                        tmp = tmp->next;

                     if (tmp)
                     {
                        if (tmp->cookie == (IOHIDElementCookie)cookie)
                        {
                           CFIndex min   = IOHIDElementGetPhysicalMin(element);
                           CFIndex state = IOHIDValueGetIntegerValue(value) - min;
                           CFIndex max   = IOHIDElementGetPhysicalMax(element) - min;
                           float val     = (float)state / (float)max;

                           hid->axes[adapter->slot][tmp->id] =
                              ((val * 2.0f) - 1.0f) * 32767.0f;
                        }
                     }
                     else
                        pushed_button = 1;
                     break;
               }
               break;
         }
         break;
      case kHIDPage_Consumer:
      case kHIDPage_Button:
         switch (type)
         {
            case kIOHIDElementTypeInput_Misc:
            case kIOHIDElementTypeInput_Button:
               pushed_button = 1;
               break;
         }
         break;
      case kHIDPage_Simulation:
          switch (type)
          {
             case kIOHIDElementTypeInput_Misc:
                 switch (use)
                 {
                 default:
                     tmp = adapter->axes;

                     while (tmp && tmp->cookie != (IOHIDElementCookie)cookie)
                     {
                        tmp = tmp->next;
                     }
                     if (tmp)
                     {
                        if (tmp->cookie == (IOHIDElementCookie)cookie)
                        {
                           CFIndex min   = IOHIDElementGetPhysicalMin(element);
                           CFIndex state = IOHIDValueGetIntegerValue(value) - min;
                           CFIndex max   = IOHIDElementGetPhysicalMax(element) - min;
                           float val     = (float)state / (float)max;

                           hid->axes[adapter->slot][tmp->id] =
                              ((val * 2.0f) - 1.0f) * 32767.0f;
                        }
                     }
                     else
                        pushed_button = 1;
                     break;
                 }
                 break;
          }
          break;
   }

   if (pushed_button)
   {
      uint8_t bit = 0;

      tmp         = adapter->buttons;

      while (tmp && tmp->cookie != (IOHIDElementCookie)cookie)
      {
         bit++;
         tmp = tmp->next;
      }

      if (tmp && tmp->cookie == (IOHIDElementCookie)cookie)
      {
         CFIndex state = IOHIDValueGetIntegerValue(value);
         if (state)
            BIT64_SET(hid->buttons[adapter->slot], bit);
         else
            BIT64_CLEAR(hid->buttons[adapter->slot], bit);
      }
   }
}

static void iohidmanager_hid_device_remove(IOHIDDeviceRef device, iohidmanager_hid_t* hid)
{
   struct iohidmanager_hid_adapter *adapter = NULL;
   int i;
   
   /*loop though the controller ports and find the device with a matching IOHINDeviceRef*/
   for (i=0; i<MAX_USERS; i++)
   {
      struct iohidmanager_hid_adapter *a = (struct iohidmanager_hid_adapter*)hid->slots[i].data;
      if (!a)
         continue;
      if (a->handle == device)
      {
         adapter = a;
         break;
      }
   }
   if (!adapter)
   {
      RARCH_LOG("Error removing device %p\n",device, hid);
      return;
   }
   int slot = adapter->slot;
   if (hid && adapter && (adapter->slot < MAX_USERS))
   {
      input_autoconfigure_disconnect(adapter->slot, adapter->name);

      hid->buttons[adapter->slot] = 0;
      memset(hid->axes[adapter->slot], 0, sizeof(hid->axes));

      pad_connection_pad_deinit(&hid->slots[adapter->slot], adapter->slot);
   }

   if (adapter)
   {
      apple_input_rec_t* tmp = NULL;
      while (adapter->hats != NULL)
      {
          tmp           = adapter->hats;
          adapter->hats = adapter->hats->next;
          free(tmp);
      }

      while (adapter->axes != NULL)
      {
          tmp           = adapter->axes;
          adapter->axes = adapter->axes->next;
          free(tmp);
      }

      while (adapter->buttons != NULL)
      {
          tmp              = adapter->buttons;
          adapter->buttons = adapter->buttons->next;
          free(tmp);
      }
      free(adapter);
   }
   RARCH_LOG("Device removed from port %d\n", slot);
}

static int32_t iohidmanager_hid_device_get_int_property(
      IOHIDDeviceRef device, CFStringRef key)
{
   CFNumberRef ref = (CFNumberRef)IOHIDDeviceGetProperty(device, key);

   if (ref && (CFGetTypeID(ref) == CFNumberGetTypeID()))
   {
      int32_t value   = 0;
      CFNumberGetValue((CFNumberRef)ref, kCFNumberIntType, &value);
      return value;
   }

   return 0;
}

static uint16_t iohidmanager_hid_device_get_vendor_id(IOHIDDeviceRef device)
{
   return iohidmanager_hid_device_get_int_property(device,
         CFSTR(kIOHIDVendorIDKey));
}

static uint16_t iohidmanager_hid_device_get_product_id(IOHIDDeviceRef device)
{
   return iohidmanager_hid_device_get_int_property(device,
         CFSTR(kIOHIDProductIDKey));
}

static uint32_t iohidmanager_hid_device_get_location_id(IOHIDDeviceRef device)
{
   return iohidmanager_hid_device_get_int_property(device,
         CFSTR(kIOHIDLocationIDKey));
}



static void iohidmanager_hid_device_get_product_string(
      IOHIDDeviceRef device, char *buf, size_t len)
{
   CFStringRef ref = (CFStringRef)
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));

   if (ref)
      CFStringGetCString(ref, buf, len, kCFStringEncodingUTF8);
}

static void iohidmanager_hid_device_add_autodetect(unsigned idx,
      const char *device_name, const char *driver_name,
      uint16_t dev_vid, uint16_t dev_pid)
{
   input_autoconfigure_connect(
         device_name,
         NULL,
         "hid",
         idx,
         dev_vid,
         dev_pid
         );

   RARCH_LOG("Port %d: %s.\n", idx, device_name);
}


static void iohidmanager_hid_device_add(IOHIDDeviceRef device, iohidmanager_hid_t* hid)
{
   int i;

    static const uint32_t axis_use_ids[11] =
    {
        kHIDUsage_GD_X,
        kHIDUsage_GD_Y,
        kHIDUsage_GD_Rx,
        kHIDUsage_GD_Ry,
        kHIDUsage_GD_Z,
        kHIDUsage_GD_Rz,
        kHIDUsage_Sim_Rudder,
        kHIDUsage_Sim_Throttle,
        kHIDUsage_Sim_Steering,
        kHIDUsage_Sim_Accelerator,
        kHIDUsage_Sim_Brake
    };

   /* check if pad was already registered previously when the application was
    started (by deterministic method). if so do not re-add the pad */
   uint32_t deviceLocationId = iohidmanager_hid_device_get_location_id(device);
   for (i=0; i<MAX_USERS; i++)
   {
      struct iohidmanager_hid_adapter *a = (struct iohidmanager_hid_adapter*)hid->slots[i].data;
      if (!a)
         continue;
      if (a->locationId == deviceLocationId)
      {
         a->handle = device;
       /* while we're not re-adding the controller, we are re-assigning the
        handle so it can be removed properly upon disconnect */
         return;
      }
   }

   IOReturn ret;
   uint16_t dev_vid, dev_pid;
   CFArrayRef elements_raw;
   int count;
   CFMutableArrayRef elements;
   CFRange range;
   bool found_axis[11] =
   { false, false, false, false, false, false, false, false, false, false, false };
   apple_input_rec_t *tmp                   = NULL;
   apple_input_rec_t *tmpButtons            = NULL;
   apple_input_rec_t *tmpAxes               = NULL;
   struct iohidmanager_hid_adapter *adapter = (struct iohidmanager_hid_adapter*)
      calloc(1, sizeof(*adapter));

   if (!adapter)
      return;
   if (!hid)
      goto error;

   adapter->handle        = device;
   adapter->locationId = deviceLocationId;

   ret = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);

   if (ret != kIOReturnSuccess)
      goto error;

   /* Move the device's run loop to this thread. */
   IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(),
         kCFRunLoopCommonModes);

#ifndef IOS
   iohidmanager_hid_device_get_product_string(device, adapter->name,
         sizeof(adapter->name));
#endif

   dev_vid = iohidmanager_hid_device_get_vendor_id  (device);
   dev_pid = iohidmanager_hid_device_get_product_id (device);

   adapter->slot = pad_connection_pad_init(hid->slots,
         adapter->name, dev_vid, dev_pid, adapter,
         &iohidmanager_hid);

   if (adapter->slot == -1)
      goto error;

   if (string_is_empty(adapter->name))
      strcpy(adapter->name, "Unknown Controller With No Name");
   
   if (pad_connection_has_interface(hid->slots, adapter->slot)) {
      IOHIDDeviceRegisterInputReportCallback(device,
            adapter->data + 1, sizeof(adapter->data) - 1,
            iohidmanager_hid_device_report, adapter);
   }
   else {
      IOHIDDeviceRegisterInputValueCallback(device,
            iohidmanager_hid_device_input_callback, adapter);
   }

   /* scan for buttons, axis, hats */
   elements_raw = IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);
   count        = (int)CFArrayGetCount(elements_raw);
   elements     = CFArrayCreateMutableCopy(
         kCFAllocatorDefault,(CFIndex)count,elements_raw);
   range        = CFRangeMake(0,count);

   CFRelease(elements_raw);

   CFArraySortValues(elements,
         range, iohidmanager_sort_elements, NULL);

   for (i = 0; i < count; i++)
   {
      IOHIDElementType type;
      uint32_t page, use, cookie;
      int detected_button     = 0;
      IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);

      if (!element)
         continue;

      type                    = IOHIDElementGetType(element);
      page                    = (uint32_t)IOHIDElementGetUsagePage(element);
      use                     = (uint32_t)IOHIDElementGetUsage(element);
      cookie                  = (uint32_t)IOHIDElementGetCookie(element);

      switch (page)
      {
         case kHIDPage_GenericDesktop:
            switch (type)
            {
               case kIOHIDElementTypeInput_Misc:
                  switch (use)
                  {
                     case kHIDUsage_GD_Hatswitch:
                        {
                           /* as far as I can tell, OSX only reports one Hat */
                           apple_input_rec_t *hat = (apple_input_rec_t *)malloc(sizeof(apple_input_rec_t));
                           hat->id                = 0;
                           hat->cookie            = (IOHIDElementCookie)cookie;
                           hat->next              = NULL;
                           adapter->hats          = hat;
                        }
                        break;
                     default:
                        {
                           uint32_t i = 0;

                           while (i < 11 && axis_use_ids[i] != use)
                              i++;

                           if (i < 11)
                           {

                              apple_input_rec_t *axis = (apple_input_rec_t *)malloc(sizeof(apple_input_rec_t));
                              axis->id                = i;
                              axis->cookie            = (IOHIDElementCookie)cookie;
                              axis->next              = NULL;

                              if (iohidmanager_check_for_id(adapter->axes,i))
                              {
                                 /* axis ID already exists, save to tmp for appending later */
                                 if (tmpAxes)
                                    iohidmanager_append_record(tmpAxes, axis);
                                 else
                                    tmpAxes           = axis;
                              }
                              else
                              {
                                 found_axis[axis->id] = true;
                                 if (adapter->axes)
                                    iohidmanager_append_record(adapter->axes, axis);
                                 else
                                    adapter->axes     = axis;
                              }
                           }
                           else
                              detected_button = 1;
                        }
                        break;
                  }
                  break;
               default:
                  /* TODO/FIXME */
                  break;
            }
            break;
         case kHIDPage_Consumer:
         case kHIDPage_Button:
            switch (type)
            {
               case kIOHIDElementTypeInput_Misc:
               case kIOHIDElementTypeInput_Button:
                  detected_button = 1;
                  break;
               default:
                  /* TODO/FIXME */
                  break;
            }
            break;

         case kHIDPage_Simulation:
             switch (use)
             {
                 default:
                 {
                     uint32_t i = 0;

                     while (i < 11 && axis_use_ids[i] != use)
                        i++;

                     if (i < 11)
                     {
                        apple_input_rec_t *axis = (apple_input_rec_t *)malloc(sizeof(apple_input_rec_t));
                        axis->id                = i;
                        axis->cookie            = (IOHIDElementCookie)cookie;
                        axis->next              = NULL;

                        if (iohidmanager_check_for_id(adapter->axes,i))
                        {
                           /* axis ID already exists, save to tmp for appending later */
                           if (tmpAxes)
                              iohidmanager_append_record(tmpAxes, axis);
                           else
                              tmpAxes           = axis;
                        }
                        else
                        {
                           found_axis[axis->id] = true;
                           if (adapter->axes)
                              iohidmanager_append_record(adapter->axes, axis);
                           else
                              adapter->axes     = axis;
                        }
                     }
                     else
                        detected_button = 1;
                     }
                     break;
                }
                break;
      }

      if (detected_button)
      {
         apple_input_rec_t *btn = (apple_input_rec_t *)malloc(sizeof(apple_input_rec_t));
         btn->id                = (uint32_t)use;
         btn->cookie            = (IOHIDElementCookie)cookie;
         btn->next              = NULL;

         if (iohidmanager_check_for_id(adapter->buttons,btn->id))
         {
            if (tmpButtons)
               iohidmanager_append_record_ordered(&tmpButtons, btn);
            else
               tmpButtons = btn;
         }
         else
         {
            if (adapter->buttons)
               iohidmanager_append_record_ordered(&adapter->buttons, btn);
            else
               adapter->buttons = btn;
         }
      }
   }

   CFRelease(elements);

   /* take care of buttons/axes with duplicate 'use' values */
   for (i = 0; i < 11; i++)
   {
      if (!found_axis[i] && tmpAxes)
      {
         apple_input_rec_t *next = tmpAxes->next;
         tmpAxes->id             = i;
         tmpAxes->next           = NULL;
         iohidmanager_append_record(adapter->axes, tmpAxes);
         tmpAxes                 = next;
      }
   }

   tmp = adapter->buttons;

   if (tmp)
   {
      while (tmp->next)
         tmp = tmp->next;
   }

   while (tmpButtons)
   {
      apple_input_rec_t *next = tmpButtons->next;

      tmpButtons->id          = tmp->id;
      tmpButtons->next        = NULL;
      tmp->next               = tmpButtons;

      tmp                     = tmp->next;
      tmpButtons              = next;
   }

   iohidmanager_hid_device_add_autodetect(adapter->slot,
         adapter->name, iohidmanager_hid.ident, dev_vid, dev_pid);
   return;

error:
   {
      apple_input_rec_t *tmp = NULL;
      while (adapter->hats != NULL)
      {
         tmp              = adapter->hats;
         adapter->hats    = adapter->hats->next;
         free(tmp);
      }
      while (adapter->axes != NULL)
      {
         tmp              = adapter->axes;
         adapter->axes    = adapter->axes->next;
         free(tmp);
      }
      while (adapter->buttons != NULL)
      {
         tmp              = adapter->buttons;
         adapter->buttons = adapter->buttons->next;
         free(tmp);
      }
      while (tmpAxes != NULL)
      {
         tmp              = tmpAxes;
         tmpAxes          = tmpAxes->next;
         free(tmp);
      }
      while (tmpButtons != NULL)
      {
         tmp              = tmpButtons;
         tmpButtons       = tmpButtons->next;
         free(tmp);
      }
      free(adapter);
   }
}


static void iohidmanager_hid_device_matched(void *data, IOReturn result,
                                            void* sender, IOHIDDeviceRef device)
{
   iohidmanager_hid_t *hid = (iohidmanager_hid_t*) hid_driver_get_data();
   
   iohidmanager_hid_device_add(device, hid);
}
static void iohidmanager_hid_device_removed(void *data, IOReturn result,
                                            void* sender, IOHIDDeviceRef device)
{
   iohidmanager_hid_t *hid = (iohidmanager_hid_t*) hid_driver_get_data();
   iohidmanager_hid_device_remove(device, hid);
}


static void iohidmanager_hid_append_matching_dictionary(
      CFMutableArrayRef array,
      uint32_t page, uint32_t use)
{
   CFMutableDictionaryRef matcher = CFDictionaryCreateMutable(
         kCFAllocatorDefault, 0,
         &kCFTypeDictionaryKeyCallBacks,
         &kCFTypeDictionaryValueCallBacks);
   CFNumberRef pagen = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
   CFNumberRef usen  = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &use);

   CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsagePageKey), pagen);
   CFDictionarySetValue(matcher, CFSTR(kIOHIDDeviceUsageKey), usen);
   CFArrayAppendValue(array, matcher);

   CFRelease(pagen);
   CFRelease(usen);
   CFRelease(matcher);
}

static int iohidmanager_hid_manager_init(iohidmanager_hid_t *hid)
{
   if (!hid)
      return -1;
   if (hid->ptr) /* already initialized. */
      return 0;

   hid->ptr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

   if (!hid->ptr)
      return -1;
   IOHIDManagerSetDeviceMatching(hid->ptr, NULL);
   IOHIDManagerScheduleWithRunLoop(hid->ptr, CFRunLoopGetCurrent(),
         kCFRunLoopDefaultMode);
   return 0;
}

static int iohidmanager_hid_manager_free(iohidmanager_hid_t *hid)
{
   if (!hid || !hid->ptr)
      return -1;

   IOHIDManagerUnscheduleFromRunLoop(hid->ptr,
         CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
   IOHIDManagerClose(hid->ptr, kIOHIDOptionsTypeNone);
   CFRelease(hid->ptr);
   hid->ptr = NULL;

   return 0;
}

static int iohidmanager_hid_manager_set_device_matching(
      iohidmanager_hid_t *hid)
{
   /* register call back to dynamically add device plugged when retroarch is
    * running
    * those will be added after the one plugged when retroarch was launched,
    * and by order they are plugged in (so not deterministic) */
   CFMutableArrayRef matcher = CFArrayCreateMutable(kCFAllocatorDefault, 0,
      &kCFTypeArrayCallBacks);

   if (!matcher)
      return -1;

   iohidmanager_hid_append_matching_dictionary(matcher,
      kHIDPage_GenericDesktop,
      kHIDUsage_GD_Joystick);
   iohidmanager_hid_append_matching_dictionary(matcher,
      kHIDPage_GenericDesktop,
      kHIDUsage_GD_GamePad);
   /* The GameCube Adapter reports usage id 0x00 */
   iohidmanager_hid_append_matching_dictionary(matcher, kHIDPage_Game, 0x00);
   
   IOHIDManagerSetDeviceMatchingMultiple(hid->ptr, matcher);
   IOHIDManagerRegisterDeviceMatchingCallback(hid->ptr,iohidmanager_hid_device_matched, 0);
   IOHIDManagerRegisterDeviceRemovalCallback(hid->ptr,iohidmanager_hid_device_removed, 0);
   
   CFRelease(matcher);


   return 0;
}

static void *iohidmanager_hid_init(void)
{
   iohidmanager_hid_t *hid_apple = (iohidmanager_hid_t*)
      calloc(1, sizeof(*hid_apple));

   if (!hid_apple)
      return NULL;

   hid_apple->slots = pad_connection_init(MAX_USERS);

   if (!hid_apple->slots)
      goto error;
   if (iohidmanager_hid_manager_init(hid_apple) == -1)
      goto error;
   if (iohidmanager_hid_manager_set_device_matching(hid_apple) == -1)
      goto error;

   return hid_apple;

error:
   if (hid_apple->slots)
      free(hid_apple->slots);
   hid_apple->slots = NULL;
   if (hid_apple)
      free(hid_apple);
   return NULL;
}

static void iohidmanager_hid_free(const void *data)
{
   iohidmanager_hid_t *hid_apple = (iohidmanager_hid_t*)data;

   if (!hid_apple || !hid_apple->ptr)
      return;

   pad_connection_destroy(hid_apple->slots);
   iohidmanager_hid_manager_free(hid_apple);

   if (hid_apple)
      free(hid_apple);
}

static void iohidmanager_hid_poll(void *data)
{
   (void)data;
}

static int32_t iohidmanager_set_report(void *handle, uint8_t report_type, uint8_t report_id, uint8_t *data_buf, size_t size)
{
   struct iohidmanager_hid_adapter *adapter =
      (struct iohidmanager_hid_adapter*)handle;

   if (adapter)
      return IOHIDDeviceSetReport(adapter->handle, translate_hid_report_type(report_type), report_type, data_buf, size);

   return -1;
}

static int32_t iohidmanager_get_report(void *handle, uint8_t report_type, uint8_t report_id, uint8_t *data_buf, size_t size)
{
   struct iohidmanager_hid_adapter *adapter =
      (struct iohidmanager_hid_adapter*)handle;

   if (adapter)
   {
      CFIndex length = size;
      return IOHIDDeviceGetReport(adapter->handle, translate_hid_report_type(report_type), report_id, data_buf, &length);
   }

   return -1;
}

hid_driver_t iohidmanager_hid = {
   iohidmanager_hid_init,
   iohidmanager_hid_joypad_query,
   iohidmanager_hid_free,
   iohidmanager_hid_joypad_button,
   iohidmanager_hid_joypad_state,
   iohidmanager_hid_joypad_get_buttons,
   iohidmanager_hid_joypad_axis,
   iohidmanager_hid_poll,
   iohidmanager_hid_joypad_rumble,
   iohidmanager_hid_joypad_name,
   "iohidmanager",
   iohidmanager_hid_device_send_control,
   iohidmanager_set_report,
   iohidmanager_get_report,
   NULL, /* set_idle */
   NULL, /* set protocol */
   NULL  /* read */
};
