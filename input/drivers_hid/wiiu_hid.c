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

#include "../include/wiiu/hid.h"
#include <wiiu/os/atomic.h>

static wiiu_event_list events;
static wiiu_adapter_list adapters;

static void report_hid_error(const char *msg, wiiu_adapter_t *adapter, int32_t error);

static bool wiiu_hid_joypad_query(void *data, unsigned slot)
{
   wiiu_hid_t *hid = (wiiu_hid_t *)data;
   if (!hid)
      return false;

   return slot < HID_MAX_SLOT();
}

static joypad_connection_t *get_pad(wiiu_hid_t *hid, unsigned slot)
{
   if (!wiiu_hid_joypad_query(hid, slot))
      return NULL;

   joypad_connection_t *result = HID_PAD_CONNECTION_PTR(slot);
   if (!result || !result->connected || !result->iface || !result->data)
      return NULL;

   return result;
}

static const char *wiiu_hid_joypad_name(void *data, unsigned slot)
{
   joypad_connection_t *pad = get_pad((wiiu_hid_t *)data, slot);

   if (!pad)
      return NULL;

   return pad->iface->get_name(pad->data);
}

static void wiiu_hid_joypad_get_buttons(void *data, unsigned slot, input_bits_t *state)
{
   joypad_connection_t *pad = get_pad((wiiu_hid_t *)data, slot);

   if (pad)
      pad->iface->get_buttons(pad->data, state);
}

static bool wiiu_hid_joypad_button(void *data, unsigned slot, uint16_t joykey)
{
   joypad_connection_t *pad = get_pad((wiiu_hid_t *)data, slot);

   if (!pad)
      return false;

   return pad->iface->button(pad->data, joykey);
}

static bool wiiu_hid_joypad_rumble(void *data, unsigned slot,
      enum retro_rumble_effect effect, uint16_t strength)
{
   joypad_connection_t *pad = get_pad((wiiu_hid_t *)data, slot);

   if (!pad)
      return false;

   pad->iface->set_rumble(pad->data, effect, strength);
   return false;
}

static int16_t wiiu_hid_joypad_axis(void *data, unsigned slot, uint32_t joyaxis)
{
   joypad_connection_t *pad = get_pad((wiiu_hid_t *)data, slot);

   if (!pad)
      return 0;

   return pad->iface->get_axis(pad->data, joyaxis);
}

static void *wiiu_hid_init(void)
{
   RARCH_LOG("[hid]: initializing HID subsystem\n");
   wiiu_hid_t *hid = new_hid();
   HIDClient *client = new_hidclient();

   if (!hid || !client)
      goto error;

   wiiu_hid_init_lists();
   start_polling_thread(hid);
   if (!hid->polling_thread)
      goto error;

   HIDAddClient(client, wiiu_attach_callback);
   hid->client = client;

   RARCH_LOG("[hid]: init success\n");
   return hid;

error:
   RARCH_LOG("[hid]: initialization failed. cleaning up.\n");

   stop_polling_thread(hid);
   delete_hid(hid);
   delete_hidclient(client);
   return NULL;
}

static void wiiu_hid_free(const void *data)
{
   wiiu_hid_t *hid = (wiiu_hid_t*)data;

   if (!hid)
      return;

   stop_polling_thread(hid);
   delete_hidclient(hid->client);
   delete_hid(hid);

   if (events.list)
   {
      wiiu_attach_event *event = NULL;
      while( (event = events.list) != NULL)
      {
         events.list = event->next;
         delete_attach_event(event);
      }
      memset(&events, 0, sizeof(events));
   }
}

static void wiiu_hid_poll(void *data)
{
   wiiu_hid_t *hid = (wiiu_hid_t *)data;
   if (!hid)
      return;

   synchronized_process_adapters(hid);
}

static void wiiu_hid_send_control(void *data, uint8_t *buf, size_t size)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   int32_t result;

   if (!adapter)
   {
      RARCH_ERR("[hid]: send_control: bad adapter.\n");
      return;
   }

   memset(adapter->tx_buffer, 0, adapter->tx_size);
   memcpy(adapter->tx_buffer, buf, size);

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

