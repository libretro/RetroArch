/*  RetroArch - A frontend for libretro.
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
#include <string.h>

#ifdef __FreeBSD__
#include <libusb.h>
#else
#include <libusb-1.0/libusb.h>
#endif

#include <rthreads/rthreads.h>
#include <compat/strl.h>
#include <queues/fifo_queue.h>
#include <string/stdstring.h>

#include "../connect/joypad_connection.h"
#include "../input_defines.h"
#include "../../tasks/tasks_internal.h"
#include "../input_driver.h"
#include "../../verbosity.h"

#ifndef LIBUSB_CAP_HAS_HOTPLUG
#define LIBUSB_CAP_HAS_HOTPLUG 0x0001
#endif

typedef struct libusb_hid
{
   libusb_context *ctx;
   joypad_connection_t *slots;
   sthread_t *poll_thread;
   int can_hotplug;
#if defined(__FreeBSD__) && LIBUSB_API_VERSION <= 0x01000102
   libusb_hotplug_callback_handle hp;
#else
   int hp; /* libusb_hotplug_callback_handle is just int */
#endif
   int quit;
} libusb_hid_t;

struct libusb_adapter
{
   libusb_hid_t *hid;
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

   int slot;

   sthread_t *thread;
   slock_t *send_control_lock;
   fifo_buffer_t *send_control_buffer;
   struct libusb_adapter *next;
};

static struct libusb_adapter adapters;

static void adapter_thread(void *data)
{
   uint8_t send_command_buf[4096];
   struct libusb_adapter *adapter = (struct libusb_adapter*)data;
   libusb_hid_t *hid              = adapter ? adapter->hid : NULL;

   if (!adapter)
      return;

   while (!adapter->quitting)
   {
      size_t send_command_size;
      int tmp;
      int report_number;
      int size = 0;

      slock_lock(adapter->send_control_lock);
      if (fifo_read_avail(adapter->send_control_buffer)
            >= sizeof(send_command_size))
      {
         fifo_read(adapter->send_control_buffer,
               &send_command_size, sizeof(send_command_size));

         if (fifo_read_avail(adapter->send_control_buffer)
               >= sizeof(send_command_size))
         {
            fifo_read(adapter->send_control_buffer,
                  send_command_buf, send_command_size);
            libusb_interrupt_transfer(adapter->handle,
                  adapter->endpoint_out, send_command_buf,
                  send_command_size, &tmp, 1000);
         }
      }
      slock_unlock(adapter->send_control_lock);

      libusb_interrupt_transfer(adapter->handle,
            adapter->endpoint_in, &adapter->data[1],
            adapter->endpoint_in_max_size, &size, 1000);

      if (adapter && hid && hid->slots && size)
         pad_connection_packet(&hid->slots[adapter->slot], adapter->slot,
               adapter->data, size+1);
   }
}

