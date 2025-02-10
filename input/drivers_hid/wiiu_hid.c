/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include <retro_endianness.h>
#include "../include/wiiu/hid.h"
#include <wiiu/os/atomic.h>
#include <string/stdstring.h>

/* TODO/FIXME - static globals */
static wiiu_event_list events;
static wiiu_adapter_list adapters;

/* Forward declarations */
static void wiiu_hid_attach(wiiu_hid_t *hid, wiiu_attach_event *event);

static void wiiu_hid_report_hid_error(const char *msg, wiiu_adapter_t *adapter, int32_t error)
{
   int16_t hid_error_code = error & 0xffff;
   int16_t error_category = (error >> 16) & 0xffff;
   const char *device     = string_is_empty(adapter->device_name) ? "unknown" : adapter->device_name;

   switch (hid_error_code)
   {
      case -100:
         RARCH_ERR("[hid]: Invalid RM command (%s)\n", device);
         break;
      case -102:
         RARCH_ERR("[hid]: Invalid IOCTL command (%s)\n", device);
         break;
      case -103:
         RARCH_ERR("[hid]: bad vector count (%s)\n", device);
         break;
      case -104:
         RARCH_ERR("[hid]: invalid memory bank (%s)\n", device);
         break;
      case -105:
         RARCH_ERR("[hid]: invalid memory alignment (%s)\n", device);
         break;
      case -106:
         RARCH_ERR("[hid]: invalid data size (%s)\n", device);
         break;
      case -107:
         RARCH_ERR("[hid]: request cancelled (%s)\n", device);
         break;
      case -108:
         RARCH_ERR("[hid]: request timed out (%s)\n", device);
         break;
      case -109:
         RARCH_ERR("[hid]: request aborted (%s)\n", device);
         break;
      case -110:
         RARCH_ERR("[hid]: client priority error (%s)\n", device);
         break;
      case -111:
         RARCH_ERR("[hid]: invalid device handle (%s)\n", device);
         break;
      default:
#if 0
         RARCH_ERR("[hid]: Unknown error (%d:%d: %s)\n",
            error_category, hid_error_code, device);
#else
         (void)error_category;
         break;
#endif
   }
}

static bool wiiu_hid_joypad_query(void *data, unsigned slot)
{
   wiiu_hid_t *hid = (wiiu_hid_t *)data;
   if (!hid)
      return false;

   return slot < joypad_state.max_slot;
}

static joypad_connection_t *wiiu_hid_get_pad(wiiu_hid_t *hid, unsigned slot)
{
   joypad_connection_t *result;
   if (!wiiu_hid_joypad_query(hid, slot))
      return NULL;
   result = &joypad_state.pads[slot];
   if (!result->connected || !result->iface || !result->connection)
      return NULL;
   return result;
}

static const char *wiiu_hid_joypad_name(void *data, unsigned slot)
{
   joypad_connection_t *pad = wiiu_hid_get_pad((wiiu_hid_t *)data, slot);

   if (!pad || !pad->iface->get_name)
      return NULL;

   return pad->iface->get_name(pad->connection);
}

static void wiiu_hid_joypad_get_buttons(void *data, unsigned slot, input_bits_t *state)
{
   joypad_connection_t *pad = wiiu_hid_get_pad((wiiu_hid_t *)data, slot);

   if (pad && pad->iface->get_buttons)
      pad->iface->get_buttons(pad->connection, state);
}

static int16_t wiiu_hid_joypad_button(void *data,
      unsigned slot, uint16_t joykey)
{
   joypad_connection_t *pad             = wiiu_hid_get_pad((wiiu_hid_t *)data, slot);
   if (!pad || !pad->iface->button)
      return 0;
   return pad->iface->button(pad->connection, joykey);
}

