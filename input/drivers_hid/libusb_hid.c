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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __FreeBSD__
#include <libusb.h>
#else
#include <libusb-1.0/libusb.h>
#endif

#include <rthreads/rthreads.h>
#include <features/features_cpu.h>
#include <compat/strl.h>
#include <queues/mpsc_stack.h>
#include <retro_atomic.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "../connect/joypad_connection.h"
#include "../input_defines.h"
#include "../../tasks/tasks_internal.h"
#include "../input_driver.h"
#include "../../verbosity.h"

/* Transfer callbacks are declared LIBUSB_CALL (WINAPI on Windows);
 * older and non-upstream headers may not define it. */
#ifndef LIBUSB_CALL
#define LIBUSB_CALL
#endif

#ifndef LIBUSB_CAP_HAS_HOTPLUG
#define LIBUSB_CAP_HAS_HOTPLUG 0x0001
#endif

/* libusb 1.0.21 added libusb_interrupt_event_handler(), which wakes a
 * thread blocked handling events. With it the poll thread can block
 * until there is work and still be stopped at once; without it, it has
 * to come up every 100 ms to look at its quit flag. FreeBSD's own
 * libusb is kept on the old path: its API version does not track
 * upstream's feature set. */
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000105 \
      && !defined(__FreeBSD__)
#define LIBUSB_HID_CAN_INTERRUPT 1
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

/* Each pad is driven by libusb's asynchronous transfers, completed on
 * the poll thread; there is no thread per pad. One IN transfer is kept
 * submitted for as long as the pad is attached, with no timeout: it
 * completes when the pad sends a report, and is resubmitted from its
 * callback. Commands (rumble, LEDs) are queued and sent one at a time
 * on a single OUT transfer, so they go out in order and as soon as the
 * endpoint takes them, never behind a read.
 *
 * Removal cancels whatever is in flight and parks the adapter on a
 * retire list. Cancellation, like every completion, is delivered by
 * event handling, and the adapter is freed once both transfers have
 * come back - by the poll thread between event batches, never from a
 * callback. Nothing waits on a transfer, and nothing takes a lock. */

/* One queued command; the payload follows. */
typedef struct libusb_command
{
   mpsc_stack_node_t link;   /* first: a node is its command */
   size_t len;
   uint8_t data[1];
} libusb_command_t;

/* Bytes of commands an adapter holds before it drops new ones. */
#define LIBUSB_SEND_QUEUE_MAX 4096

struct libusb_adapter
{
   libusb_hid_t *hid;
   struct libusb_device *device;
   libusb_device_handle *handle;
   int interface_number;
   int endpoint_in;
   int endpoint_out;
   int endpoint_in_max_size;
   int endpoint_out_max_size;

   uint8_t manufacturer_name[NAME_MAX_LENGTH];
   uint8_t name[NAME_MAX_LENGTH];
   uint8_t data[2048];
   uint8_t send_buf[4096];

   int32_t slot;

   struct libusb_transfer *in_transfer;
   struct libusb_transfer *out_transfer;

   /* Commands come from any thread - the frontend's rumble, a pad
    * handler on the event-handling thread - and are pushed on
    * 'pending'. Whichever thread holds 'out_busy' sends them, one at a
    * time and in order: it alone touches 'queue' (those taken off
    * 'pending', in send order) and 'send_buf'. */
   mpsc_stack_t pending;
   libusb_command_t *queue;
   retro_atomic_int_t out_busy;
   retro_atomic_int_t queued_bytes;
   /* send_control() calls inside the adapter; it is not freed under
    * one. */
   retro_atomic_int_t senders;
   /* Set on the event-handling thread when the pad goes. */
   retro_atomic_int_t removing;
   /* The event-handling thread's alone. */
   bool in_busy;

   struct libusb_adapter *next;
};

static struct libusb_adapter adapters;
/* Removed adapters whose transfers have not all come back yet. Only
 * the event-handling thread touches it. */
static struct libusb_adapter *retiring = NULL;

static void LIBUSB_CALL libusb_adapter_out_cb(struct libusb_transfer *transfer);

/* Has the poll thread come round between event batches, where it frees
 * removed adapters. Without an interrupt it comes round every 100 ms
 * anyway. */
