/*  RetroArch - A frontend for libretro.
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

#include <libusb-1.0/libusb.h>
#include <rthreads/rthreads.h>
#include <compat/strl.h>
#include <queues/fifo_buffer.h>
#include "../connect/joypad_connection.h"
#include "../../driver.h"
#include "../input_autodetect.h"
#include "../input_hid_driver.h"

typedef struct libusb_hid
{
   libusb_hotplug_callback_handle hp;
   joypad_connection_t *slots;
} libusb_hid_t;

struct libusb_adapter
{
   volatile bool quitting;
   struct libusb_device *device;
   libusb_device_handle *handle;
   int interface_number;
   int endpoint_in;
   int endpoint_out;
   int endpoint_in_max_size;
   int endpoint_out_max_size;

   uint8_t manufacturer_name[255];
   uint8_t name[255];
   uint8_t data[2048];

   uint32_t slot;

   sthread_t *thread;
   slock_t *send_control_lock;
   fifo_buffer_t *send_control_buffer;
   struct libusb_adapter *next;
};

static struct libusb_adapter adapters;

static void adapter_thread(void *data)
{
   struct libusb_adapter *adapter = (struct libusb_adapter*)data;
   uint8_t send_command_buf[4096];

   while (!adapter->quitting)
   {
      driver_t *driver               = driver_get_ptr();
      libusb_hid_t *hid              = (libusb_hid_t*)driver->hid_data;
      int size = 0;
      size_t send_command_size;
      int tmp;
      int report_number;

      slock_lock(adapter->send_control_lock);
      if (fifo_read_avail(adapter->send_control_buffer) >= sizeof(send_command_size))
      {
         fifo_read(adapter->send_control_buffer, &send_command_size, sizeof(send_command_size));
         if (fifo_read_avail(adapter->send_control_buffer) >= sizeof(send_command_size))
         {
            fifo_read(adapter->send_control_buffer, send_command_buf, send_command_size);
#if 0
            report_number = send_command_buf[0];
            libusb_control_transfer(adapter->handle,
                  LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT,
                  0x09/*HID Set_Report*/,
                  (2/*HID output*/ << 8) | report_number,
                  adapter->interface_number,
                  send_command_buf, send_command_size,
                  1000/*timeout millis*/);
#else
            libusb_interrupt_transfer(adapter->handle, adapter->endpoint_out, send_command_buf, send_command_size, &tmp, 1000);
#endif
         }
      }
      slock_unlock(adapter->send_control_lock);

      libusb_interrupt_transfer(adapter->handle, adapter->endpoint_in, &adapter->data[1], adapter->endpoint_in_max_size, &size, 1000);

#if 0
      static unsigned count;
      fprintf(stderr, "[%s] Gets here, count: %d\n", adapter->name, count++);
#endif
      if (adapter && hid && hid->slots && size)
         pad_connection_packet(&hid->slots[adapter->slot], adapter->slot,
               adapter->data, size+1);
   }
}

