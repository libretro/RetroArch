/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Sergi Granell (xerpi)
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <gccore.h>
#include <rthreads/rthreads.h>

#include "../input_defines.h"
#include "../input_driver.h"

#include "../connect/joypad_connection.h"

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

#define WIIUSB_SC_NONE     0
#define WIIUSB_SC_INTMSG   1
#define WIIUSB_SC_CTRLMSG  2
#define WIIUSB_SC_CTRLMSG2 3

typedef struct wiiusb_hid
{
   joypad_connection_t   *connections;
   struct wiiusb_adapter *adapters_head;

   sthread_t *poll_thread;
   volatile bool poll_thread_quit;

   /* helps on knowing if a new device has been inserted */
   bool device_detected;
   /* helps on detecting that a device has just been removed */
   bool removal_cb;

   bool manual_removal;
} wiiusb_hid_t;

struct wiiusb_adapter
{
   wiiusb_hid_t *hid;
   struct wiiusb_adapter *next;

   bool busy;
   int32_t device_id;
   int32_t handle;
   int32_t endpoint_in;
   int32_t endpoint_out;
   int32_t endpoint_in_max_size;
   int32_t endpoint_out_max_size;

   int32_t slot;
   uint8_t *data;

   uint8_t  send_control_type;
   uint8_t *send_control_buffer;
   uint32_t send_control_size;
};

static void wiiusb_hid_process_control_message(struct wiiusb_adapter* adapter)
{
   int32_t r;
   switch (adapter->send_control_type)
   {
      case WIIUSB_SC_INTMSG:
         do
         {
            r = USB_WriteIntrMsg(adapter->handle,
               adapter->endpoint_out, adapter->send_control_size,
               adapter->send_control_buffer);
         } while (r < 0);
         break;
      case WIIUSB_SC_CTRLMSG:
         do
         {
            r = USB_WriteCtrlMsg(adapter->handle, USB_REQTYPE_INTERFACE_SET,
               USB_REQ_SETREPORT, (USB_REPTYPE_FEATURE<<8) | 0xf4, 0x0,
               adapter->send_control_size, adapter->send_control_buffer);
         } while (r < 0);
         break;
      case WIIUSB_SC_CTRLMSG2:
         do
         {
            r = USB_WriteCtrlMsg(adapter->handle, USB_REQTYPE_INTERFACE_SET,
                  USB_REQ_SETREPORT, (USB_REPTYPE_OUTPUT<<8) | 0x01, 0x0,
                  adapter->send_control_size, adapter->send_control_buffer);
         } while (r < 0);
         break;
      /*default:  any other case we do nothing */
   }
   /* Reset the control type */
   adapter->send_control_type = WIIUSB_SC_NONE;
}

static int32_t wiiusb_hid_read_cb(int32_t size, void *data)
{
   struct wiiusb_adapter *adapter = (struct wiiusb_adapter*)data;
   wiiusb_hid_t *hid = adapter ? adapter->hid : NULL;

   if (hid && hid->connections && size > 0)
      pad_connection_packet(&hid->connections[adapter->slot],
            adapter->slot, adapter->data-1, size+1);

  if (adapter)
      adapter->busy = false;

  return size;
}

static void wiiusb_hid_device_send_control(void *data,
      uint8_t* data_buf, size_t size)
{
   uint8_t control_type;
   struct wiiusb_adapter *adapter = (struct wiiusb_adapter*)data;
   if (!adapter || !data_buf || !adapter->send_control_buffer)
      return;

   /* first byte contains the type of control to use
    * which can be NONE, INT_MSG, CTRL_MSG, CTRL_MSG2 */
   control_type = data_buf[0];
   /* decrement size by one as we are getting rid of first byte */
   adapter->send_control_size = size - 1;
   /* increase the buffer address so we access the actual data */
   data_buf++;
   memcpy(adapter->send_control_buffer, data_buf,  adapter->send_control_size);
   /* Activate it so it can be processed in the adapter thread */
   adapter->send_control_type = control_type;
}

static void wiiusb_hid_device_add_autodetect(unsigned idx,
      const char *device_name, const char *driver_name,
      uint16_t dev_vid, uint16_t dev_pid)
{
   input_autoconfigure_connect(
         device_name,
         NULL,
         "hid",
         idx,
         dev_vid,
         dev_pid);
}

static void wiiusb_get_description(usb_device_entry *device,
      struct wiiusb_adapter *adapter, usb_devdesc *devdesc)
{
   unsigned char c;
   unsigned i, k;

