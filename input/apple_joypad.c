/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include "apple_input.h"
#include "input_common.h"
#include "../general.h"

#ifdef IOS
#include "../apple/iOS/bluetooth/btdynamic.c"
#include "../apple/iOS/bluetooth/btpad.c"
#include "../apple/iOS/bluetooth/btpad_queue.c"
#elif defined(OSX)
#include <IOKit/hid/IOHIDManager.h>
#endif

#ifdef OSX
struct apple_pad_connection
{
    uint32_t slot;
    IOHIDDeviceRef device;
    uint8_t data[2048];
};

static IOHIDManagerRef g_hid_manager;

static void apple_pad_send_control(struct apple_pad_connection* connection,
      uint8_t* data, size_t size)
{
    IOHIDDeviceSetReport(connection->device,
          kIOHIDReportTypeOutput, 0x01, data + 1, size - 1);
}

/* NOTE: I pieced this together through trial and error, 
 * any corrections are welcome. */

static void hid_device_input_callback(void* context, IOReturn result,
      void* sender, IOHIDValueRef value)
{
    IOHIDElementRef element;
    uint32_t type, page, use;
    struct apple_pad_connection* connection = (struct apple_pad_connection*)
       context;
    
    element = IOHIDValueGetElement(value);
    type    = IOHIDElementGetType(element);
    page    = IOHIDElementGetUsagePage(element);
    use     = IOHIDElementGetUsage(element);
    
    /* Joystick handler.
     * TODO: Can GamePad work the same? */
    if (type == kIOHIDElementTypeInput_Button && page == kHIDPage_Button)
    {
        CFIndex state = IOHIDValueGetIntegerValue(value);
        
        if (state)
            g_current_input_data.pad_buttons[connection->slot] |= (1 << (use - 1));
        else
            g_current_input_data.pad_buttons[connection->slot] &= ~(1 << (use - 1));
    }
    else if (type == kIOHIDElementTypeInput_Misc && page == kHIDPage_GenericDesktop)
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
        g_current_input_data.pad_buttons[connection->slot] = 0;
        memset(g_current_input_data.pad_axis[connection->slot],
              0, sizeof(g_current_input_data.pad_axis));
        
        apple_joypad_disconnect(connection->slot);
        free(connection);
    }
    
    IOHIDDeviceClose(sender, kIOHIDOptionsTypeNone);
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
    char device_name[1024];
    CFStringRef device_name_ref;
    struct apple_pad_connection* connection = (struct apple_pad_connection*)
       calloc(1, sizeof(*connection));
    
    connection->device = device;
    connection->slot = MAX_PLAYERS;
    
    IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
    IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(),
          kCFRunLoopCommonModes);
    IOHIDDeviceRegisterRemovalCallback(device, hid_device_removed, connection);
    
    device_name_ref = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
    CFStringGetCString(device_name_ref, device_name,
          sizeof(device_name), kCFStringEncodingUTF8);
    
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
#endif

#include "wiimote.c"
#include "apple_joypad_ps3.c"
#include "apple_joypad_wii.c"

typedef struct
{
   bool used;
   struct apple_pad_interface* iface;
   void* data;
   
   bool is_gcapi;
} joypad_slot_t;

static joypad_slot_t slots[MAX_PLAYERS];

static int32_t find_empty_slot(void)
{
   unsigned i;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (!slots[i].used)
      {
         memset(&slots[i], 0, sizeof(slots[0]));
         return i;
      }
   }
   return -1;
}

int32_t apple_joypad_connect(const char* name,
      struct apple_pad_connection* connection)
{
   int32_t slot;
   slot = find_empty_slot();

   if (slot >= 0 && slot < MAX_PLAYERS)
   {
      unsigned i;
      joypad_slot_t* s = (joypad_slot_t*)&slots[slot];
      s->used = true;

      static const struct
      {
         const char* name;
         struct apple_pad_interface* iface;
      } pad_map[] = 
      {
         { "Nintendo RVL-CNT-01",         &apple_pad_wii },
         { "PLAYSTATION(R)3 Controller",  &apple_pad_ps3 },
         { 0, 0}
      };

      for (i = 0; name && pad_map[i].name; i++)
         if (strstr(name, pad_map[i].name))
         {
            s->iface = pad_map[i].iface;
            s->data = s->iface->connect(connection, slot);
         }
   }

   return slot;
}