static int16_t wiiu_hid_joypad_axis(void *data, unsigned slot, uint32_t joyaxis)
{
   joypad_connection_t *pad = wiiu_hid_get_pad((wiiu_hid_t *)data, slot);

   if (pad)
   {
      if (AXIS_NEG_GET(joyaxis) < 4)
      {
         int16_t val = pad->iface->get_axis(pad->connection, AXIS_NEG_GET(joyaxis));
         if (val < 0)
            return val;
      }
      else if (AXIS_POS_GET(joyaxis) < 4)
      {
         int16_t val = pad->iface->get_axis(pad->connection, AXIS_POS_GET(joyaxis));
         if (val > 0)
            return val;
      }
   }
   return 0;
}

static int16_t wiiu_hid_joypad_state(void *data,
      rarch_joypad_info_t *joypad_info,
      const void *binds_data,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   const struct retro_keybind *binds    = (const struct retro_keybind*)
      binds_data;
   uint16_t port_idx                    = joypad_info->joy_idx;
   joypad_connection_t *pad             = wiiu_hid_get_pad((wiiu_hid_t *)data, port_idx);
   if (!pad)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN
            && pad->iface->button && pad->iface->button(pad->connection, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE && pad->iface->get_axis &&
            ((float)abs(pad->iface->get_axis(pad->connection, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static bool wiiu_hid_joypad_rumble(void *data, unsigned slot,
      enum retro_rumble_effect effect, uint16_t strength)
{
   joypad_connection_t *pad = wiiu_hid_get_pad((wiiu_hid_t *)data, slot);

   if (!pad || !pad->iface->set_rumble)
      return false;

   pad->iface->set_rumble(pad->connection, effect, strength);
   return false;
}

static void *wiiu_hid_alloc_zeroed(size_t alignment, size_t len)
{
   void *result = memalign(alignment, len);
   if (result)
      memset(result, 0, len);
   return result;
}

static void wiiu_hid_init_lists(void)
{
   memset(&events, 0, sizeof(events));
   OSFastMutex_Init(&(events.lock), "attach_events");
   memset(&adapters, 0, sizeof(adapters));
   OSFastMutex_Init(&(adapters.lock), "adapters");
}

static void wiiu_hid_delete_adapter(wiiu_adapter_t *adapter)
{
   if (!adapter)
      return;

   if (adapter->rx_buffer)
   {
      free(adapter->rx_buffer);
      adapter->rx_buffer = NULL;
   }
   if (adapter->tx_buffer)
   {
      free(adapter->tx_buffer);
      adapter->tx_buffer = NULL;
   }

   if (adapter->pad_driver && adapter->pad_driver_data)
   {
      adapter->pad_driver->deinit(adapter->pad_driver_data);
      adapter->pad_driver_data = NULL;
   }

   free(adapter);
}


/**
 * Block until all the HIDRead() calls have returned.
 */
static void wiiu_hid_polling_thread_cleanup(OSThread *thread, void *stack)
{
   int incomplete          = 0;
   int retries             = 0;
   wiiu_adapter_t *adapter = NULL;

   RARCH_LOG("Waiting for in-flight reads to finish.\n");

   /* We don't need to protect the adapter list here because nothing else
      will access it during this method (the HID system is shut down, and
      the only other access is the polling thread that just stopped */
   do
   {
      incomplete = 0;
      for (adapter = adapters.list; adapter != NULL; adapter = adapter->next)
      {
         if (adapter->state == ADAPTER_STATE_READING)
            incomplete++;
      }

      if (incomplete == 0)
      {
         RARCH_LOG("All in-flight reads complete.\n");
         while (adapters.list)
         {
            RARCH_LOG("[hid]: shutting down adapter..\n");
            adapter = adapters.list;
            adapters.list = adapter->next;
            pad_connection_pad_deregister(joypad_state.pads, adapter->pad_driver, adapter->pad_driver_data);
            wiiu_hid_delete_adapter(adapter);
         }
      }

      if (incomplete)
         usleep(5000);

      if (++retries >= 1000)
      {
         RARCH_WARN("[hid]: timed out waiting for in-flight read to finish.\n");
         incomplete = 0;
      }
   } while (incomplete);
}

static OSThread *wiiu_hid_new_thread(void)
{
   OSThread *t = wiiu_hid_alloc_zeroed(8, sizeof(OSThread));

   if (!t)
      return NULL;

   t->tag      = OS_THREAD_TAG;

   return t;
}

static wiiu_adapter_t *wiiu_hid_synchronized_lookup_adapter(uint32_t handle)
{
   wiiu_adapter_t *iterator;

   OSFastMutex_Lock(&(adapters.lock));

   for (iterator = adapters.list; iterator != NULL; iterator = iterator->next)
   {
      if (iterator->handle == handle)
         break;
   }
   OSFastMutex_Unlock(&(adapters.lock));

   return iterator;
}

static void wiiu_hid_detach(wiiu_hid_t *hid, wiiu_attach_event *event)
{
   wiiu_adapter_t *adapter = wiiu_hid_synchronized_lookup_adapter(event->handle);

   /* this will signal the read loop to stop for this adapter
    * the read loop method will update this to ADAPTER_STATE_GC
    * so the adapter poll method can clean it up. */
   if (adapter)
      adapter->connected = false;
}

static void wiiu_handle_attach_events(wiiu_hid_t *hid, wiiu_attach_event *list)
{
   wiiu_attach_event *event, *event_next = NULL;

   if (!hid || !list)
      return;

   for (event = list; event != NULL; event = event_next)
   {
      event_next  = event->next;
      if (event->type == HID_DEVICE_ATTACH)
         wiiu_hid_attach(hid, event);
      else
         wiiu_hid_detach(hid, event);
      if (event)
         free(event);
   }
}

static void wiiu_hid_read_loop_callback(uint32_t handle, int32_t error,
              uint8_t *buffer, uint32_t buffer_size, void *userdata)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)userdata;
   if (!adapter)
   {
      RARCH_ERR("read_loop_callback: bad userdata\n");
      return;
   }

   if (error < 0)
      wiiu_hid_report_hid_error("async read failed", adapter, error);

   if (adapter->state == ADAPTER_STATE_READING)
   {
      adapter->state = ADAPTER_STATE_READY;

      if (error == 0)
         adapter->pad_driver->packet_handler(adapter->pad_driver_data, buffer, buffer_size);
   }
}


static void wiiu_poll_adapter(wiiu_adapter_t *adapter)
{
   if (!adapter->connected)
   {
      adapter->state = ADAPTER_STATE_DONE;
      return;
   }

   adapter->state = ADAPTER_STATE_READING;
   HIDRead(adapter->handle, adapter->rx_buffer, adapter->rx_size,
      wiiu_hid_read_loop_callback, adapter);
}

static void wiiu_poll_adapters(wiiu_hid_t *hid)
{
   wiiu_adapter_t *it;
   OSFastMutex_Lock(&(adapters.lock));

   for (it = adapters.list; it != NULL; it = it->next)
   {
      if (it->state == ADAPTER_STATE_READY)
         wiiu_poll_adapter(it);

      if (it->state == ADAPTER_STATE_DONE)
         it->state = ADAPTER_STATE_GC;
   }

   OSFastMutex_Unlock(&(adapters.lock));
}

static wiiu_attach_event *wiiu_hid_synchronized_get_events_list(void)
{
   return (wiiu_attach_event *)SwapAtomic32((uint32_t *)&events.list, 0);
}

static int wiiu_hid_polling_thread(int argc, const char **argv)
{
   wiiu_hid_t *hid = (wiiu_hid_t *)argv;

   while (!hid->polling_thread_quit)
   {
      wiiu_handle_attach_events(hid, wiiu_hid_synchronized_get_events_list());
      wiiu_poll_adapters(hid);
   }

   return 0;
}

static void wiiu_hid_start_polling_thread(wiiu_hid_t *hid)
{
   OSThreadAttributes attributes = OS_THREAD_ATTRIB_AFFINITY_CPU2;
   int32_t stack_size            = 0x8000;
   int32_t priority              = 10;
   OSThread *thread              = wiiu_hid_new_thread();
   void *stack                   = wiiu_hid_alloc_zeroed(16, stack_size);

   RARCH_LOG("[hid]: starting polling thread.\n");

   if (!thread || !stack)
      goto error;

   if (!OSCreateThread(thread,
            wiiu_hid_polling_thread,
            1, (char *)hid,
            stack+stack_size, stack_size,
            priority,
            attributes))
   {
      RARCH_LOG("[hid]: OSCreateThread failed.\n");
      goto error;
   }

   OSSetThreadCleanupCallback(thread, wiiu_hid_polling_thread_cleanup);

   hid->polling_thread       = thread;
   hid->polling_thread_stack = stack;
   OSResumeThread(thread);
   return;

error:
   if (thread)
      free(thread);
   if (stack)
      free(stack);

   return;
}

static void wiiu_hid_stop_polling_thread(wiiu_hid_t *hid)
{
   int thread_result = 0;

   if (!hid || !hid->polling_thread)
      return;

   /* Unregister our HID client so we don't get any new events. */
   if (hid->client)
   {
     HIDDelClient(hid->client);
     hid->client = NULL;
   }

   /* tell the thread it's time to stop. */
   hid->polling_thread_quit = true;
   /* This returns once the thread runs and the cleanup method completes. */
   OSJoinThread(hid->polling_thread, &thread_result);
   free(hid->polling_thread);
   free(hid->polling_thread_stack);
   hid->polling_thread = NULL;
   hid->polling_thread_stack = NULL;
}

static void wiiu_hid_synchronized_add_event(wiiu_attach_event *event)
{
   wiiu_attach_event *head = (wiiu_attach_event *)SwapAtomic32((uint32_t *)&events.list, 0);
   event->next             = head;
   head                    = event;
   SwapAtomic32((uint32_t *)&events.list, (uint32_t)head);
}

static void wiiu_hid_synchronized_add_to_adapters_list(wiiu_adapter_t *adapter)
{
   OSFastMutex_Lock(&(adapters.lock));
   adapter->next = adapters.list;
   adapters.list = adapter;
   OSFastMutex_Unlock(&(adapters.lock));
}

static void wiiu_hid_log_device(HIDDevice *device)
{
   RARCH_LOG("                handle: %d\n",     device->handle);
   RARCH_LOG("  physical_device_inst: %d\n",     device->physical_device_inst);
   RARCH_LOG("                   vid: 0x%04x\n", SWAP_IF_BIG(device->vid));
   RARCH_LOG("                   pid: 0x%04x\n", SWAP_IF_BIG(device->pid));
   RARCH_LOG("       interface_index: %d\n",     device->interface_index);
   RARCH_LOG("             sub_class: %d\n",     device->sub_class);
   RARCH_LOG("              protocol: %d\n",     device->protocol);
   RARCH_LOG("    max_packet_size_rx: %d\n",     device->max_packet_size_rx);
   RARCH_LOG("    max_packet_size_tx: %d\n",     device->max_packet_size_tx);
}

static void wiiu_hid_get_device_name(HIDDevice *device, wiiu_attach_event *event)
{
   uint8_t name_buffer_size = 46; /* enough to detect WiiU Pro controller */
   uint8_t *name_buffer     = wiiu_hid_alloc_zeroed(4, name_buffer_size);

   if (name_buffer)
   {
      /* HIDGetDescriptor() fills name_buffer in this way:
       * - First two bytes are empty
       * - Every second byte is empty
       * - Maximum name_buffer size is unknown (with 63 it starts to fail with one of my controllers)
       * - Truncates device names if name_buffer is too small */
      uint8_t *top   = &event->device_name[0];
      int32_t result = HIDGetDescriptor(device->handle, 3, 2, 0, name_buffer, name_buffer_size, NULL, NULL);
      if (result > 0)
      {
         int i;
         for(i = 2; i < result; i += 2)
         {
            top[0] = name_buffer[i];
            top++;
         }
      }
      free(name_buffer);
   }
}


static wiiu_attach_event *wiiu_hid_new_attach_event(HIDDevice *device)
{
   wiiu_attach_event *event = NULL;
   /* Ignore mice and keyboards as HID devices */
   if (device->protocol > 0)
      return NULL;
   if (!(event = wiiu_hid_alloc_zeroed(4, sizeof(wiiu_attach_event))))
      return NULL;

   event->handle             = device->handle;
   event->vendor_id          = device->vid;
   event->product_id         = device->pid;
   event->interface_index    = device->interface_index;
   event->is_keyboard        = (device->sub_class == 1
         && device->protocol == 1);
   event->is_mouse           = (device->sub_class == 1
         && device->protocol == 2);
   event->max_packet_size_rx = device->max_packet_size_rx;
   event->max_packet_size_tx = device->max_packet_size_tx;
   wiiu_hid_get_device_name(device, event);

   return event;
}

static int32_t wiiu_attach_callback(HIDClient *client,
      HIDDevice *device, uint32_t attach)
{
   wiiu_attach_event *event = NULL;

   if (device)
   {
      if (attach)
         wiiu_hid_log_device(device);
      if (!(event = wiiu_hid_new_attach_event(device)))
         return DEVICE_UNUSED;
   }
   else
   {
      if (attach)
         RARCH_LOG("NULL device.\n");
      return DEVICE_UNUSED;
   }

   event->type = attach;
   wiiu_hid_synchronized_add_event(event);

   return DEVICE_USED;
}

static void *wiiu_hid_init(void)
{
   wiiu_hid_t *hid   = wiiu_hid_alloc_zeroed(4, sizeof(wiiu_hid_t));
   HIDClient *client = wiiu_hid_alloc_zeroed(32, sizeof(HIDClient));

   if (!hid || !client)
      goto error;

   wiiu_hid_init_lists();
   wiiu_hid_start_polling_thread(hid);
   if (!hid->polling_thread)
      goto error;

   HIDAddClient(client, wiiu_attach_callback);
   hid->client = client;

   return hid;

error:
   wiiu_hid_stop_polling_thread(hid);
   if (hid)
      free(hid);
   if (client)
      free(client);
   return NULL;
}

static void wiiu_hid_free(const void *data)
{
   wiiu_hid_t *hid = (wiiu_hid_t*)data;

   if (!hid)
      return;

   wiiu_hid_stop_polling_thread(hid);
   if (hid->client)
      free(hid->client);
   free(hid);

   if (events.list)
   {
      wiiu_attach_event *event = NULL;
      while ((event = events.list))
      {
         events.list = event->next;
         if (event)
            free(event);
      }
      memset(&events, 0, sizeof(events));
   }
}

static uint8_t wiiu_hid_try_init_driver_multi(wiiu_adapter_t *adapter, joypad_connection_entry_t *entry)
{
   adapter->pad_driver_data = entry->iface->init(adapter, -1, &wiiu_hid);
   if (!adapter->pad_driver_data)
      return ADAPTER_STATE_DONE;
   pad_connection_pad_register(joypad_state.pads, adapter->pad_driver, adapter->pad_driver_data, adapter, &hidpad_driver, SLOT_AUTO);
   return ADAPTER_STATE_READY;
}

static uint8_t wiiu_hid_try_init_driver(wiiu_adapter_t *adapter)
{
   int slot;
   joypad_connection_entry_t *entry = find_connection_entry(adapter->vendor_id, adapter->product_id, adapter->device_name);
   if (!entry)
   {
      RARCH_LOG("Failed to find entry for vid: 0x%04x, pid: 0x%04x, name: %s\n",
            SWAP_IF_BIG(adapter->vendor_id),
            SWAP_IF_BIG(adapter->product_id),
            adapter->device_name);
      return ADAPTER_STATE_DONE;
   }

   RARCH_LOG("Found entry for: vid: 0x%04x, pid: 0x%04x, name: %s\n",
         SWAP_IF_BIG(adapter->vendor_id),
         SWAP_IF_BIG(adapter->product_id),
         adapter->device_name);

   adapter->pad_driver = entry->iface;

   if (entry->iface->multi_pad)
      return wiiu_hid_try_init_driver_multi(adapter, entry);

   slot = pad_connection_find_vacant_pad(joypad_state.pads);
   if (slot < 0)
   {
      RARCH_LOG("wiiu_hid_try_init_driver: no slot available\n");
      return ADAPTER_STATE_DONE;
   }

   adapter->pad_driver_data = entry->iface->init(adapter, slot, &wiiu_hid);

   if (!adapter->pad_driver_data)
   {
      RARCH_LOG("wiiu_hid_try_init_driver: pad init failed\n");
      return ADAPTER_STATE_DONE;
   }

   pad_connection_pad_register(joypad_state.pads, adapter->pad_driver, adapter->pad_driver_data, adapter, &hidpad_driver, slot);

   return ADAPTER_STATE_READY;
}


static void synchronized_process_adapters(wiiu_hid_t *hid)
{
   wiiu_adapter_t *adapter = NULL;
   wiiu_adapter_t *prev    = NULL, *adapter_next = NULL;
   bool keep_prev          = false;

   OSFastMutex_Lock(&(adapters.lock));

   for (adapter = adapters.list; adapter != NULL; adapter = adapter_next)
   {
      adapter_next = adapter->next;

      switch (adapter->state)
      {
         case ADAPTER_STATE_NEW:
            adapter->state = wiiu_hid_try_init_driver(adapter);
            break;
         case ADAPTER_STATE_READY:
         case ADAPTER_STATE_READING:
            if (adapter->pad_driver && adapter->pad_driver->multi_pad)
               pad_connection_pad_refresh(joypad_state.pads, adapter->pad_driver, adapter->pad_driver_data, adapter, &hidpad_driver);
            break;
         case ADAPTER_STATE_DONE:
            break;
         case ADAPTER_STATE_GC:
            {
               /* remove from the list */
               if (!prev)
                  adapters.list = adapter->next;
               else
                  prev->next = adapter->next;

               if (adapter->pad_driver && adapter->pad_driver_data)
                  pad_connection_pad_deregister(joypad_state.pads, adapter->pad_driver, adapter->pad_driver_data);
               /* adapter is no longer valid after this point */
               wiiu_hid_delete_adapter(adapter);
               /* signal not to update prev ptr since adapter is now invalid */
               keep_prev = true;
            }
            break;

         default:
            RARCH_ERR("[hid]: Invalid adapter state: %d\n", adapter->state);
            break;
      }
      if (!keep_prev)
         prev   = adapter;
      keep_prev = false;
   }
   OSFastMutex_Unlock(&(adapters.lock));
}

static void wiiu_hid_poll(void *data)
{
   wiiu_hid_t *hid = (wiiu_hid_t *)data;
   if (!hid)
      return;

   synchronized_process_adapters(hid);
}

static void wiiu_hid_send_control(void *data, uint8_t *s, size_t len)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   int32_t result;

   if (!adapter)
   {
      RARCH_ERR("[hid]: send_control: bad adapter.\n");
      return;
   }

   memset(adapter->tx_buffer, 0, adapter->tx_size);
   memcpy(adapter->tx_buffer, s, len);

   /* From testing, HIDWrite returns an error that looks like it's two
    * int16_t's bitmasked together. For example, one error I saw when trying
    * to write a single byte was 0xffe2ff97, which works out to -30 and -105.
    *  I have no idea what these mean. */
   result = HIDWrite(adapter->handle, adapter->tx_buffer, adapter->tx_size, NULL, NULL);
   if (result < 0)
   {
      int16_t r1 =  (result & 0x0000FFFF);
      int16_t r2 = ((result & 0xFFFF0000) >> 16);
      RARCH_LOG("[hid]: write failed: %08x (%d:%d)\n", result, r2, r1);
   }
}

static void _fixup_report_buffer(uint8_t **s, uint8_t report_id, size_t *len)
{
   if ((*s)[0] == report_id)
   {
      *s   = (*s)+ 1;
      *len = *len - 1;
   }
}

static int32_t wiiu_hid_set_report(void *data, uint8_t report_type,
               uint8_t report_id, uint8_t *report_data, size_t report_length)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   if (!adapter || report_length > adapter->tx_size)
      return -1;

   _fixup_report_buffer(&report_data, report_id, &report_length);

   memset(adapter->tx_buffer, 0, adapter->tx_size);
   memcpy(adapter->tx_buffer, report_data, report_length);

   return HIDSetReport(adapter->handle,
         report_type,
         report_id,
         adapter->tx_buffer,
         adapter->tx_size,
         NULL, NULL);
}

static int32_t wiiu_hid_get_report(void *handle, uint8_t report_type, uint8_t report_id,
      uint8_t *report_data, size_t report_length)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)handle;
   if (!adapter || report_length > adapter->tx_size)
      return -1;

   _fixup_report_buffer(&report_data, report_id, &report_length);

   memset(adapter->tx_buffer, 0, adapter->tx_size);
   memcpy(adapter->tx_buffer, report_data, report_length);

   return HIDGetReport(adapter->handle,
         report_type,
         report_id,
         adapter->tx_buffer,
         adapter->tx_size,
         NULL, NULL);
}