   for (c = 0; c < devdesc->bNumConfigurations; c++)
   {
      const usb_configurationdesc *config = &devdesc->configurations[c];

      for (i = 0; i < (int)config->bNumInterfaces; i++)
      {
         const usb_interfacedesc *inter = &config->interfaces[i];

         for (k = 0; k < (int)inter->bNumEndpoints; k++)
         {
            const usb_endpointdesc *epdesc = &inter->endpoints[k];
            bool is_int = (epdesc->bmAttributes & 0x03)     == USB_ENDPOINT_INTERRUPT;
            bool is_out = (epdesc->bEndpointAddress & 0x80) == USB_ENDPOINT_OUT;
            bool is_in  = (epdesc->bEndpointAddress & 0x80) == USB_ENDPOINT_IN;

            if (is_int)
            {
               if (is_in)
               {
                  adapter->endpoint_in = epdesc->bEndpointAddress;
                  adapter->endpoint_in_max_size = epdesc->wMaxPacketSize;
               }
               if (is_out)
               {
                  adapter->endpoint_out = epdesc->bEndpointAddress;
                  adapter->endpoint_out_max_size = epdesc->wMaxPacketSize;
               }
            }
         }
         break;
      }
   }
}

static const char *wiiusb_hid_joypad_name(void *data, unsigned pad)
{
   wiiusb_hid_t *hid = (wiiusb_hid_t*)data;
   if (pad >= MAX_USERS)
      return NULL;

   if (hid)
      return pad_connection_get_name(&hid->connections[pad], pad);

   return NULL;
}

static int32_t wiiusb_hid_release_adapter(struct wiiusb_adapter *adapter)
{
   wiiusb_hid_t *hid = NULL;
   const char *name  = NULL;

   if (!adapter)
      return -1;

   hid  = adapter->hid;
   name = wiiusb_hid_joypad_name(hid, adapter->slot);

   input_autoconfigure_disconnect(adapter->slot, name);

   pad_connection_pad_deinit(&hid->connections[adapter->slot], adapter->slot);

   free(adapter->send_control_buffer);
   free(adapter->data);
   free(adapter);

   return 0;
}

static int wiiusb_hid_remove_adapter(struct wiiusb_adapter *adapter)
{
   if (!adapter)
      return -1;

   if (adapter->handle > 0)
      USB_CloseDevice(&adapter->handle);

   wiiusb_hid_release_adapter(adapter);

   return 0;
}

static int wiiusb_hid_removal_cb(int result, void *usrdata)
{
   struct wiiusb_adapter *adapter = (struct wiiusb_adapter *)usrdata;
   wiiusb_hid_t *hid              = adapter ? adapter->hid       : NULL;
   struct wiiusb_adapter *temp    = hid     ? hid->adapters_head : NULL;

   if (!adapter || !hid || !temp || hid->manual_removal)
      return -1;

   if (temp == adapter)
      hid->adapters_head = adapter->next;
   else
      while (temp->next)
      {
         if (temp->next == adapter)
         {
            temp->next = adapter->next;
            break;
         }
         temp = temp->next;
      }

   /* get rid of the adapter */
   wiiusb_hid_release_adapter(adapter);

   /* notify that we pass thru the removal callback */
   hid->removal_cb = true;

   return 0;
}