static int32_t wiiu_hid_set_report(void *data, uint8_t report_type,
               uint8_t report_id, void *report_data, uint32_t report_length)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   if (!adapter || report_length > adapter->tx_size)
      return -1;

   memset(adapter->tx_buffer, 0, adapter->tx_size);
   memcpy(adapter->tx_buffer, report_data, report_length);

   return HIDSetReport(adapter->handle,
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

static int32_t wiiu_hid_read(void *data, void *buffer, size_t size)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   int32_t result;

   if (!adapter)
      return -1;

   if (size > adapter->rx_size)
      return -1;

   result = HIDRead(adapter->handle, buffer, size, NULL, NULL);
   if (result < 0)
      report_hid_error("read failed", adapter, result);

   return result;
}

static void start_polling_thread(wiiu_hid_t *hid)
{
   OSThreadAttributes attributes = OS_THREAD_ATTRIB_AFFINITY_CPU2;
   BOOL result                   = false;
   int32_t stack_size            = 0x8000;
   int32_t priority              = 10;
   OSThread *thread              = new_thread();
   void *stack                   = alloc_zeroed(16, stack_size);

   RARCH_LOG("[hid]: starting polling thread.\n");

   if (!thread || !stack)
   {
      RARCH_LOG("[hid]: allocation failed, aborting thread start.\n");
      goto error;
   }

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

static void stop_polling_thread(wiiu_hid_t *hid)
{
   int thread_result = 0;
   RARCH_LOG("[hid]: stopping polling thread.\n");

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

static void log_device(HIDDevice *device)
{
   if (!device)
   {
      RARCH_LOG("NULL device.\n");
   }

   RARCH_LOG("                handle: %d\n", device->handle);
   RARCH_LOG("  physical_device_inst: %d\n", device->physical_device_inst);
   RARCH_LOG("                   vid: 0x%x\n", device->vid);
   RARCH_LOG("                   pid: 0x%x\n", device->pid);
   RARCH_LOG("       interface_index: %d\n", device->interface_index);
   RARCH_LOG("             sub_class: %d\n", device->sub_class);
   RARCH_LOG("              protocol: %d\n", device->protocol);
   RARCH_LOG("    max_packet_size_rx: %d\n", device->max_packet_size_rx);
   RARCH_LOG("    max_packet_size_tx: %d\n", device->max_packet_size_tx);
}

static uint8_t try_init_driver(wiiu_adapter_t *adapter)
{
   adapter->driver_handle = adapter->driver->init(adapter);
   if (adapter->driver_handle == NULL)
   {
     RARCH_ERR("[hid]: Failed to initialize driver: %s\n",
        adapter->driver->name);
     return ADAPTER_STATE_DONE;
   }

   return ADAPTER_STATE_READY;
}

static void synchronized_process_adapters(wiiu_hid_t *hid)
{
   wiiu_adapter_t *adapter = NULL;
   wiiu_adapter_t *prev = NULL, *adapter_next = NULL;
   bool keep_prev = false;

   OSFastMutex_Lock(&(adapters.lock));

   for (adapter = adapters.list; adapter != NULL; adapter = adapter_next)
   {
     adapter_next = adapter->next;

     switch(adapter->state)
     {
       case ADAPTER_STATE_NEW:
          adapter->state = try_init_driver(adapter);
          break;
       case ADAPTER_STATE_READY:
       case ADAPTER_STATE_READING:
       case ADAPTER_STATE_DONE:
          break;
       case ADAPTER_STATE_GC:
          /* remove from the list */
          if (!prev)
             adapters.list = adapter->next;
          else
             prev->next = adapter->next;

          /* adapter is no longer valid after this point */
          delete_adapter(adapter);
          /* signal not to update prev ptr since adapter is now invalid */
          keep_prev = true;
          break;
       default:
          RARCH_ERR("[hid]: Invalid adapter state: %d\n", adapter->state);
          break;
     }
     prev = keep_prev ? prev : adapter;
     keep_prev = false;
   }
   OSFastMutex_Unlock(&(adapters.lock));
}

static void synchronized_add_event(wiiu_attach_event *event)
{
   wiiu_attach_event *head = (wiiu_attach_event *)SwapAtomic32((uint32_t *)&events.list, 0);

   event->next = head;
   head = event;

   SwapAtomic32((uint32_t *)&events.list, (uint32_t)head);
}

static wiiu_attach_event *synchronized_get_events_list(void)
{
   return (wiiu_attach_event *)SwapAtomic32((uint32_t *)&events.list, 0);
}

static wiiu_adapter_t *synchronized_lookup_adapter(uint32_t handle)
{
   OSFastMutex_Lock(&(adapters.lock));
   wiiu_adapter_t *iterator;

   for (iterator = adapters.list; iterator != NULL; iterator = iterator->next)
   {
      if (iterator->handle == handle)
         break;
   }
   OSFastMutex_Unlock(&(adapters.lock));

   return iterator;
}

static void synchronized_add_to_adapters_list(wiiu_adapter_t *adapter)
{
   OSFastMutex_Lock(&(adapters.lock));
   adapter->next = adapters.list;
   adapters.list = adapter;
   OSFastMutex_Unlock(&(adapters.lock));
}

static int32_t wiiu_attach_callback(HIDClient *client,
      HIDDevice *device, uint32_t attach)
{
   wiiu_attach_event *event = NULL;

   if (attach)
   {
      RARCH_LOG("[hid]: Device attach event generated.\n");
      log_device(device);
   }
   else
   {
      RARCH_LOG("[hid]: Device detach event generated.\n");
   }

   if (device)
      event = new_attach_event(device);

   if (!event)
      goto error;

   event->type = attach;
   synchronized_add_event(event);

   return DEVICE_USED;

error:
   delete_attach_event(event);
   return DEVICE_UNUSED;
}

static void wiiu_hid_detach(wiiu_hid_t *hid, wiiu_attach_event *event)
{
   wiiu_adapter_t *adapter = synchronized_lookup_adapter(event->handle);

   /* this will signal the read loop to stop for this adapter
    * the read loop method will update this to ADAPTER_STATE_GC
    * so the adapter poll method can clean it up. */
   if (adapter)
      adapter->connected = false;
}

static void wiiu_hid_attach(wiiu_hid_t *hid, wiiu_attach_event *event)
{
   wiiu_adapter_t *adapter = new_adapter(event);

   if (!adapter)
   {
      RARCH_ERR("[hid]: Failed to allocate adapter.\n");
      goto error;
   }

   adapter->hid    = hid;
   adapter->driver = event->driver;
   adapter->state  = ADAPTER_STATE_NEW;

   synchronized_add_to_adapters_list(adapter);

   return;

error:
   delete_adapter(adapter);
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
   {
      report_hid_error("async read failed", adapter, error);
   }

   if (adapter->state == ADAPTER_STATE_READING)
   {
      adapter->state = ADAPTER_STATE_READY;

      if (error == 0)
      {
         adapter->driver->handle_packet(adapter->driver_handle,
            buffer, buffer_size);
      }
   }
}

static void report_hid_error(const char *msg, wiiu_adapter_t *adapter, int32_t error)
{
   if (error >= 0)
      return;

   int16_t hid_error_code = error & 0xffff;
   int16_t error_category = (error >> 16) & 0xffff;
   const char *device = (adapter && adapter->driver) ? adapter->driver->name : "unknown";

   switch(hid_error_code)
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
#if 0
      default:
         RARCH_ERR("[hid]: Unknown error (%d:%d: %s)\n",
            error_category, hid_error_code, device);
#endif
   }
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
         while(adapters.list != NULL)
         {
            RARCH_LOG("[hid]: shutting down adapter..\n");
            adapter = adapters.list;
            adapters.list = adapter->next;
            delete_adapter(adapter);
         }
      }

      if (incomplete)
         usleep(5000);

      if (++retries >= 1000)
      {
         RARCH_WARN("[hid]: timed out waiting for in-flight read to finish.\n");
         incomplete = 0;
      }
   } while(incomplete);
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
      delete_attach_event(event);
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

