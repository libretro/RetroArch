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

#include "wiiu_hid.h"

static wiiu_event_list events;
static wiiu_adapter_list adapters;

static bool wiiu_hid_joypad_query(void *data, unsigned slot)
{
   wiiu_hid_t *hid = (wiiu_hid_t *)data;
   if (!hid)
      return false;

   return slot < hid->connections_size;
}

static const char *wiiu_hid_joypad_name(void *data, unsigned slot)
{
   if (!wiiu_hid_joypad_query(data, slot))
      return NULL;

   wiiu_hid_t *hid = (wiiu_hid_t *)data;

   return hid->connections[slot].iface->get_name(data);
}

static void wiiu_hid_joypad_get_buttons(void *data, unsigned port, retro_bits_t *state)
{
   (void)data;
   (void)port;

   BIT256_CLEAR_ALL_PTR(state);
}

static bool wiiu_hid_joypad_button(void *data, unsigned port, uint16_t joykey)
{
   (void)data;
   (void)port;
   (void)joykey;

   return false;
}

static bool wiiu_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)pad;
   (void)effect;
   (void)strength;

   return false;
}

static int16_t wiiu_hid_joypad_axis(void *data, unsigned port, uint32_t joyaxis)
{
   (void)data;
   (void)port;
   (void)joyaxis;

   return 0;
}

static void *wiiu_hid_init(void)
{
   RARCH_LOG("[hid]: wiiu_hid: init\n");
   wiiu_hid_t *hid = new_hid();
   HIDClient *client = new_hidclient();

   if (!hid || !client)
      goto error;

   wiiu_hid_init_lists();
   start_polling_thread(hid);
   if (!hid->polling_thread)
      goto error;

   RARCH_LOG("[hid]: Registering HIDClient\n");
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
   (void)data;
}

static void wiiu_hid_send_control(void *data, uint8_t *buf, size_t size)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   if (!adapter)
      return;

   HIDWrite(adapter->handle, buf, size, NULL, NULL);
}

static int32_t wiiu_hid_set_report(void *data, uint8_t report_type,
               uint8_t report_id, void *report_data, uint32_t report_length)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   if (!adapter)
      return -1;

   return HIDSetReport(adapter->handle,
         report_type,
         report_id,
         report_data,
         report_length,
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

   RARCH_LOG("[hid]: thread: 0x%x; stack: 0x%x\n", thread, stack);

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

   hid->polling_thread_quit = true;
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


static void synchronized_add_event(wiiu_attach_event *event)
{
   OSFastMutex_Lock(&(events.lock));
   event->next = events.list;
   events.list = event;
   OSFastMutex_Unlock(&(events.lock));
}

static wiiu_attach_event *synchronized_get_events_list(void)
{
   wiiu_attach_event *list;
   OSFastMutex_Lock(&(events.lock));
   list = events.list;
   events.list = NULL;
   OSFastMutex_Unlock(&(events.lock));

   return list;
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

   log_device(device);

   switch(attach)
   {
      case HID_DEVICE_ATTACH:
      case HID_DEVICE_DETACH:
         if (device)
            event = new_attach_event(device);

         if(!event)
            goto error;

         event->type = attach;
         synchronized_add_event(event);
         return DEVICE_USED;
      default:
         break;
   }

error:
   delete_attach_event(event);
   return DEVICE_UNUSED;
}

static void wiiu_hid_detach(wiiu_hid_t *hid, wiiu_attach_event *event)
{
}


static void wiiu_hid_attach(wiiu_hid_t *hid, wiiu_attach_event *event)
{
   wiiu_adapter_t *adapter = new_adapter(event);

   if(!adapter)
   {
      RARCH_ERR("[hid]: Failed to allocate adapter.\n");
      goto error;
   }

   adapter->hid    = hid;
   adapter->slot   = pad_connection_pad_init(hid->connections,
         "hid", event->vendor_id, event->product_id, adapter,
         &wiiu_hid);

   if(adapter->slot < 0)
   {
      RARCH_ERR("[hid]: No available slots.\n");
      goto error;
   }

   RARCH_LOG("[hid]: got slot %d\n", adapter->slot);

   if(!pad_connection_has_interface(hid->connections, adapter->slot))
   {
      RARCH_ERR("[hid]: Interface not found for HID device with vid=0x%04x pid=0x%04x\n",
            event->vendor_id, event->product_id);
      goto error;
   }

   RARCH_LOG("[hid]: adding to adapter list\n");
   synchronized_add_to_adapters_list(adapter);

#if 0
   /* this is breaking again. Not sure why. But disabling it now so the
    * startup/shutdown times aren't affected by the blocking call. */
   RARCH_LOG("[hid]: starting read loop\n");
   wiiu_start_read_loop(adapter);
#endif
   return;

error:
   delete_adapter(adapter);
}

void wiiu_start_read_loop(wiiu_adapter_t *adapter)
{
  adapter->state = ADAPTER_STATE_READING;
#if 0
  RARCH_LOG("HIDRead(0x%08x, 0x%08x, %d, 0x%08x, 0x%08x)\n",
	adapter->handle, adapter->rx_buffer, adapter->rx_size,
        wiiu_hid_read_loop_callback, adapter);
#endif
  HIDRead(adapter->handle, adapter->rx_buffer, adapter->rx_size, wiiu_hid_read_loop_callback, adapter);
}

/**
 * Takes a buffer and formats it for the log file, 16 bytes per line.
 *
 * When the end of the buffer is reached, it is padded out with 0xff. So e.g.
 * a 5-byte buffer might look like:
 *
 * 5 bytes read fro HIDRead:
 * 0102030405ffffff  ffffffffffffffff
 * ==================================
 */
static void log_buffer(uint8_t *data, uint32_t len)
{
   int i, o;
   int padding = len % 16;
   uint8_t buf[16];

   (uint8_t *)data;
   (uint32_t)len;

   RARCH_LOG("%d bytes read from HIDRead:\n", len);

   for(i = 0, o = 0; i < len; i++)
   {
      buf[o] = data[i];
      o++;
      if(o == 16)
      {
         o = 0;
         RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x  %02x%02x%02x%02x%02x%02x%02x%02x\n",
               buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
               buf[8], buf[9], buf[10], buf[11], buf[12],  buf[13], buf[14], buf[15]);
      }
   }

   if(padding)
   {
      for(i = padding; i < 16; i++)
         buf[i] = 0xff;

      RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x  %02x%02x%02x%02x%02x%02x%02x%02x\n",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
            buf[8], buf[9], buf[10], buf[11], buf[12],  buf[13], buf[14], buf[15]);
   }
   RARCH_LOG("==================================\n");

}