static int wiiusb_hid_add_adapter(void *data, usb_device_entry *dev)
{
   usb_devdesc desc;
   const char        *device_name = NULL;
   wiiusb_hid_t              *hid = (wiiusb_hid_t*)data;
   struct wiiusb_adapter *adapter = (struct wiiusb_adapter*)
      calloc(1, sizeof(struct wiiusb_adapter));

   if (!adapter)
      return -1;

   if (!hid)
   {
      free(adapter);
      RARCH_ERR("Allocation of adapter failed.\n");
      return -1;
   }

   if (USB_OpenDevice(dev->device_id, dev->vid, dev->pid, &adapter->handle) < 0)
   {
      RARCH_ERR("Error opening device 0x%p (VID/PID: %04x:%04x).\n",
           dev->device_id, dev->vid, dev->pid);
      free(adapter);
      return -1;
   }

   adapter->device_id = dev->device_id;

   USB_GetDescriptors(adapter->handle, &desc);

   wiiusb_get_description(dev, adapter, &desc);

   if (adapter->endpoint_in == 0)
   {
      RARCH_ERR("Could not find HID config for device.\n");
      goto error;
   }

   /* Allocate mem for the send control buffer, 32bit aligned */
   adapter->send_control_type   = WIIUSB_SC_NONE;
   adapter->send_control_buffer = memalign(32, 128);

   if (!adapter->send_control_buffer)
   {
      RARCH_ERR("Error creating send control buffer.\n");
      goto error;
   }

   /* Sent the pad name as dummy, we don't know the
    * control name until we get its interface */
   adapter->slot = pad_connection_pad_init(hid->connections,
         "hid", desc.idVendor, desc.idProduct,
         adapter, &wiiusb_hid);

   if (adapter->slot == -1)
      goto error;

   if (!pad_connection_has_interface(hid->connections, adapter->slot))
   {
      RARCH_ERR(" Interface not found.\n");
      goto error;
   }

   adapter->data      = memalign(32, 128);
   adapter->hid       = hid;
   adapter->next      = hid->adapters_head;
   hid->adapters_head = adapter;

   /*  Get the name from the interface */
   device_name = wiiusb_hid_joypad_name(hid, adapter->slot);

   RARCH_LOG("Interface found: [%s].\n", device_name);

   RARCH_LOG("Device 0x%p attached (VID/PID: %04x:%04x).\n",
         adapter->device_id, desc.idVendor, desc.idProduct);

   wiiusb_hid_device_add_autodetect(adapter->slot,
         device_name, wiiusb_hid.ident, desc.idVendor, desc.idProduct);

   USB_FreeDescriptors(&desc);
   USB_DeviceRemovalNotifyAsync(adapter->handle, wiiusb_hid_removal_cb, adapter);

   return 0;

error:
   if (adapter->send_control_buffer)
      free(adapter->send_control_buffer);
   if (adapter)
      free(adapter);
   USB_FreeDescriptors(&desc);
   USB_CloseDevice(&adapter->handle);
   return -1;
}

static bool wiiusb_hid_new_device(wiiusb_hid_t *hid, int32_t id)
{
   struct wiiusb_adapter *temp;

   if (!hid)
      return false; /* false, so we do not proceed to add it */

   temp = hid->adapters_head;
   while (temp)
   {
      if (temp->device_id == id)
         return false;

      temp = temp->next;
   }

   return true;
}

static void wiiusb_hid_scan_for_devices(wiiusb_hid_t *hid)
{
   unsigned i;
   u8 count;
   usb_device_entry *dev_entries = (usb_device_entry *)
      calloc(MAX_USERS, sizeof(*dev_entries));

   if (!dev_entries)
      goto error;

   if (USB_GetDeviceList(dev_entries, MAX_USERS, USB_CLASS_HID, &count) < 0)
      goto error;

   for (i = 0; i < count; i++)
   {
    /* first check the device is not already in our list */
      if (!wiiusb_hid_new_device(hid, dev_entries[i].device_id))
         continue;

      if (dev_entries[i].vid > 0 && dev_entries[i].pid > 0)
         wiiusb_hid_add_adapter(hid, &dev_entries[i]);
   }

error:
   if (dev_entries)
      free(dev_entries);
}

static void wiiusb_hid_poll_thread(void *data)
{
   wiiusb_hid_t              *hid = (wiiusb_hid_t*)data;
   struct wiiusb_adapter *adapter = NULL;

   if (!hid)
      return;

   while (!hid->poll_thread_quit)
   {

      /* first check for new devices */
      if (hid->device_detected)
      {
         /* turn off the detection flag */
         hid->device_detected = false;
         /* search for new pads and add them as needed */
         wiiusb_hid_scan_for_devices(hid);
      }

      /* process each active adapter */
      for (adapter = hid->adapters_head; adapter; adapter=adapter->next)
      {
         if (adapter->busy)
            continue;

         /* lock itself while writing or reading */
         adapter->busy = true;

         if (adapter->send_control_type)
            wiiusb_hid_process_control_message(adapter);

         USB_ReadIntrMsgAsync(adapter->handle, adapter->endpoint_in,
               adapter->endpoint_in_max_size,
               adapter->data, wiiusb_hid_read_cb, adapter);
      }

      /* Wait 10 milliseconds to process again */
      usleep(10000);
   }
}

static int wiiusb_hid_change_cb(int result, void *usrdata)
{
   wiiusb_hid_t *hid = (wiiusb_hid_t*)usrdata;

   if (!hid)
      return -1;

   /* As it's not coming from the removal callback
      then we detected a new device being inserted */
  if (!hid->removal_cb)
    hid->device_detected = true;
  else
    hid->removal_cb      = false;

   /* Re-submit the change alert */
   USB_DeviceChangeNotifyAsync(USB_CLASS_HID, wiiusb_hid_change_cb, usrdata);

   return 0;
}