static void libusb_hid_wake(libusb_hid_t *hid)
{
#ifdef LIBUSB_HID_CAN_INTERRUPT
   libusb_interrupt_event_handler(hid->ctx);
#else
   (void)hid;
#endif
}

/* The holder of out_busy: the next command in send order, or NULL. */
static libusb_command_t *libusb_adapter_next_command(
      struct libusb_adapter *adapter)
{
   libusb_command_t *cmd;

   if (!adapter->queue)
   {
      /* Pushed newest first: reversed into send order. */
      mpsc_stack_node_t *l     = mpsc_stack_drain(&adapter->pending);
      mpsc_stack_node_t *order = NULL;
      while (l)
      {
         mpsc_stack_node_t *next = l->next;
         l->next = order;
         order   = l;
         l       = next;
      }
      adapter->queue = (libusb_command_t*)order;
   }
   if ((cmd = adapter->queue))
      adapter->queue = (libusb_command_t*)cmd->link.next;
   return cmd;
}

/* The holder of out_busy: sends the next command, or lets go of the
 * claim when none is left. A command pushed while it lets go was left
 * to it by a sender that found the claim held, so it looks again once
 * it has let go, and takes the claim back for one. Submitting takes
 * libusb's own locks; nothing of ours is held. */
static void libusb_adapter_send_next(struct libusb_adapter *adapter)
{
   for (;;)
   {
      libusb_command_t *cmd;

      while ((cmd = libusb_adapter_next_command(adapter)))
      {
         size_t _len = cmd->len;
         retro_atomic_fetch_sub_int(&adapter->queued_bytes, (int)_len);
         if (retro_atomic_load_seq_cst_int(&adapter->removing))
         {
            free(cmd);
            continue;
         }
         memcpy(adapter->send_buf, cmd->data, _len);
         free(cmd);
         libusb_fill_interrupt_transfer(adapter->out_transfer,
               adapter->handle, (unsigned char)adapter->endpoint_out,
               adapter->send_buf, (int)_len,
               libusb_adapter_out_cb, adapter, 1000);
         /* The callback sends the next one. A command that fails to
          * go is dropped, as one that fails on the wire is. */
         if (libusb_submit_transfer(adapter->out_transfer) == LIBUSB_SUCCESS)
            return;
      }

      retro_atomic_store_release_int(&adapter->out_busy, 0);
      retro_atomic_thread_fence_seq_cst();
      if (     mpsc_stack_empty(&adapter->pending)
            || retro_atomic_exchange_int(&adapter->out_busy, 1))
         return;
   }
}

static void LIBUSB_CALL libusb_adapter_out_cb(struct libusb_transfer *transfer)
{
   /* Whatever happened to this one, send the next. A command that
    * failed or timed out is dropped, as it was when sent inline. */
   libusb_adapter_send_next((struct libusb_adapter*)transfer->user_data);
}

static void LIBUSB_CALL libusb_adapter_in_cb(struct libusb_transfer *transfer)
{
   struct libusb_adapter *adapter = (struct libusb_adapter*)transfer->user_data;
   libusb_hid_t *hid              = adapter->hid;
   bool resubmit                  = false;

   /* in_busy and removal are the event-handling thread's, as this
    * callback is. */
   adapter->in_busy = false;
   if (!retro_atomic_load_acquire_int(&adapter->removing))
      resubmit = (   transfer->status == LIBUSB_TRANSFER_COMPLETED
                  || transfer->status == LIBUSB_TRANSFER_TIMED_OUT);

   /* Removal and this callback both run on the event-handling thread,
    * so a slot that is not being removed is not deinitialised under
    * this call. */
   if (     resubmit
         && transfer->status == LIBUSB_TRANSFER_COMPLETED
         && transfer->actual_length > 0
         && hid && hid->slots)
      pad_connection_packet(&hid->slots[adapter->slot], adapter->slot,
            adapter->data, transfer->actual_length);

   /* Anything else - the device went away, an error, a cancel - ends
    * the reads. */
   if (!resubmit)
   {
      if (     transfer->status != LIBUSB_TRANSFER_CANCELLED
            && transfer->status != LIBUSB_TRANSFER_NO_DEVICE)
         RARCH_WARN("[libusb] Reads from \"%s\" stopped (transfer status %d).\n",
               (const char*)adapter->name, (int)transfer->status);
      return;
   }

   adapter->in_busy = (libusb_submit_transfer(transfer) == LIBUSB_SUCCESS);
}

