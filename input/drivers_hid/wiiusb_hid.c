/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015 - Sergi Granell (xerpi)
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

#include <gccore.h>

#include <rthreads/rthreads.h>
#include <compat/strl.h>
#include <queues/fifo_buffer.h>

#include "../connect/joypad_connection.h"
#include "../input_autodetect.h"
#include "../input_hid_driver.h"
#include "../../verbosity.h"

typedef struct wiiusb_hid
{
   joypad_connection_t *slots;
   int hp; /* wiiusb_hotplug_callback_handle is just int */
   int quit;
} wiiusb_hid_t;

struct wiiusb_adapter
{
   wiiusb_hid_t *hid;
   volatile bool quitting;
   usb_device_entry device;
   int handle;
   int interface_number;
   int endpoint_in;
   int endpoint_out;
   int endpoint_in_max_size;
   int endpoint_out_max_size;

   uint8_t manufacturer_name[255];
   uint8_t name[255];
   uint8_t *data;

   int slot;

   sthread_t *thread;
   slock_t *send_control_lock;
   fifo_buffer_t *send_control_buffer;
   struct wiiusb_adapter *next;
};

static struct wiiusb_adapter adapters;

static void adapter_thread(void *data)
{
   uint8_t __attribute__((aligned(32))) send_command_buf[4096];
   struct wiiusb_adapter *adapter = (struct wiiusb_adapter*)data;
   wiiusb_hid_t *hid = adapter ? adapter->hid : NULL;

   if (!adapter)
      return;

   while (!adapter->quitting)
   {
      size_t send_command_size;
      int size = 0;

      slock_lock(adapter->send_control_lock);

      if (fifo_read_avail(adapter->send_control_buffer) >= sizeof(send_command_size))
      {
         fifo_read(adapter->send_control_buffer, &send_command_size, sizeof(send_command_size));
         if (fifo_read_avail(adapter->send_control_buffer) >= sizeof(send_command_size))
         {
            fifo_read(adapter->send_control_buffer, send_command_buf, send_command_size);
            USB_WriteIntrMsg(adapter->handle, adapter->endpoint_out, send_command_size, send_command_buf);
         }
      }
      slock_unlock(adapter->send_control_lock);

      size = USB_ReadIntrMsg(adapter->handle, adapter->endpoint_in, adapter->endpoint_in_max_size, &adapter->data[0]);
      /* RARCH_LOG("%p USB_ReadIntrMsg(%i, %i, %i, %p): %i\n", &adapter->data[0],
         adapter->handle, adapter->endpoint_in, adapter->endpoint_in_max_size, &adapter->data[0],
         size); */

      //RARCH_LOG("%03i %03i %03i %03i\n", adapter->data[0], adapter->data[1], adapter->data[2], adapter->data[3], adapter->data[4]);
      //memmove(&adapter->data[1], &adapter->data[0], 2048);

      if (adapter && hid && hid->slots && size)
         pad_connection_packet(&hid->slots[adapter->slot], adapter->slot,
               adapter->data - 1, size+1);
   }
}

static void wiiusb_hid_device_send_control(void *data,
      uint8_t* data_buf, size_t size)
{
   struct wiiusb_adapter *adapter = (struct wiiusb_adapter*)data;

   if (!adapter)
      return;

   slock_lock(adapter->send_control_lock);

   if (fifo_write_avail(adapter->send_control_buffer) >= size + sizeof(size))
   {
      fifo_write(adapter->send_control_buffer, &size, sizeof(size));
      fifo_write(adapter->send_control_buffer, data_buf, size);
   }
   else
   {
      RARCH_WARN("adapter write buffer is full, cannot write send control\n");
   }
   slock_unlock(adapter->send_control_lock);
}

static void wiiusb_hid_device_add_autodetect(unsigned idx,
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
#if 0
         if (inter->bInterfaceClass == USB_CLASS_HID)
#endif
         {
            adapter->interface_number = (int)inter->bInterfaceNumber;

            for(k = 0; k < (int)inter->bNumEndpoints; k++)
            {
               const usb_endpointdesc *epdesc = &inter->endpoints[k];
               bool is_int = (epdesc->bmAttributes & 0x03) == USB_ENDPOINT_INTERRUPT;
               bool is_out = (epdesc->bEndpointAddress & 0x80) == USB_ENDPOINT_OUT;
               bool is_in = (epdesc->bEndpointAddress & 0x80) == USB_ENDPOINT_IN;
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
         }
         break;
      }
   }
}