static bool wiiusb_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static void wiiusb_hid_joypad_get_buttons(void *data,
      unsigned port, input_bits_t *state)
{
  wiiusb_hid_t *hid = (wiiusb_hid_t*)data;
  if (hid)
  {
    pad_connection_get_buttons(&hid->connections[port], port, state);
    return;
  }
  BIT256_CLEAR_ALL_PTR(state);
}

static int16_t wiiusb_hid_joypad_button(void *data,
      unsigned port, uint16_t joykey)
{
   input_bits_t buttons;

   if (port >= DEFAULT_MAX_PADS)
      return 0;
   wiiusb_hid_joypad_get_buttons(data, port, &buttons);

   /* Check hat. */
   if (GET_HAT_DIR(joykey))
      return 0;
   else if (joykey < 32)
      return (BIT256_GET(buttons, joykey) != 0);
   return 0;
}

static int16_t wiiusb_hid_joypad_axis(void *data,
      unsigned port, uint32_t joyaxis)
{
   wiiusb_hid_t *hid = (wiiusb_hid_t*)data;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      int16_t val = pad_connection_get_axis(&hid->connections[port],
            port, AXIS_NEG_GET(joyaxis));

      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      int16_t val = pad_connection_get_axis(&hid->connections[port],
            port, AXIS_POS_GET(joyaxis));

      if (val > 0)
         return val;
   }
   return 0;
}


static int16_t wiiusb_hid_joypad_state(
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
            && wiiusb_hid_joypad_button(data, port_idx, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(wiiusb_hid_joypad_axis(data, port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static bool wiiusb_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   wiiusb_hid_t *hid  = (wiiusb_hid_t*)data;

   if (!hid)
      return false;

   return pad_connection_rumble(&hid->connections[pad], pad, effect, strength);
}

static void wiiusb_hid_free(const void *data)
{
   struct wiiusb_adapter      *adapter = NULL;
   struct wiiusb_adapter *next_adapter = NULL;
   wiiusb_hid_t                   *hid = (wiiusb_hid_t*)data;

   if (!hid)
      return;

   hid->poll_thread_quit = true;

   if (hid->poll_thread)
      sthread_join(hid->poll_thread);

   hid->manual_removal   = TRUE;

   /* remove each of the adapters */
   for (adapter = hid->adapters_head; adapter; adapter = next_adapter)
   {
      next_adapter = adapter->next;
      wiiusb_hid_remove_adapter(adapter);
   }

   pad_connection_destroy(hid->connections);

   free(hid);
}

static void *wiiusb_hid_init(void)
{
   joypad_connection_t *connections = NULL;
   wiiusb_hid_t                *hid = (wiiusb_hid_t*)calloc(1, sizeof(*hid));

   if (!hid)
      goto error;

   connections = pad_connection_init(MAX_USERS);

   if (!connections)
      goto error;

   /* Initialize HID values */
   hid->connections      = connections;
   hid->adapters_head    = NULL;
   hid->removal_cb       = FALSE;
   hid->manual_removal   = FALSE;
   hid->poll_thread_quit = FALSE;
   /* we set it initially to TRUE so we force
    * to add the already connected pads */
   hid->device_detected  = TRUE;

   hid->poll_thread      = sthread_create(wiiusb_hid_poll_thread, hid);

   if (!hid->poll_thread)
   {
      RARCH_ERR("Error initializing poll thread.\n");
      goto error;
   }

   USB_DeviceChangeNotifyAsync(USB_CLASS_HID, wiiusb_hid_change_cb, (void *)hid);

   return hid;

error:
   wiiusb_hid_free(hid);
   return NULL;
}

static void wiiusb_hid_poll(void *data)
{
   (void)data;
}

hid_driver_t wiiusb_hid = {
   wiiusb_hid_init,
   wiiusb_hid_joypad_query,
   wiiusb_hid_free,
   wiiusb_hid_joypad_button,
   wiiusb_hid_joypad_state,
   wiiusb_hid_joypad_get_buttons,
   wiiusb_hid_joypad_axis,
   wiiusb_hid_poll,
   wiiusb_hid_joypad_rumble,
   wiiusb_hid_joypad_name,
   "wiiusb",
   wiiusb_hid_device_send_control,
};