static int wiiu_hid_polling_thread(int argc, const char **argv)
{
   wiiu_hid_t *hid = (wiiu_hid_t *)argv;

   RARCH_LOG("[hid]: polling thread is starting\n");

   while(!hid->polling_thread_quit)
   {
      wiiu_handle_attach_events(hid, synchronized_get_events_list());
      wiiu_poll_adapters(hid);
   }

   RARCH_LOG("[hid]: polling thread is stopping\n");
   return 0;
}

static OSThread *new_thread(void)
{
   OSThread *t = alloc_zeroed(8, sizeof(OSThread));

   if (!t)
      return NULL;

   t->tag      = OS_THREAD_TAG;

   return t;
}

static void wiiu_hid_init_lists(void)
{
   RARCH_LOG("[hid]: Initializing events list\n");
   memset(&events, 0, sizeof(events));
   OSFastMutex_Init(&(events.lock), "attach_events");
   RARCH_LOG("[hid]: Initializing adapters list\n");
   memset(&adapters, 0, sizeof(adapters));
   OSFastMutex_Init(&(adapters.lock), "adapters");
}

static wiiu_hid_t *new_hid(void)
{
   RARCH_LOG("[hid]: new_hid()\n");
   return alloc_zeroed(4, sizeof(wiiu_hid_t));
}