static void libusb_hid_device_send_control(void *data,
      uint8_t *s, size_t len)
{
   struct libusb_adapter *adapter = (struct libusb_adapter*)data;
   libusb_command_t *cmd;

   if (!adapter || !adapter->endpoint_out)
      return;

   retro_atomic_fetch_add_seq_cst_int(&adapter->senders, 1);
   if (retro_atomic_load_seq_cst_int(&adapter->removing))
      goto done;

   if (     len > sizeof(adapter->send_buf)
         || retro_atomic_fetch_add_int(&adapter->queued_bytes, (int)len)
            + len > LIBUSB_SEND_QUEUE_MAX)
   {
      if (len <= sizeof(adapter->send_buf))
         retro_atomic_fetch_sub_int(&adapter->queued_bytes, (int)len);
      RARCH_WARN("[libusb] Adapter write buffer is full, cannot write send control.\n");
      goto done;
   }
   if (!(cmd = (libusb_command_t*)malloc(
               offsetof(libusb_command_t, data) + len)))
   {
      retro_atomic_fetch_sub_int(&adapter->queued_bytes, (int)len);
      goto done;
   }
   cmd->len = len;
   memcpy(cmd->data, s, len);
   mpsc_stack_push(&adapter->pending, &cmd->link);

   /* Sends it now if no send is in flight; else the holder will. */
   retro_atomic_thread_fence_seq_cst();
   if (!retro_atomic_exchange_int(&adapter->out_busy, 1))
      libusb_adapter_send_next(adapter);

done:
   {
      /* The last sender out of a removed adapter has the poll thread
       * free it: no transfer of its own is left to wake that. Taken
       * before letting go, after which the adapter may be gone. */
      libusb_hid_t *hid = adapter->hid;
      bool removed      = retro_atomic_load_seq_cst_int(&adapter->removing) != 0;
      if (     retro_atomic_fetch_sub_int(&adapter->senders, 1) == 1
            && removed && hid)
         libusb_hid_wake(hid);
   }
}

/* Releases everything an adapter owns. Only for an adapter with no
 * transfer in flight. */
static void libusb_adapter_destroy(struct libusb_adapter *adapter)
{
   if (adapter->in_transfer)
      libusb_free_transfer(adapter->in_transfer);
   if (adapter->out_transfer)
      libusb_free_transfer(adapter->out_transfer);
   if (adapter->handle)
   {
      /* Releasing an unclaimed interface is a harmless error return. */
      libusb_release_interface(adapter->handle, adapter->interface_number);
      libusb_close(adapter->handle);
   }
   /* Commands nobody sent. */
   {
      libusb_command_t *cmd;
      while ((cmd = libusb_adapter_next_command(adapter)))
         free(cmd);
   }
   free(adapter);
}

/* Frees the removed adapters whose transfers have all come back. On
 * the event-handling thread, outside any callback. Returns true when
 * none are left. */
static bool libusb_hid_reap(void)
{
   struct libusb_adapter **link = &retiring;

   while (*link)
   {
      struct libusb_adapter *adapter = *link;
      bool idle;

      idle =   !adapter->in_busy
            && !retro_atomic_load_seq_cst_int(&adapter->out_busy)
            && !retro_atomic_load_seq_cst_int(&adapter->senders);

      if (!idle)
      {
         link = &adapter->next;
         continue;
      }
      *link = adapter->next;
      libusb_adapter_destroy(adapter);
   }
   return retiring == NULL;
}