static int32_t wiiu_hid_set_idle(void *data, uint8_t duration)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   if (!adapter)
      return -1;

   return HIDSetIdle(adapter->handle,
         adapter->interface_index,
         duration,
         NULL, NULL);
}

static int32_t wiiu_hid_set_protocol(void *data, uint8_t protocol)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   if (!adapter)
      return -1;

   return HIDSetProtocol(adapter->handle,
         adapter->interface_index,
         protocol,
         NULL, NULL);
}

static int32_t wiiu_hid_read(void *data, void *buffer, size_t len)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   int32_t result;
   if (!adapter)
      return -1;
   if (len > adapter->rx_size)
      return -1;
   if ((result = HIDRead(adapter->handle, buffer, len, NULL, NULL)) < 0)
      wiiu_hid_report_hid_error("read failed", adapter, result);
   return result;
}

static void wiiu_hid_init_cachealigned_buffer(int32_t min_size,
      uint8_t **out_buf_ptr, int32_t *actual_size)
{
   *actual_size = (min_size + 0x3f) & ~0x3f;
   *out_buf_ptr = wiiu_hid_alloc_zeroed(64, *actual_size);
}

static wiiu_adapter_t *wiiu_hid_new_adapter(wiiu_attach_event *event)
{
   wiiu_adapter_t *adapter  = wiiu_hid_alloc_zeroed(32, sizeof(wiiu_adapter_t));

   if (!adapter)
      return NULL;

   adapter->handle          = event->handle;
   adapter->interface_index = event->interface_index;
   adapter->product_id      = event->product_id;
   adapter->vendor_id       = event->vendor_id;
   wiiu_hid_init_cachealigned_buffer(event->max_packet_size_rx, &adapter->rx_buffer, &adapter->rx_size);
   wiiu_hid_init_cachealigned_buffer(event->max_packet_size_tx, &adapter->tx_buffer, &adapter->tx_size);
   memcpy(adapter->device_name, event->device_name, sizeof(adapter->device_name));
   adapter->connected       = true;

   return adapter;
}