static void libusb_hid_device_send_control(void *data,
      uint8_t* data_buf, size_t size)
{
   struct libusb_adapter *adapter = (struct libusb_adapter*)data;

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

static void libusb_hid_device_add_autodetect(unsigned idx,
      const char *device_name, const char *driver_name,
      uint16_t dev_vid, uint16_t dev_pid)
{
   input_autoconfigure_connect(
         device_name,
         NULL,
         driver_name,
         idx,
         dev_vid,
         dev_pid
         );
}

static void libusb_get_description(struct libusb_device *device,
      struct libusb_adapter *adapter)
{
   int j;
   unsigned i, k;
   struct libusb_config_descriptor *config;

   int desc_ret = libusb_get_config_descriptor(device, 0, &config);

   if (desc_ret != 0)
   {
      RARCH_ERR("Error %d getting libusb config descriptor\n", desc_ret);
      return;
   }

   for (i = 0; i < (int)config->bNumInterfaces; i++)
   {
      const struct libusb_interface *inter = &config->interface[i];

      for(j = 0; j < inter->num_altsetting; j++)
      {
         const struct libusb_interface_descriptor *interdesc =
            &inter->altsetting[j];

#if 0
         if (interdesc->bInterfaceClass == LIBUSB_CLASS_HID)
#endif
         {
            adapter->interface_number = (int)interdesc->bInterfaceNumber;

            for(k = 0; k < (int)interdesc->bNumEndpoints; k++)
            {
               const struct libusb_endpoint_descriptor *epdesc =
                  &interdesc->endpoint[k];
               bool is_int = (epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK)
                  == LIBUSB_TRANSFER_TYPE_INTERRUPT;
               bool is_out = (epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
                  == LIBUSB_ENDPOINT_OUT;
               bool is_in = (epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
                  == LIBUSB_ENDPOINT_IN;

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
   const char *device_name         = NULL;
   struct libusb_adapter *old_head = NULL;
   struct libusb_hid          *hid = (struct libusb_hid*)data;
   struct libusb_adapter *adapter  = (struct libusb_adapter*)
      calloc(1, sizeof(struct libusb_adapter));

   if (!adapter)
      return -1;

   if (!hid)
   {
      free(adapter);
      RARCH_ERR("Allocation of adapter failed.\n");
      return -1;
   }

   rc = libusb_get_device_descriptor(dev, &desc);

   if (rc != LIBUSB_SUCCESS)
   {
      RARCH_ERR("Error getting device descriptor.\n");
      goto error;
   }

   adapter->device = dev;

   libusb_get_description(adapter->device, adapter);

   if (adapter->endpoint_in == 0)
   {
      RARCH_ERR("Could not find HID config for device.\n");
      goto error;
   }

   rc = libusb_open (adapter->device, &adapter->handle);

   if (rc != LIBUSB_SUCCESS)
   {
      RARCH_ERR("Error opening device 0x%p (VID/PID: %04x:%04x).\n",
            (void*)adapter->device, desc.idVendor, desc.idProduct);
      goto error;
   }

   if (desc.iManufacturer)
   {
      libusb_get_string_descriptor_ascii(adapter->handle,
            desc.iManufacturer, adapter->manufacturer_name,
            sizeof(adapter->manufacturer_name));
#if 0
      RARCH_ERR(" Adapter Manufacturer name: %s\n",
            adapter->manufacturer_name);
#endif
   }

   if (desc.iProduct)
   {
      libusb_get_string_descriptor_ascii(adapter->handle,
            desc.iProduct, adapter->name,
            sizeof(adapter->name));
#if 0
      RARCH_ERR(" Adapter name: %s\n", adapter->name);
#endif
   }

   device_name   = (const char*)adapter->name;

   if (string_is_empty((const char*)adapter->name))
      goto error;

   adapter->send_control_lock = slock_new();
   adapter->send_control_buffer = fifo_new(4096);

   if (!adapter->send_control_lock || !adapter->send_control_buffer)
   {
      RARCH_ERR("Error creating send control buffer.\n");
      goto error;
   }

   adapter->slot = pad_connection_pad_init(hid->slots,
         device_name, desc.idVendor, desc.idProduct,
         adapter, &libusb_hid);

   if (adapter->slot == -1)
      goto error;

   if (!pad_connection_has_interface(hid->slots, adapter->slot))
   {
      RARCH_ERR("Interface not found (%s) (VID/PID: %04x:%04x).\n",
         adapter->name, desc.idVendor, desc.idProduct);
      goto error;
   }

   RARCH_LOG("Interface found: [%s].\n", adapter->name);

   if (libusb_kernel_driver_active(adapter->handle, 0) == 1
         && libusb_detach_kernel_driver(adapter->handle, 0))
   {
      RARCH_ERR("Error detaching handle 0x%p from kernel.\n", adapter->handle);
      goto error;
   }

   rc = libusb_claim_interface(adapter->handle, adapter->interface_number);

   if (rc != LIBUSB_SUCCESS)
   {
      RARCH_ERR("Error claiming interface %d .\n", adapter->interface_number);
      goto error;
   }

   RARCH_LOG("Device 0x%p attached (VID/PID: %04x:%04x).\n",
         adapter->device, desc.idVendor, desc.idProduct);

   libusb_hid_device_add_autodetect(adapter->slot,
         device_name, libusb_hid.ident, desc.idVendor, desc.idProduct);

   adapter->hid = hid;
   adapter->thread = sthread_create(adapter_thread, adapter);

   if (!adapter->thread)
   {
      RARCH_ERR("Error initializing adapter thread.\n");
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
   struct libusb_adapter  *adapter = (struct libusb_adapter*)&adapters;
   struct libusb_hid          *hid = (struct libusb_hid*)data;

   while (adapter->next == NULL)
      return -1;

   if (adapter->next->device == dev)
   {
      struct libusb_adapter *new_next = NULL;
      const char                *name = (const char*)adapter->next->name;

      input_autoconfigure_disconnect(adapter->slot, name);

      adapter->next->quitting = true;
      sthread_join(adapter->next->thread);

      pad_connection_pad_deinit(&hid->slots[adapter->slot], adapter->slot);

      slock_free(adapter->send_control_lock);
      fifo_free(adapter->send_control_buffer);

      libusb_release_interface(adapter->next->handle,
            adapter->next->interface_number);
      libusb_close(adapter->next->handle);

      new_next = adapter->next->next;
      free(adapter->next);
      adapter->next = new_next;

      return 0;
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
         RARCH_WARN("Unhandled event: %d\n", event);
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

static void libusb_hid_joypad_get_buttons(void *data, unsigned port,
      input_bits_t *state)
{
   libusb_hid_t        *hid   = (libusb_hid_t*)data;
   if (hid)
   {
      pad_connection_get_buttons(&hid->slots[port], port, state);
      return;
   }

   BIT256_CLEAR_ALL_PTR(state);
}

static bool libusb_hid_joypad_button(void *data,
      unsigned port, uint16_t joykey)
{
   input_bits_t buttons;
   libusb_hid_joypad_get_buttons(data, port, &buttons);

   /* Check hat. */
   if (GET_HAT_DIR(joykey))
      return false;

   /* Check the button. */
   if ((port < MAX_USERS) && (joykey < 32))
      return (BIT256_GET(buttons, joykey) != 0);
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

static int16_t libusb_hid_joypad_axis(void *data,
      unsigned port, uint32_t joyaxis)
{
   libusb_hid_t         *hid = (libusb_hid_t*)data;
   int16_t               val = 0;

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

static void libusb_hid_free(const void *data)
{
   libusb_hid_t *hid = (libusb_hid_t*)data;

   while(adapters.next)
      if (remove_adapter(hid, adapters.next->device) == -1)
         RARCH_ERR("could not remove device %p\n",
               adapters.next->device);

   if (hid->poll_thread)
   {
      hid->quit = 1;
      sthread_join(hid->poll_thread);
   }

   if (hid->slots)
      pad_connection_destroy(hid->slots);

   if (hid->can_hotplug)
      libusb_hotplug_deregister_callback(hid->ctx, hid->hp);

   libusb_exit(hid->ctx);
   free(hid);
}

static void poll_thread(void *data)
{
   libusb_hid_t *hid = (libusb_hid_t*)data;

   while (!hid->quit)
   {
      struct timeval timeout = {0};
      libusb_handle_events_timeout_completed(NULL,
            &timeout, &hid->quit);
   }
}

static void *libusb_hid_init(void)
{
   unsigned i, count;
   int ret;
   struct libusb_device **devices;
   libusb_hid_t *hid = (libusb_hid_t*)calloc(1, sizeof(*hid));

   if (!hid)
      goto error;

   ret = libusb_init(&hid->ctx);

   if (ret < 0)
      goto error;

#if LIBUSB_API_VERSION <= 0x01000102
   /* API is too old, so libusb_has_capability function does not exist.
    * Since we can't be sure, we assume for now there might be hot-plugging
    * capability and continue on until we're told otherwise.
    */
   hid->can_hotplug = 1;
#else
   /* Ask libusb if it supports hotplug and store the result.
    * Note: On Windows this will probably be false, see:
    *  https://github.com/libusb/libusb/issues/86
    */
   if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
      hid->can_hotplug = 1;
   else
      hid->can_hotplug = 0;
#endif

   hid->slots = pad_connection_init(MAX_USERS);

   if (!hid->slots)
      goto error;

   count = libusb_get_device_list(hid->ctx, &devices);

   for (i = 0; i < count; i++)
   {
      struct libusb_device_descriptor desc;
      libusb_get_device_descriptor(devices[i], &desc);

      if (desc.idVendor > 0 && desc.idProduct > 0)
         add_adapter(hid, devices[i]);
   }

   if (count > 0)
      libusb_free_device_list(devices, 1);

   if (hid->can_hotplug)
   {
      ret = libusb_hotplug_register_callback(
            hid->ctx,
            (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
            LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
            (libusb_hotplug_flag)LIBUSB_HOTPLUG_ENUMERATE,
            LIBUSB_HOTPLUG_MATCH_ANY,
            LIBUSB_HOTPLUG_MATCH_ANY,
            LIBUSB_HOTPLUG_MATCH_ANY,
            libusb_hid_hotplug_callback,
            hid,
            &hid->hp);

      if (ret != LIBUSB_SUCCESS)
      {
         /* Creating the hotplug callback has failed. We assume libusb
          * is still okay to continue and just update our knowledge of
          * the situation accordingly.
          */
         RARCH_WARN("[libusb] Failed to create a hotplug callback.\n");
         hid->can_hotplug = 0;
      }
   }

   hid->poll_thread = sthread_create(poll_thread, hid);

   if (!hid->poll_thread)
   {
      RARCH_ERR("Error creating polling thread");
      goto error;
   }

   return hid;

error:
   if (hid)
      libusb_hid_free(hid);
   return NULL;
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
   libusb_hid_device_send_control,
};