static int remove_adapter(void *data, usb_device_entry *dev)
{
   struct wiiusb_adapter *adapter = (struct wiiusb_adapter*)&adapters;
   struct wiiusb_hid *hid = (struct wiiusb_hid*)data;

   while (adapter->next == NULL)
      return -1;

   if (&adapter->next->device == dev)
   {
      struct wiiusb_adapter *new_next = NULL;
      const char *name = (const char*)adapter->next->name;

      input_config_autoconfigure_disconnect(adapter->slot, name);

      adapter->next->quitting = true;
      sthread_join(adapter->next->thread);

      pad_connection_pad_deinit(&hid->slots[adapter->slot], adapter->slot);

      slock_free(adapter->send_control_lock);
      fifo_free(adapter->send_control_buffer);

      free(adapter->data);

      USB_CloseDevice(&adapter->next->handle);

      new_next = adapter->next->next;
      free(adapter->next);
      adapter->next = new_next;

      return 0;
   }

   adapter = adapter->next;

   return -1;
}

static int wiiusb_hid_removalnotify_cb(int result, void *usrdata)
{
   wiiusb_hid_t *hid = (wiiusb_hid_t*)usrdata;

   if (!hid)
      return -1;

   return 0;
}

static int add_adapter(void *data, usb_device_entry *dev)
{
   int rc;
   usb_devdesc desc;
   const char *device_name = NULL;
   struct wiiusb_adapter *old_head = NULL;
   struct wiiusb_hid *hid = (struct wiiusb_hid*)data;
   struct wiiusb_adapter *adapter  = (struct wiiusb_adapter*)
      calloc(1, sizeof(struct wiiusb_adapter));

   (void)rc;

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
            (void*)&adapter->device, dev->vid, dev->pid);
      free(adapter);
      return -1;
   }

   adapter->device = *dev;

   USB_GetDescriptors(adapter->handle, &desc);

   wiiusb_get_description(&adapter->device, adapter, &desc);

   if (adapter->endpoint_in == 0)
   {
      RARCH_ERR("Could not find HID config for device.\n");
      goto error;
   }

   if (desc.iManufacturer)
   {
      USB_GetAsciiString(adapter->handle, desc.iManufacturer, 0,
            sizeof(adapter->manufacturer_name), adapter->manufacturer_name);
#if 0
      RARCH_ERR(" Adapter Manufacturer name: %s\n", adapter->manufacturer_name);
#endif
   }

   if (desc.iProduct)
   {
      USB_GetAsciiString(adapter->handle, desc.iProduct, 0,
            sizeof(adapter->name), adapter->name);
#if 0
      RARCH_ERR(" Adapter name: %s\n", adapter->name);
#endif
   }

   device_name = (const char *)adapter->name;

   adapter->send_control_lock = slock_new();
   adapter->send_control_buffer = fifo_new(4096);

   if (!adapter->send_control_lock || !adapter->send_control_buffer)
   {
      RARCH_ERR("Error creating send control buffer.\n");
      goto error;
   }

   adapter->slot = pad_connection_pad_init(hid->slots,
         device_name, desc.idVendor, desc.idProduct,
         adapter, &wiiusb_hid_device_send_control);

   if (adapter->slot == -1)
      goto error;

   if (!pad_connection_has_interface(hid->slots, adapter->slot))
   {
      RARCH_ERR(" Interface not found (%s).\n", adapter->name);
      goto error;
   }

   RARCH_LOG("Interface found: [%s].\n", adapter->name);

   RARCH_LOG("Device 0x%p attached (VID/PID: %04x:%04x).\n",
         adapter->device, desc.idVendor, desc.idProduct);

   wiiusb_hid_device_add_autodetect(adapter->slot,
         device_name, wiiusb_hid.ident, desc.idVendor, desc.idProduct);

   adapter->hid = hid;
   adapter->thread = sthread_create(adapter_thread, adapter);

   if (!adapter->thread)
   {
      RARCH_ERR("Error initializing adapter thread.\n");
      goto error;
   }

   adapter->data = memalign(32, 2048);

   old_head = adapters.next;
   adapters.next = adapter;
   adapter->next = old_head;

   USB_FreeDescriptors(&desc);

   USB_DeviceRemovalNotifyAsync(adapter->handle, wiiusb_hid_removalnotify_cb, (void *)hid);

   return 0;

error:
   if (adapter->thread)
      sthread_join(adapter->thread);
   if (adapter->send_control_lock)
      slock_free(adapter->send_control_lock);
   if (adapter->send_control_buffer)
      fifo_free(adapter->send_control_buffer);
   if (adapter)
      free(adapter);
   USB_FreeDescriptors(&desc);
   USB_CloseDevice(&adapter->handle);
   return -1;
}