static void libusb_hid_device_send_control(void *data,
      uint8_t* data_buf, size_t size)
{
   struct libusb_adapter *adapter = (struct libusb_adapter*)data;

   if (adapter)
   {
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
}

static void libusb_hid_device_add_autodetect(unsigned idx,
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

static void libusb_get_description(struct libusb_device *device,
      struct libusb_adapter *adapter)
{
   unsigned i, j, k;
   struct libusb_config_descriptor *config;

   libusb_get_config_descriptor(device, 0, &config);

   fprintf(stderr, "Interfaces: %d.\n", (int)config->bNumInterfaces);

   for(i=0; i < (int)config->bNumInterfaces; i++)
   {
      const struct libusb_interface *inter = &config->interface[i];

      fprintf(stderr, " Number of alternate settings: %d.\n", inter->num_altsetting);

      for(j = 0; j < inter->num_altsetting; j++)
      {
         const struct libusb_interface_descriptor *interdesc = &inter->altsetting[j];

         //if (interdesc->bInterfaceClass == LIBUSB_CLASS_HID)
         {
            adapter->interface_number = (int)interdesc->bInterfaceNumber;

            fprintf(stderr, "  Interface Number: %d.\n",    (int)interdesc->bInterfaceNumber);
            fprintf(stderr, "  Number of endpoints: %d.\n", (int)interdesc->bNumEndpoints);

            for(k = 0; k < (int)interdesc->bNumEndpoints; k++)
            {
               const struct libusb_endpoint_descriptor *epdesc = &interdesc->endpoint[k];
               bool is_int = (epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT;
               bool is_out = (epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT;
               bool is_in = (epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN;
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
               fprintf(stderr, "    Descriptor Type: %d.\n", (int)epdesc->bDescriptorType);
               fprintf(stderr, "    EP Address: %d.\n",      (int)epdesc->bEndpointAddress);
               fprintf(stderr, "    is_int: %d.\n",          (int)is_int);
               fprintf(stderr, "    is_out: %d.\n",          (int)is_out);
               fprintf(stderr, "    is_in: %d.\n\n",         (int)is_in);
            }
         }
         goto ret;
      }
   }

   ret:
   libusb_free_config_descriptor(config);
}

static int add_adapter(void *data, struct libusb_device *dev)
{
   int rc;
   struct libusb_device_descriptor desc;
   struct libusb_adapter *old_head = NULL;
   struct libusb_hid          *hid = (struct libusb_hid*)data;
   const char *device_name         = NULL;
   struct libusb_adapter *adapter  = (struct libusb_adapter*)calloc(1, sizeof(struct libusb_adapter));

   if (!adapter || !hid)
   {
      fprintf(stderr, "Allocation of adapter failed.\n");
      return -1;
   }

   rc = libusb_get_device_descriptor(dev, &desc);

   if (rc != LIBUSB_SUCCESS)
   {
      fprintf(stderr, "Error getting device descriptor.\n");
      goto error;
   }

   adapter->device = dev;

   libusb_get_description(adapter->device, adapter);

   if (adapter->endpoint_in == 0)
   {
      fprintf(stderr, "Could not find HID config for device.\n");
      goto error;
   }

   rc = libusb_open (adapter->device, &adapter->handle);

   if (rc != LIBUSB_SUCCESS)
   {
      fprintf(stderr, "Error opening device 0x%p (VID/PID: %04x:%04x).\n",
            adapter->device, desc.idVendor, desc.idProduct);
      goto error;
   }

   if (desc.iManufacturer)
   {
      libusb_get_string_descriptor_ascii(adapter->handle,
            desc.iManufacturer, adapter->manufacturer_name,
            sizeof(adapter->manufacturer_name));
      fprintf(stderr, " Adapter Manufacturer name: %s\n",
            adapter->manufacturer_name);
   }

   if (desc.iProduct)
   {
      libusb_get_string_descriptor_ascii(adapter->handle,
            desc.iProduct, adapter->name,
            sizeof(adapter->name));
      fprintf(stderr, " Adapter name: %s\n", adapter->name);
   }

   device_name   = (const char*)adapter->name;

   if (adapter->name[0] == '\0')
      goto error;

   adapter->send_control_lock = slock_new();
   adapter->send_control_buffer = fifo_new(4096);

   if (!adapter->send_control_lock || !adapter->send_control_buffer)
   {
      fprintf(stderr, "Error creating send control buffer.\n");
      goto error;
   }

   adapter->slot = pad_connection_pad_init(hid->slots,
         device_name, desc.idVendor, desc.idProduct, adapter, &libusb_hid_device_send_control);

   if (!pad_connection_has_interface(hid->slots, adapter->slot))
   {
      fprintf(stderr, " Interface not found (%s).\n", adapter->name);
      goto error;
   }

   if (libusb_kernel_driver_active(adapter->handle, 0) == 1
         && libusb_detach_kernel_driver(adapter->handle, 0))
   {
      fprintf(stderr, " Error detaching handle 0x%p from kernel.\n", adapter->handle);
      goto error;
   }

   rc = libusb_claim_interface(adapter->handle, adapter->interface_number);

   if (rc != LIBUSB_SUCCESS)
   {
      fprintf(stderr, "Error claiming interface %d .\n", adapter->interface_number);
      goto error;
   }

   fprintf(stderr, " Device 0x%p attached (VID/PID: %04x:%04x).\n",
         adapter->device, desc.idVendor, desc.idProduct);

   libusb_hid_device_add_autodetect(adapter->slot,
         device_name, libusb_hid.ident, desc.idVendor, desc.idProduct);

   adapter->thread = sthread_create(adapter_thread, adapter);

   if (!adapter->thread)
   {
      fprintf(stderr, "Error initializing adapter thread.\n");
      goto error;
   }

   old_head      = adapters.next;
   adapters.next = adapter;
   adapter->next = old_head;

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
   return -1;
}

static int remove_adapter(void *data, struct libusb_device *dev)
{
   struct libusb_adapter *adapter = (struct libusb_adapter*)&adapters;
   struct libusb_hid          *hid = (struct libusb_hid*)data;

   while (adapter->next != NULL)
   {
      if (adapter->next->device == dev)
      {
         fprintf(stderr, "Device 0x%p disconnected.\n", adapter->next->device);

         struct libusb_adapter *new_next = NULL;
         adapter->next->quitting = true;
         sthread_join(adapter->next->thread);

         pad_connection_pad_deinit(&hid->slots[adapter->slot], adapter->slot);

         slock_free(adapter->send_control_lock);
         fifo_free(adapter->send_control_buffer);

         libusb_release_interface(adapter->next->handle, adapter->next->interface_number);
         libusb_close(adapter->next->handle);

         new_next = adapter->next->next;
         free(adapter->next);
         adapter->next = new_next;


         return 0;
      }

      adapter = adapter->next;
   }

   return -1;
}

static int libusb_hid_hotplug_callback(struct libusb_context *ctx,
      struct libusb_device *dev, libusb_hotplug_event event, void *user_data)
{
   libusb_hid_t *hid = (libusb_hid_t*)user_data;

   switch (event)
   {
      case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
         add_adapter(hid, dev);
         break;
      case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
         remove_adapter(hid, dev);
         break;
      default:
         fprintf(stderr, "Unhandled event: %d\n", event);
         break;
   }

   return 0;
}


static bool libusb_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *libusb_hid_joypad_name(void *data, unsigned pad)
{
   /* TODO/FIXME - implement properly */
   if (pad >= MAX_USERS)
      return NULL;

   return NULL;
}

static uint64_t libusb_hid_joypad_get_buttons(void *data, unsigned port)
{
   libusb_hid_t        *hid   = (libusb_hid_t*)data;
   if (hid)
      return pad_connection_get_buttons(&hid->slots[port], port);
   return 0;
}

static bool libusb_hid_joypad_button(void *data, unsigned port, uint16_t joykey)
{
   uint64_t buttons          = libusb_hid_joypad_get_buttons(data, port);

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

static bool libusb_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   libusb_hid_t        *hid   = (libusb_hid_t*)data;
   if (!hid)
      return false;
   return pad_connection_rumble(&hid->slots[pad], pad, effect, strength);
}

static int16_t libusb_hid_joypad_axis(void *data, unsigned port, uint32_t joyaxis)
{
   libusb_hid_t         *hid = (libusb_hid_t*)data;
   int16_t               val = 0;

   if (joyaxis == AXIS_NONE)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      val = pad_connection_get_axis(&hid->slots[port], port, AXIS_NEG_GET(joyaxis));

      if (val >= 0)
         val = 0;
   }
   else if(AXIS_POS_GET(joyaxis) < 4)
   {
      val = pad_connection_get_axis(&hid->slots[port], port, AXIS_POS_GET(joyaxis));

      if (val <= 0)
         val = 0;
   }

   return val;
}

static void libusb_hid_free(void *data)
{
   libusb_hid_t *hid = (libusb_hid_t*)data;

   while(adapters.next)
      remove_adapter(hid, adapters.next->device);

   pad_connection_destroy(hid->slots);

   libusb_hotplug_deregister_callback(NULL, hid->hp);

   libusb_exit(NULL);
   if (hid)
      free(hid);
}

static void *libusb_hid_init(void)
{
   unsigned i;
   int ret, count;
   struct libusb_device **devices;
   libusb_hid_t *hid = (libusb_hid_t*)calloc(1, sizeof(*hid));

   if (!hid)
      goto error;

   ret = libusb_init(NULL);

   if (ret < 0)
      goto error;

   if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
      goto error;

   hid->slots = (joypad_connection_t*)pad_connection_init(MAX_USERS);

   count = libusb_get_device_list(NULL, &devices);

   for (i = 0; i < count; i++)
   {
      struct libusb_device_descriptor desc;
      libusb_get_device_descriptor(devices[i], &desc);

      if (desc.idVendor > 0 && desc.idProduct > 0)
         add_adapter(hid, devices[i]);
   }

   if (count > 0)
      libusb_free_device_list(devices, 1);

   ret = libusb_hotplug_register_callback(NULL,
         LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
         LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0,
         LIBUSB_HOTPLUG_MATCH_ANY,
         LIBUSB_HOTPLUG_MATCH_ANY,
         LIBUSB_HOTPLUG_MATCH_ANY,
         libusb_hid_hotplug_callback,
         hid,
         &hid->hp);

   if (ret != LIBUSB_SUCCESS)
   {
      fprintf(stderr, "Error creating a hotplug callback.\n");
      goto error;
   }


   return hid;

error:
   libusb_hid_free(hid);
   return hid;
}


static void libusb_hid_poll(void *data)
{
   (void)data;
}

hid_driver_t libusb_hid = {
   libusb_hid_init,
   libusb_hid_joypad_query,
   libusb_hid_free,
   libusb_hid_joypad_button,
   libusb_hid_joypad_get_buttons,
   libusb_hid_joypad_axis,
   libusb_hid_poll,
   libusb_hid_joypad_rumble,
   libusb_hid_joypad_name,
   "libusb",
};