static void wiiu_hid_attach(wiiu_hid_t *hid, wiiu_attach_event *event)
{
   wiiu_adapter_t *adapter = wiiu_hid_new_adapter(event);

   if (!adapter)
   {
      RARCH_ERR("[hid]: Failed to allocate adapter.\n");
      wiiu_hid_delete_adapter(adapter);
      return;
   }

   adapter->hid        = hid;
   adapter->vendor_id  = event->vendor_id;
   adapter->product_id = event->product_id;
   adapter->state      = ADAPTER_STATE_NEW;

   wiiu_hid_synchronized_add_to_adapters_list(adapter);
}

hid_driver_t wiiu_hid = {
   wiiu_hid_init,
   wiiu_hid_joypad_query,
   wiiu_hid_free,
   wiiu_hid_joypad_button,
   wiiu_hid_joypad_state,
   wiiu_hid_joypad_get_buttons,
   wiiu_hid_joypad_axis,
   wiiu_hid_poll,
   wiiu_hid_joypad_rumble,
   wiiu_hid_joypad_name,
   "wiiu",
   wiiu_hid_send_control,
   wiiu_hid_set_report,
   wiiu_hid_get_report,
   wiiu_hid_set_idle,
   wiiu_hid_set_protocol,
   wiiu_hid_read,
};