static void libusb_hid_device_add_autodetect(unsigned idx,
      const char *device_name, const char *driver_name,
      uint16_t dev_vid, uint16_t dev_pid)
{
   input_autoconfigure_connect(
         device_name,
         NULL, NULL,
         "hid",
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
      RARCH_ERR("[libusb] Error %d getting libusb config descriptor.\n", desc_ret);
      return;
   }

   for (i = 0; i < (int)config->bNumInterfaces; i++)
   {
      const struct libusb_interface *inter = &config->interface[i];

      for (j = 0; j < inter->num_altsetting; j++)
      {
         const struct libusb_interface_descriptor *interdesc =
            &inter->altsetting[j];

#if 0
         if (interdesc->bInterfaceClass == LIBUSB_CLASS_HID)
#endif
         {
            adapter->interface_number = (int)interdesc->bInterfaceNumber;

            for (k = 0; k < (int)interdesc->bNumEndpoints; k++)
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
      RARCH_ERR("[libusb] Allocation of adapter failed.\n");
      return -1;
   }

   mpsc_stack_init(&adapter->pending);
   retro_atomic_int_init(&adapter->out_busy, 0);
   retro_atomic_int_init(&adapter->queued_bytes, 0);
   retro_atomic_int_init(&adapter->senders, 0);
   retro_atomic_int_init(&adapter->removing, 0);

   rc = libusb_get_device_descriptor(dev, &desc);

   if (rc != LIBUSB_SUCCESS)
   {
      RARCH_ERR("[libusb] Error getting device descriptor.\n");
      goto error;
   }

   adapter->device = dev;

   libusb_get_description(adapter->device, adapter);

   if (adapter->endpoint_in == 0)
   {
      RARCH_ERR("[libusb] Could not find HID config for device.\n");
      goto error;
   }

   rc = libusb_open (adapter->device, &adapter->handle);

   if (rc != LIBUSB_SUCCESS)
   {
      RARCH_ERR("[libusb] Error opening device 0x%p (VID/PID: %04x:%04x).\n",
            (void*)adapter->device, desc.idVendor, desc.idProduct);
      goto error;
   }

   if (desc.iManufacturer)
   {
      libusb_get_string_descriptor_ascii(adapter->handle,
            desc.iManufacturer, adapter->manufacturer_name,
            sizeof(adapter->manufacturer_name));
#if 0
      RARCH_ERR("[libusb] Adapter manufacturer name: %s\n",
            adapter->manufacturer_name);
#endif
   }

   if (desc.iProduct)
   {
      libusb_get_string_descriptor_ascii(adapter->handle,
            desc.iProduct, adapter->name,
            sizeof(adapter->name));
#if 0
      RARCH_ERR("[libusb] Adapter name: %s\n", adapter->name);
#endif
   }

   device_name   = (const char*)adapter->name;

   if (!*(const char*)adapter->name)
      goto error;

   adapter->slot                = -1;
   adapter->in_transfer         = libusb_alloc_transfer(0);
   adapter->out_transfer        = libusb_alloc_transfer(0);

   if (!adapter->in_transfer || !adapter->out_transfer)
   {
      RARCH_ERR("[libusb] Error allocating transfers.\n");
      goto error;
   }

   adapter->slot = pad_connection_pad_init(hid->slots,
         device_name, desc.idVendor, desc.idProduct,
         adapter, &libusb_hid);

   if (adapter->slot == -1)
      goto error;

   if (!pad_connection_has_interface(hid->slots, adapter->slot))
   {
      RARCH_ERR("[libusb] Interface not found (%s) (VID/PID: %04x:%04x).\n",
         adapter->name, desc.idVendor, desc.idProduct);
      goto error;
   }

   RARCH_LOG("[libusb] Interface found: \"%s\".\n", adapter->name);

   if (libusb_kernel_driver_active(adapter->handle, 0) == 1
         && libusb_detach_kernel_driver(adapter->handle, 0))
   {
      RARCH_ERR("[libusb] Error detaching handle 0x%p from kernel.\n", adapter->handle);
      goto error;
   }

   rc = libusb_claim_interface(adapter->handle, adapter->interface_number);

   if (rc != LIBUSB_SUCCESS)
   {
      RARCH_ERR("[libusb] Error claiming interface %d.\n", adapter->interface_number);
      goto error;
   }

   RARCH_LOG("[libusb] Device 0x%p attached (VID/PID: %04x:%04x).\n",
         adapter->device, desc.idVendor, desc.idProduct);

   libusb_hid_device_add_autodetect(adapter->slot,
         device_name, libusb_hid.ident, desc.idVendor, desc.idProduct);

   adapter->hid = hid;

   if (adapter->endpoint_in_max_size > (int)sizeof(adapter->data))
      adapter->endpoint_in_max_size = (int)sizeof(adapter->data);
   libusb_fill_interrupt_transfer(adapter->in_transfer, adapter->handle,
         (unsigned char)adapter->endpoint_in, adapter->data,
         adapter->endpoint_in_max_size, libusb_adapter_in_cb, adapter, 0);
   if (libusb_submit_transfer(adapter->in_transfer) != LIBUSB_SUCCESS)
   {
      RARCH_ERR("[libusb] Error starting reads from the adapter.\n");
      goto error;
   }
   adapter->in_busy = true;

   old_head      = adapters.next;
   adapters.next = adapter;
   adapter->next = old_head;

   return 0;

error:
   /* Nothing is in flight: the read is the last thing started. */
   if (adapter->slot >= 0 && hid->slots)
      pad_connection_pad_deinit(&hid->slots[adapter->slot], adapter->slot);
   libusb_adapter_destroy(adapter);
   return -1;
}

static int remove_adapter(void *data, struct libusb_device *dev)
{
   struct libusb_adapter     *prev = &adapters;
   struct libusb_hid          *hid = (struct libusb_hid*)data;

   /* Walk the whole list: the device that left is whichever one it
    * is, not necessarily the last one plugged in.  This used to look
    * at the head only, so unplugging any pad but the most recent was
    * ignored - its thread went on issuing transfers to a device that
    * was gone, its slot stayed connected, its handle leaked. */
   for (; prev->next; prev = prev->next)
   {
      struct libusb_adapter *adapter = prev->next;

      if (adapter->device != dev)
         continue;

      /* Everything below is the removed adapter's own.  It used to
       * read slot, the send lock and the send queue from
       * the list's sentinel head instead - slot 0, NULL, NULL - so
       * it disconnected and deinitialised slot 0 whatever pad had
       * left, freed nothing, and left the real slot connected. */
      input_autoconfigure_disconnect(adapter->slot,
            (const char*)adapter->name);

      /* No new reads or sends from here on; cancel what is in
       * flight. The cancellations come back through event handling,
       * and the adapter is freed by libusb_hid_reap() once both have.
       * Nothing here waits for them - this can be running inside the
       * hotplug callback, on the very thread that delivers them. */
      retro_atomic_exchange_int(&adapter->removing, 1);
      /* A sender that claims the OUT transfer after this sees the
       * flag and sends nothing; one that claimed it before may still
       * submit after the cancel, and is reaped when that completes,
       * within its one-second timeout. */
      if (adapter->in_busy)
         libusb_cancel_transfer(adapter->in_transfer);
      if (retro_atomic_load_seq_cst_int(&adapter->out_busy))
         libusb_cancel_transfer(adapter->out_transfer);

      if (hid && hid->slots && adapter->slot >= 0)
         pad_connection_pad_deinit(&hid->slots[adapter->slot], adapter->slot);

      prev->next    = adapter->next;
      adapter->next = retiring;
      retiring      = adapter;
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
         RARCH_WARN("[libusb] Unhandled event: %d\n", event);
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

static int16_t libusb_hid_joypad_button(void *data,
      unsigned port, uint16_t joykey)
{
   input_bits_t buttons;

   if (port >= DEFAULT_MAX_PADS)
      return 0;
   libusb_hid_joypad_get_buttons(data, port, &buttons);

   /* Check hat. */
   if (GET_HAT_DIR(joykey))
      return 0;
   else if (joykey < 32)
      return (BIT256_GET(buttons, joykey) != 0);
   return 0;
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

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      int16_t val = pad_connection_get_axis(&hid->slots[port],
            port, AXIS_NEG_GET(joyaxis));

      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      int16_t val = pad_connection_get_axis(&hid->slots[port],
            port, AXIS_POS_GET(joyaxis));

      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t libusb_hid_joypad_state(
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
            && libusb_hid_joypad_button(data, port_idx, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(libusb_hid_joypad_axis(data, port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void libusb_hid_free(const void *data)
{
   libusb_hid_t *hid = (libusb_hid_t*)data;

   /* The poll thread runs the hotplug callbacks, which add to and
    * remove from the adapter list; stop it first so the list is this
    * thread's alone while it is torn down.  (It used to be joined
    * after, with the callbacks racing the loop below.) */
   if (hid->poll_thread)
   {
      hid->quit = 1;
#ifdef LIBUSB_HID_CAN_INTERRUPT
      /* Wakes the poll thread out of its wait for events, so the join
       * below returns as soon as it has seen the flag. If it is not in
       * the wait yet, its next call returns at once instead. */
      libusb_interrupt_event_handler(hid->ctx);
#endif
      sthread_join(hid->poll_thread);
   }

   /* No more arrivals while the adapters are torn down: the events
    * pumped below would otherwise deliver them. */
   if (hid->can_hotplug)
   {
      libusb_hotplug_deregister_callback(hid->ctx, hid->hp);
      hid->can_hotplug = 0;
   }

   while (adapters.next)
   {
      if (remove_adapter(hid, adapters.next->device) == -1)
      {
         /* Cannot happen - the head is on the list - but the old
          * loop would have spun here forever if it did. */
         RARCH_ERR("[libusb] Could not remove device %p.\n",
               adapters.next->device);
         break;
      }
   }

   /* The poll thread is gone, so the cancellations are delivered here.
    * Each call returns as soon as libusb has events to hand over, and
    * cancelled transfers come back promptly; the deadline only bounds
    * a device stack that never answers, whose adapters are then left
    * allocated rather than freed under a transfer still in flight. */
   if (hid->ctx)
   {
      retro_time_t deadline = cpu_features_get_time_usec() + 2000000;

      while (!libusb_hid_reap())
      {
         struct timeval timeout;
         retro_time_t left = deadline - cpu_features_get_time_usec();

         if (left <= 0)
         {
            RARCH_ERR("[libusb] Transfers still in flight at shutdown; "
                  "leaving their adapters allocated.\n");
            break;
         }
         timeout.tv_sec  = (long)(left / 1000000);
         timeout.tv_usec = (long)(left % 1000000);
         libusb_handle_events_timeout_completed(hid->ctx, &timeout, NULL);
      }
   }

   if (hid->slots)
      pad_connection_destroy(hid->slots);

   if (!retiring && hid->ctx)
      libusb_exit(hid->ctx);
   free(hid);
}

static void poll_thread(void *data)
{
   libusb_hid_t *hid = (libusb_hid_t*)data;

   while (!hid->quit)
   {
#ifdef LIBUSB_HID_CAN_INTERRUPT
      /* Block until there are events to handle - transfers, hotplug -
       * with no timeout to wake for. libusb_hid_free() sets hid->quit
       * and interrupts this wait, and libusb checks the completed flag
       * on the way out. */
      libusb_handle_events_completed(hid->ctx, &hid->quit);
      libusb_hid_reap();
#else
      /* No way to interrupt the wait: block for up to 100 ms per lap
       * and look at the flag in between. The timeout used to be zero,
       * so this thread returned immediately every call and spun a
       * core for as long as the driver was loaded. */
      struct timeval timeout;
      timeout.tv_sec  = 0;
      timeout.tv_usec = 100000;
      libusb_handle_events_timeout_completed(hid->ctx,
            &timeout, &hid->quit);
      libusb_hid_reap();
#endif
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

   if (hid->can_hotplug)
   {
      /* LIBUSB_HOTPLUG_ENUMERATE below delivers an ARRIVED for every
       * device already attached, during registration, so the initial
       * scan is the hotplug path's.  Enumerating here as well, as this
       * used to, added every pad twice: the second add_adapter()
       * opened a second handle and failed claiming the interface. */
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

   if (!hid->can_hotplug)
   {
      /* No hotplug: one scan at start is all the devices there will
       * ever be. */
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
   }

   hid->poll_thread = sthread_create(poll_thread, hid);

   if (!hid->poll_thread)
   {
      RARCH_ERR("[libusb] Error creating polling thread.");
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
   libusb_hid_joypad_state,
   libusb_hid_joypad_get_buttons,
   libusb_hid_joypad_axis,
   libusb_hid_poll,
   libusb_hid_joypad_rumble,
   libusb_hid_joypad_name,
   "libusb",
   libusb_hid_device_send_control,
};