static void delete_hid(wiiu_hid_t *hid)
{
   RARCH_LOG("[hid]: delete_hid()\n");
   if (hid)
      free(hid);
}

static HIDClient *new_hidclient(void)
{
   RARCH_LOG("[hid]: new_hidclient()\n");
   return alloc_zeroed(32, sizeof(HIDClient));
}

static void delete_hidclient(HIDClient *client)
{
   RARCH_LOG("[hid]: delete_hidclient()\n");
   if (client)
      free(client);
}

static void init_cachealigned_buffer(int32_t min_size, uint8_t **out_buf_ptr, int32_t *actual_size)
{
   *actual_size = (min_size + 0x3f) & ~0x3f;

   *out_buf_ptr = alloc_zeroed(64, *actual_size);
}

static wiiu_adapter_t *new_adapter(wiiu_attach_event *event)
{
   wiiu_adapter_t *adapter  = alloc_zeroed(32, sizeof(wiiu_adapter_t));

   if (!adapter)
      return NULL;

   adapter->handle          = event->handle;
   adapter->interface_index = event->interface_index;
   init_cachealigned_buffer(event->max_packet_size_rx, &adapter->rx_buffer, &adapter->rx_size);
   init_cachealigned_buffer(event->max_packet_size_tx, &adapter->tx_buffer, &adapter->tx_size);
   adapter->connected       = true;

   return adapter;
}

static void delete_adapter(wiiu_adapter_t *adapter)
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
   if (adapter->driver && adapter->driver_handle) {
      adapter->driver->free(adapter->driver_handle);
      adapter->driver_handle = NULL;
      adapter->driver = NULL;
   }

   free(adapter);
}

static wiiu_attach_event *new_attach_event(HIDDevice *device)
{
   hid_device_t *driver = hid_device_driver_lookup(device->vid, device->pid);
   if (!driver)
   {
      RARCH_ERR("[hid]: Failed to locate driver for device vid=%04x pid=%04x\n",
        device->vid, device->pid);
      return NULL;
   }
   wiiu_attach_event *event = alloc_zeroed(4, sizeof(wiiu_attach_event));
   if (!event)
      return NULL;

   event->driver             = driver;
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

   return event;
}

static void delete_attach_event(wiiu_attach_event *event)
{
   if (event)
      free(event);
}

void *alloc_zeroed(size_t alignment, size_t size)
{
   void *result = memalign(alignment, size);
   if (result)
      memset(result, 0, size);

   return result;
}

hid_driver_t wiiu_hid = {
   wiiu_hid_init,
   wiiu_hid_joypad_query,
   wiiu_hid_free,
   wiiu_hid_joypad_button,
   wiiu_hid_joypad_get_buttons,
   wiiu_hid_joypad_axis,
   wiiu_hid_poll,
   wiiu_hid_joypad_rumble,
   wiiu_hid_joypad_name,
   "wiiu",
   wiiu_hid_send_control,
   wiiu_hid_set_report,
   wiiu_hid_set_idle,
   wiiu_hid_set_protocol,
   wiiu_hid_read,
};