int32_t apple_joypad_connect_gcapi(void)
{
   int32_t slot;
   slot = find_empty_slot();

   if (slot >= 0 && slot < MAX_PLAYERS)
   {
      joypad_slot_t *s = (joypad_slot_t*)&slots[slot];
      s->used = true;
      s->is_gcapi = true;
   }

   return slot;
}

void apple_joypad_disconnect(uint32_t slot)
{
   if (slot < MAX_PLAYERS && slots[slot].used)
   {
      joypad_slot_t* s = (joypad_slot_t*)&slots[slot];

      if (s->iface && s->data && s->iface->disconnect)
         s->iface->disconnect(s->data);

      memset(s, 0, sizeof(joypad_slot_t));
   }
}

void apple_joypad_packet(uint32_t slot, uint8_t* data, uint32_t length)
{
   if (slot < MAX_PLAYERS && slots[slot].used)
   {
      joypad_slot_t *s = (joypad_slot_t*)&slots[slot];

      if (s->iface && s->data && s->iface->packet_handler)
         s->iface->packet_handler(s->data, data, length);
   }
}

bool apple_joypad_has_interface(uint32_t slot)
{
   if (slot < MAX_PLAYERS && slots[slot].used)
      return slots[slot].iface ? true : false;

   return false;
}

// RetroArch joypad driver:
static bool apple_joypad_init(void)
{
#ifdef OSX
    CFMutableArrayRef matcher;
    
    if (!g_hid_manager)
    {
        g_hid_manager = IOHIDManagerCreate(
              kCFAllocatorDefault, kIOHIDOptionsTypeNone);
        
        matcher = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        append_matching_dictionary(matcher, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
        append_matching_dictionary(matcher, kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad);
        
        IOHIDManagerSetDeviceMatchingMultiple(g_hid_manager, matcher);
        CFRelease(matcher);
        
        IOHIDManagerRegisterDeviceMatchingCallback(g_hid_manager, hid_manager_device_attached, 0);
        IOHIDManagerScheduleWithRunLoop(g_hid_manager, CFRunLoopGetMain(), kCFRunLoopCommonModes);
        
        IOHIDManagerOpen(g_hid_manager, kIOHIDOptionsTypeNone);
    }
#endif
   return true;
}

static bool apple_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS;
}

static void apple_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < MAX_PLAYERS; i ++)
   {
      if (slots[i].used && slots[i].iface && slots[i].iface->set_rumble)
      {
         slots[i].iface->set_rumble(slots[i].data, RETRO_RUMBLE_STRONG, 0);
         slots[i].iface->set_rumble(slots[i].data, RETRO_RUMBLE_WEAK, 0);
      }
   }

#ifdef OSX
    if (g_hid_manager)
    {
        IOHIDManagerClose(g_hid_manager, kIOHIDOptionsTypeNone);
        IOHIDManagerUnscheduleFromRunLoop(g_hid_manager,
              CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
        
        CFRelease(g_hid_manager);
    }
    g_hid_manager = NULL;
#endif
}

static bool apple_joypad_button(unsigned port, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
      return false;
   // Check the button
   return (port < MAX_PLAYERS && joykey < 32) ? 
      (g_current_input_data.pad_buttons[port] & (1 << joykey)) != 0 : false;
}

static int16_t apple_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int16_t val;

   if (joyaxis == AXIS_NONE)
      return 0;

   val = 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      val = g_current_input_data.pad_axis[port][AXIS_NEG_GET(joyaxis)];
      val = (val < 0) ? val : 0;
   }
   else if(AXIS_POS_GET(joyaxis) < 4)
   {
      val = g_current_input_data.pad_axis[port][AXIS_POS_GET(joyaxis)];
      val = (val > 0) ? val : 0;
   }

   return val;
}

static void apple_joypad_poll(void)
{
}

static bool apple_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   if (pad < MAX_PLAYERS && slots[pad].used && slots[pad].iface
       && slots[pad].iface->set_rumble)
   {
      slots[pad].iface->set_rumble(slots[pad].data, effect, strength);
      return true;
   }

   return false;
}

static const char *apple_joypad_name(unsigned joypad)
{
   (void)joypad;
   return NULL;
}

const rarch_joypad_driver_t apple_joypad = {
   apple_joypad_init,
   apple_joypad_query_pad,
   apple_joypad_destroy,
   apple_joypad_button,
   apple_joypad_axis,
   apple_joypad_poll,
   apple_joypad_rumble,
   apple_joypad_name,
   "apple"
};