static int wiiusb_hid_changenotify_cb(int result, void *usrdata)
{
   wiiusb_hid_t *hid = (wiiusb_hid_t*)usrdata;

   if (!hid)
      return -1;

   /* usb_device_entry entries[8];
      u8 cnt = 0;

      USB_GetDeviceList(entries, 8, USB_CLASS_HID, &cnt);

      add_adapter((void *)hid, &entries[0]); */

   // RARCH_LOG("Wii USB hid change notify callback\n");

   USB_DeviceChangeNotifyAsync(USB_CLASS_HID, wiiusb_hid_changenotify_cb, usrdata);

   return 0;
}

static bool wiiusb_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *wiiusb_hid_joypad_name(void *data, unsigned pad)
{
   /* TODO/FIXME - implement properly */
   if (pad >= MAX_USERS)
      return NULL;

   return NULL;
}

static uint64_t wiiusb_hid_joypad_get_buttons(void *data, unsigned port)
{
   wiiusb_hid_t *hid = (wiiusb_hid_t*)data;
   if (hid)
      return pad_connection_get_buttons(&hid->slots[port], port);
   return 0;
}

static bool wiiusb_hid_joypad_button(void *data, unsigned port, uint16_t joykey)
{
   uint64_t buttons = wiiusb_hid_joypad_get_buttons(data, port);

   if (joykey == NO_BTN)
      return false;

   /* Check hat. */
   if (GET_HAT_DIR(joykey))
      return false;

   /* Check the button. */
   if ((port < MAX_USERS) && (joykey < 32))
      return ((buttons & (1 << joykey)) != 0);
   return false;
}

static bool wiiusb_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   wiiusb_hid_t		  *hid	= (wiiusb_hid_t*)data;
   if (!hid)
      return false;
   return pad_connection_rumble(&hid->slots[pad], pad, effect, strength);
}

static int16_t wiiusb_hid_joypad_axis(void *data,
      unsigned port, uint32_t joyaxis)
{
   wiiusb_hid_t			*hid = (wiiusb_hid_t*)data;
   int16_t					val = 0;

   if (joyaxis == AXIS_NONE)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      val = pad_connection_get_axis(&hid->slots[port],
            port, AXIS_NEG_GET(joyaxis));

      if (val >= 0)
         val = 0;
   }
   else if(AXIS_POS_GET(joyaxis) < 4)
   {
      val = pad_connection_get_axis(&hid->slots[port],
            port, AXIS_POS_GET(joyaxis));

      if (val <= 0)
         val = 0;
   }

   return val;
}

static void wiiusb_hid_free(void *data)
{
   wiiusb_hid_t *hid = (wiiusb_hid_t*)data;

   while (adapters.next)
      if (remove_adapter(hid, &adapters.next->device) == -1)
         RARCH_ERR("could not remove device %p\n",
               adapters.next->device);

   pad_connection_destroy(hid->slots);

   // wiiusb_hotplug_deregister_callback(hid->ctx, hid->hp);

   if (hid)
      free(hid);
}

static void *wiiusb_hid_init(void)
{
   unsigned i;
   u8 count;
   int ret;
   usb_device_entry *dev_entries;
   wiiusb_hid_t *hid = (wiiusb_hid_t*)calloc(1, sizeof(*hid));

   (void)ret;

   if (!hid)
      goto error;

   hid->slots = pad_connection_init(MAX_USERS);

   if (!hid->slots)
      goto error;

   dev_entries = (usb_device_entry *)calloc(MAX_USERS, sizeof(*dev_entries));

   if (!dev_entries)
      goto error;

   if (USB_GetDeviceList(dev_entries, MAX_USERS, USB_CLASS_HID, &count) < 0)
   {
      free(dev_entries);
      goto error;
   }

   for (i = 0; i < count; i++)
   {
      if (dev_entries[i].vid > 0 && dev_entries[i].pid > 0)
         add_adapter(hid, &dev_entries[i]);
   }

   free(dev_entries);

   USB_DeviceChangeNotifyAsync(USB_CLASS_HID, wiiusb_hid_changenotify_cb, (void *)hid);

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
   wiiusb_hid_joypad_get_buttons,
   wiiusb_hid_joypad_axis,
   wiiusb_hid_poll,
   wiiusb_hid_joypad_rumble,
   wiiusb_hid_joypad_name,
   "wiiusb",
};