static void wiiu_hid_do_read(wiiu_adapter_t *adapter,
      uint8_t *data, uint32_t length)
{
 /*  log_buffer(data, length);
  * do_sampling()
  * do other stuff?
  */
}

static void wiiu_hid_read_loop_callback(uint32_t handle, int32_t error,
              uint8_t *buffer, uint32_t buffer_size, void *userdata)
{
  wiiu_adapter_t *adapter = (wiiu_adapter_t *)userdata;
  if(!adapter)
  {
    RARCH_ERR("read_loop_callback: bad userdata\n");
    return;
  }

  if(adapter->hid->polling_thread_quit)
  {
    RARCH_LOG("Shutting down read loop for slot %d\n", adapter->slot);
    adapter->state = ADAPTER_STATE_DONE;
    return;
  }

  wiiu_hid_do_read(adapter, buffer, buffer_size);

  adapter->state = ADAPTER_STATE_READING;
  HIDRead(adapter->handle, adapter->rx_buffer, adapter->rx_size,
      wiiu_hid_read_loop_callback, adapter);
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

   do
   {
      OSFastMutex_Lock(&(adapters.lock));
      incomplete = 0;
      for(adapter = adapters.list; adapter != NULL; adapter = adapter->next)
      {
         if(adapter->state == ADAPTER_STATE_READING)
            incomplete++;
      }
      /* We are clear for shutdown. Clean up the list 
       * while we are holding the lock. */
      if(incomplete == 0)
      {
         while(adapters.list != NULL)
         {
            adapter = adapters.list;
            adapters.list = adapter->next;
            delete_adapter(adapter);
         }
      }
      OSFastMutex_Unlock(&(adapters.lock));

      if(incomplete)
         usleep(5000);

      if(++retries >= 1000)
      {
         RARCH_WARN("[hid]: timed out waiting for in-flight read to finish.\n");
         incomplete = 0;
      }
   }while(incomplete);

   RARCH_LOG("All in-flight reads complete.\n");
}

static void wiiu_handle_attach_events(wiiu_hid_t *hid, wiiu_attach_event *list)
{
   wiiu_attach_event *event, *event_next = NULL;

   if(!hid || !list)
      return;

   for(event = list; event != NULL; event = event_next)
   {
      event_next  = event->next;
      if(event->type == HID_DEVICE_ATTACH)
         wiiu_hid_attach(hid, event);
      else
         wiiu_hid_detach(hid, event);
      delete_attach_event(event);
   }
}

static int wiiu_hid_polling_thread(int argc, const char **argv)
{
   wiiu_hid_t *hid = (wiiu_hid_t *)argv;
   int i           = 0;

   RARCH_LOG("[hid]: polling thread is starting\n");

   while(!hid->polling_thread_quit)
   {
      wiiu_handle_attach_events(hid, synchronized_get_events_list());
      usleep(10000);
      i += 10000;
      if(i >= (1000 * 1000 * 3))
         i = 0;
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
   if(hid)
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
   if(client)
      free(client);
}

static wiiu_adapter_t *new_adapter(wiiu_attach_event *event)
{
   wiiu_adapter_t *adapter  = alloc_zeroed(64, sizeof(wiiu_adapter_t));

   if (!adapter)
      return NULL;

   adapter->handle          = event->handle;
   adapter->interface_index = event->interface_index;
   adapter->rx_size         = event->max_packet_size_rx;
   adapter->rx_buffer       = alloc_zeroed(64, adapter->rx_size);

   return adapter;
}

static void delete_adapter(wiiu_adapter_t *adapter)
{
   if (!adapter)
      return;

   if(adapter->rx_buffer)
   {
      free(adapter->rx_buffer);
      adapter->rx_buffer = NULL;
   }
   free(adapter);
}

static wiiu_attach_event *new_attach_event(HIDDevice *device)
{
   wiiu_attach_event *event = alloc_zeroed(4, sizeof(wiiu_attach_event));
   if(!event)
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

   return event;
}

static void delete_attach_event(wiiu_attach_event *event)
{
   if(event)
      free(event);
}


void *alloc_zeroed(size_t alignment, size_t size)
{
   void *result = memalign(alignment, size);
   if(result)
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
   wiiu_hid_set_protocol
};

